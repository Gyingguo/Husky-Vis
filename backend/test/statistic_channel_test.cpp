#include <iostream>
#include <vector>
#include <map>

#include "core/channel/generate_channel.hpp"
#include "core/channel/chart_type_channel.hpp"
#include "core/channel/aggregate_channel.hpp"
#include "core/channel/statistic_channel.hpp"
#include "core/common/base_obj.hpp"
#include "core/common/constant.hpp"

using namespace std;

int main () {
    // constant
    husky::visualization::Constant constant_test;
    constant_test.init_constant("/data/yuying/project/Husky-Vis/backend/core/common/constant.json");

    // generate_channel
    std::vector<std::string> generate_dataset;
    generate_dataset.push_back("name");
    generate_dataset.push_back("year");
    generate_dataset.push_back("cylinder");

    husky::visualization::GenerateChannel generate_channel_test;
    generate_channel_test.generate_suggestions(generate_dataset, "name");

    std::vector<husky::visualization::SuggestionObject> suggestions = generate_channel_test.get_generated_suggestions();

    std::map<std::string, std::string> schema;
    schema.emplace("name", "N");
    schema.emplace("year", "T");
    schema.emplace("cylinder", "Q");

    // chart_type_channel
    husky::visualization::ChartTypeChannel chart_type_channel_test;
    chart_type_channel_test.chart_type_suggestions(suggestions, schema, constant_test);
    std::vector<husky::visualization::SuggestionObject> chart_suggestions = chart_type_channel_test.get_chart_type_suggestions();

    // aggregate channel
    husky::visualization::AggregateChannel aggregate_channel_test;
    aggregate_channel_test.aggregate_suggestions(chart_suggestions, constant_test);
    std::vector<husky::visualization::SuggestionObject> aggregate_suggestions = aggregate_channel_test.get_aggregate_suggestions();

    // statistic channel
    husky::visualization::StatisticChannel statistic_channel_test;
    statistic_channel_test.statistic_suggestions(aggregate_suggestions, constant_test);
    std::vector<husky::visualization::SuggestionObject> statistic_suggestions = statistic_channel_test.get_statistic_suggestions();

    // output
    for (std::vector<husky::visualization::SuggestionObject>::iterator item = statistic_suggestions.begin(); item != statistic_suggestions.end(); item++) {
        cout << *item;
    }

}
