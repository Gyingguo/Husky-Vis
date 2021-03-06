#pragma once

#include <iostream>

#include <string>
#include <vector>
#include <algorithm>
#include <utility>

#include "mongo/bson/bson.h"
#include "mongo/client/dbclient.h"

#include "boost/property_tree/ptree.hpp"
#include "husky/core/engine.hpp"
#include "husky/io/input/inputformat_store.hpp"
#include "husky/lib/aggregator_factory.hpp"

#include "preprocess.hpp"
#include "core/common/base_obj.hpp"
#include "core/common/constant.hpp"
#include "core/dataloader/dataloader.hpp"

#include "core/channel/generate_channel.hpp"
#include "core/channel/chart_type_channel.hpp"
#include "core/channel/aggregate_channel.hpp"
#include "core/channel/statistic_channel.hpp"
#include "core/channel/process_rawdata_channel.hpp"
#include "core/channel/process_aggregatedata_channel.hpp"

using namespace std;
using boost::property_tree::ptree;
using boost::property_tree::ptree_error;

namespace husky {
namespace visualization {

class JsonItem {
public:
    using KeyT = std::string;

    JsonItem() = default;
    explicit JsonItem(const KeyT& w) : json_item(w) {}
    const KeyT& id() const { return json_item; }

    KeyT json_item;
};

class Controller {
public:
    static void init_with_args(int argc, char** argv, std::vector<std::string> args) {
        husky::init_with_args(argc, argv, args);
    }

