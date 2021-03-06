#include <iostream>
#include <map>
#include <vector>

#include "process_aggregatedata_channel.hpp"
#include "../common/base_obj.hpp"
#include "../common/utils.hpp"

namespace husky{
namespace visualization {

    /**
     * [groupByDimension description]
     * @param  {[type]} sets
     * [{"measure": "name",
     *   "dimension": "year",
     *   "chartType": "Q_Q_POINT",
     *   "aggregateType": "SUM",
     *   "statisticalMethod": "variance",
     *   "groupByRawData": {"1992": [1, 2, 3], "1993": [1, 2, 3], "1994": [1, 2, 3]}
     *   }
     * ,{"measure": "name",
     *   "dimension": "cylinder",
     *   "chartType": "Q_T_BAR",
     *   "aggregateType": "COUNT",
     *   "statisticalMethod": "variance",
     *   "groupByRawData": {"1992": [1, 2, 3], "1993": [1, 2, 3], "1994": [1, 2, 3]}
     *   }]
     *
     * @param  {[type]} data      [{},{}]
     * @return {[type]}           {"1992": [1, 2, 3], "1993": [1, 2, 3], "1994": [1, 2, 3]}
     *
     * [{"measure": "name",
     *   "dimension": "year",
     *   "chartType": "Q_Q_POINT",
     *   "aggregateType": "SUM",
     *   "statisticalMethod": "variance",
     *   "groupByRawData": {"1992": [1, 2, 3], "1993": [1, 2, 3], "1994": [1, 2, 3]},
     *   "aggregateData": {"1992": 6, "1993": 6, "1994": 6}
     *   }
     * ,{"measure": "name",
     *   "dimension": "cylinder",
     *   "chartType": "Q_T_BAR",
     *   "aggregateType": "COUNT",
     *   "statisticalMethod": "variance",
     *   "groupByRawData": {"1992": [1, 2, 3], "1993": [1, 2, 3], "1994": [1, 2, 3]}
     *   "aggregateData": {"1992": 3, "1993": 3, "1994": 3}
     *   }]
     *
     * */
    void ProcessAggregateDataChannel::process_aggregatedata_suggestions(std::vector<husky::visualization::SuggestionObject> dataset) {
        for (auto set : dataset) {
            for (const auto elem : set.group_by_raw_data) {
                const std::string& key = elem.first;
                const std::vector<double>& value = elem.second;

                const std::string& aggregate_method = set.key.aggregate_type;
                double aggregate_result = (Utils::map_function[aggregate_method])(value);
                set.aggregate_data[key] = aggregate_result;
            }

            suggestions.push_back(set);
        }
    }

    std::vector<husky::visualization::SuggestionObject> ProcessAggregateDataChannel::get_aggregatedata_suggestions() {
        return suggestions;
    }

}
}