    static void init_visualization(std::vector<husky::visualization::SuggestionObject>& topk_suggestions, const std::string& select_attribute = "") {
        husky::LOG_I << "init_visualization";
        // distributed suggestions
        const std::string& distributed = husky::Context::get_param("distribute");
        husky::visualization::Constant constant;
        constant.init_constant(husky::Context::get_param("constant"));

        std::vector<husky::visualization::SuggestionObject> suggestions;
        go_nodata_channels(suggestions, select_attribute);

        std::vector<husky::visualization::SuggestionObject> all_calculated_suggestions;

        if (distributed == "suggestions") {
            husky::visualization::DataLoader dataloader;

            dataloader.load_data();
            ptree data = dataloader.get_data();

            // default strategy: suggestions loaded balance in each worker and each thread
            // in this situation, each machine each thread accesses the whole data
            // load balance according to thread
            int total_workers = husky::Context::get_num_workers();
            int items_per_worker = suggestions.size() / total_workers;

            // check if global_tid start with 0 or 1, assume start with 0
            int global_tid = husky::Context::get_global_tid();

            int start = items_per_worker * global_tid;
            // make sure all items will be processed
            int end = global_tid == total_workers - 1 ? suggestions.size() : start + items_per_worker;

            // each thread gets its own item part
            std::vector<husky::visualization::SuggestionObject> items_part(end - start);
            std::copy(suggestions.begin() + start, suggestions.begin() + end, items_part.begin());
            // go through process rawdata channel
            husky::visualization::ProcessRawDataChannel process_rawdata_channel;
            process_rawdata_channel.process_rawdata_suggestions(items_part, data);
            std::vector<husky::visualization::SuggestionObject> process_r_suggestions =
                process_rawdata_channel.get_rawdata_suggestions();

            // go through process aggregatedata channel
            // process aggregratedata channel
            husky::visualization::ProcessAggregateDataChannel process_aggregatedata_channel;
            process_aggregatedata_channel.process_aggregatedata_suggestions(process_r_suggestions);
            std::vector<husky::visualization::SuggestionObject> process_a_suggestions =
                process_aggregatedata_channel.get_aggregatedata_suggestions();

            // calculate scores
            for (int i = 0; i < process_a_suggestions.size(); i++) {
                husky::visualization::SuggestionObject suggestion_with_score =
                    husky::visualization::Preprocess::calculate_scores(process_a_suggestions[i], constant);

                all_calculated_suggestions.push_back(suggestion_with_score);
            }
        } else if (distributed == "data") {
            husky::LOG_I << "init_visualization distributed data";
            // override
            auto& infmt = husky::io::InputFormatStore::create_mongodb_inputformat();
            infmt.set_server(husky::Context::get_param("mongo_server"));
            infmt.set_ns(husky::Context::get_param("mongo_db"), husky::Context::get_param("mongo_collection"));
            infmt.set_query("");

            auto& json_item_list = husky::ObjListStore::create_objlist<JsonItem>();
            auto& ch = husky::ChannelStore::create_push_combined_channel<int, husky::SumCombiner<int>>(infmt, json_item_list);
      
            auto parse_item = [&](std::string & chunk) {
                ch.push(1, chunk);
            };

            husky::load(infmt, parse_item);

            // aggregate sum
            husky::lib::Aggregator<std::map<std::string, double>> sum(
                    std::map<std::string, double>(),
                    [](std::map<std::string, double>& a, const std::map<std::string, double>& b) {
                for (std::map<std::string, double>::const_iterator b_it = b.begin(); b_it != b.end(); ++b_it) {
                    auto it = a.find(b_it->first);
                    if (it != a.end()) {
                        a[b_it->first] += b_it->second;
                    } else {
                        a.insert(*b_it);
                    }
                    // husky::LOG_I << "global sum: " + std::to_string(a[b_it->first]);
                }   
            });
            sum.to_reset_each_iter();

            // aggregate variance_mean_num
            husky::lib::Aggregator<std::map<std::string, husky::VarianceMeanNum>> variance_mean_num(
                    std::map<std::string, husky::VarianceMeanNum>(),
                    [](std::map<std::string, husky::VarianceMeanNum>& a, const std::map<std::string, husky::VarianceMeanNum>& b) {
                for (std::map<std::string, husky::VarianceMeanNum>::const_iterator b_it = b.begin(); b_it != b.end(); ++b_it) {
                    auto it = a.find(b_it->first);
                    if (it != a.end()) {
                        a[b_it->first] += b_it->second;
                    } else {
                        a.insert(*b_it);
                    }
                }
            });
            variance_mean_num.to_reset_each_iter();

            // aggregate max
            husky::lib::Aggregator<std::map<std::string, double>> max(
                    std::map<std::string, double>(),
                    [](std::map<std::string, double>& a, const std::map<std::string, double>& b) {
                for (std::map<std::string, double>::const_iterator b_it = b.begin(); b_it != b.end(); ++b_it) {
                    auto it = a.find(b_it->first);
                    if (it != a.end()) {
                        a[b_it->first] = a[b_it->first] > b_it->second ? a[b_it->first] : b_it->second;
                    } else {
                        a.insert(*b_it);
                    }
                }
            });
            max.to_reset_each_iter();

            // aggregate min
            husky::lib::Aggregator<std::map<std::string, double>> min(
                    std::map<std::string, double>(),
                    [](std::map<std::string, double>& a, const std::map<std::string, double>& b) {
                for (std::map<std::string, double>::const_iterator b_it = b.begin(); b_it != b.end(); ++b_it) {
                    auto it = a.find(b_it->first);
                    if (it != a.end()) {
                        a[b_it->first] = a[b_it->first] < b_it->second ? a[b_it->first] : b_it->second;
                    } else {
                        a.insert(*b_it);
                    }
                }
            });
            min.to_reset_each_iter();

            for (int i = 0; i < suggestions.size(); i++) {
                string& aggregate_type = suggestions[i].key.aggregate_type;
                
                husky::list_execute(json_item_list, [&](JsonItem & item) {
                    mongo::BSONObj o = mongo::fromjson(item.id());

                    // aggregate
                    // measure is Q, dimension is N
                    std::string& measure = suggestions[i].key.measure;
                    std::string& dimension = suggestions[i].key.dimension;
                    double measure_value;
                    try {
                        measure_value = o.getField(measure).Number();
                    } catch (mongo::MsgAssertionException&) {
                        measure_value = 0;
                    }
                    std::string dimension_value = o.getStringField(dimension);
                    
                    std::pair<std::string, double> current_item;
                    current_item = std::make_pair(dimension_value, measure_value);
          
                    if (aggregate_type == "SUM") {
                        sum.update([&](std::map<std::string, double>& x, const std::pair<std::string, double>& y) {
                            x[y.first] += y.second;
                        }, current_item);
                    } else if (aggregate_type == "MEAN") {
                        variance_mean_num.update([&](std::map<std::string, husky::VarianceMeanNum>& x,
                                const std::pair<std::string, double>& y) {
                            x[y.first] += y.second;
                        }, current_item);
                    } else if (aggregate_type == "MAX") {
                        max.update([&](std::map<std::string, double>& x, const std::pair<std::string, double>& y) {
                            auto it = x.find(y.first);
                            if (it != x.end()) {
                                x[y.first] = x[y.first] > y.second ? x[y.first] : y.second;
                            } else {
                                x.insert(y);
                            }
                        }, current_item);
                    } else if (aggregate_type == "MIN") {
                        min.update([&](std::map<std::string, double>& x, const std::pair<std::string, double>& y) {
                            auto it = x.find(y.first);
                            if (it != x.end()) {
                                x[y.first] = x[y.first] < y.second ? x[y.first] : y.second;
                            } else {
                                x.insert(y);
                            }
                        }, current_item);
                    } else if (aggregate_type == "VARIANCE") {
                        variance_mean_num.update([&](std::map<std::string, husky::VarianceMeanNum>& x,
                                const std::pair<std::string, double>& y) {
                            x[y.first] += y.second;
                        }, current_item);
                    }
	        });
                
                husky::lib::AggregatorFactory::sync();

                // get the aggregate result
                if (aggregate_type == "SUM") {
                    suggestions[i].aggregate_data = sum.get_value();
                } else if (aggregate_type == "MEAN") {
                    std::map<std::string, husky::VarianceMeanNum>& variance_data = variance_mean_num.get_value();
                    for (std::map<std::string, husky::VarianceMeanNum>::iterator it = variance_data.begin();
                            it != variance_data.end(); it++) {
                        suggestions[i].aggregate_data.emplace(it->first, it->second.get_mean());
                    }
                } else if (aggregate_type == "MAX") {
                    suggestions[i].aggregate_data = max.get_value();
                } else if (aggregate_type == "MIN") {
                    suggestions[i].aggregate_data = min.get_value();
                } else if (aggregate_type == "VARIANCE") {
                    std::map<std::string, husky::VarianceMeanNum>& variance_data = variance_mean_num.get_value();
                    for (std::map<std::string, husky::VarianceMeanNum>::iterator it = variance_data.begin();
                            it != variance_data.end(); it++) {
                        suggestions[i].aggregate_data.emplace(it->first, it->second.get_variance());
                    }
                }  

                // calculated score
                husky::visualization::SuggestionObject suggestion_with_score =
                    husky::visualization::Preprocess::calculate_scores(suggestions[i], constant);

                all_calculated_suggestions.push_back(suggestion_with_score);
            }
        }

        // get topk suggestions
        if (husky::Context::get_global_tid() == 0) {
            int topk = std::stoi(husky::Context::get_param("topk"));
            topk_suggestions = husky::visualization::Preprocess::get_topk_suggestions(all_calculated_suggestions, topk);
            husky::LOG_I << "topk: " << topk_suggestions.size();

            // output to check
            for (std::vector<husky::visualization::SuggestionObject>::iterator item = topk_suggestions.begin();
                item != topk_suggestions.end(); item++) {
                cout << *item;
            }
        }
    }

    static void get_attributes(std::vector<std::string>& attributes) {
        // load data
        husky::visualization::DataLoader dataloader;

        dataloader.load_schema();
        ptree data_schema = dataloader.get_data_schema();

        attributes = husky::visualization::Preprocess::collect_attributes(data_schema);
    }

    static void go_nodata_channels(std::vector<husky::visualization::SuggestionObject>& suggestions,
            const std::string & select_attribute) {
        // go through channels except process_rawdata_channel and process_aggregatedata_channel

        // load data
        husky::visualization::DataLoader dataloader;

        dataloader.load_schema();
        ptree data_schema = dataloader.get_data_schema();

        // load constant values
        husky::visualization::Constant constant;
        constant.init_constant(husky::Context::get_param("constant"));

        // preprocess data
        // collect attributes
        std::vector<std::string> attributes = husky::visualization::Preprocess::collect_attributes(data_schema);
        // collect attributes_datatypes
        std::map<std::string, std::string> attribute_type = husky::visualization::Preprocess::collect_schema_type(data_schema);

        // go through generate_channel
        husky::visualization::GenerateChannel generate_channel;
        generate_channel.generate_suggestions(attributes, select_attribute);
        std::vector<husky::visualization::SuggestionObject> g_suggestions = generate_channel.get_generated_suggestions();

        // go through chart_type_channel
        husky::visualization::ChartTypeChannel chart_type_channel;
        chart_type_channel.chart_type_suggestions(g_suggestions, attribute_type, constant);
        std::vector<husky::visualization::SuggestionObject> c_suggestions = chart_type_channel.get_chart_type_suggestions();

        // go through aggregate channel
        husky::visualization::AggregateChannel aggregate_channel;
        aggregate_channel.aggregate_suggestions(c_suggestions, constant);
        std::vector<husky::visualization::SuggestionObject> a_suggestions = aggregate_channel.get_aggregate_suggestions();

        // go through statistic channel
        husky::visualization::StatisticChannel statistic_channel;
        statistic_channel.statistic_suggestions(a_suggestions, constant);
        std::vector<husky::visualization::SuggestionObject> s_suggestions = statistic_channel.get_statistic_suggestions();

        // set
        suggestions = s_suggestions;
    }
};

}
}
