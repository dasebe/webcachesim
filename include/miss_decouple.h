//
// Created by zhenyus on 3/10/19.
//

#ifndef WEBCACHESIM_MISS_DECOUPLE_H
#define WEBCACHESIM_MISS_DECOUPLE_H

#include <cstdint>
#include <unordered_map>
#include <math.h>
#include "nlohmann/json.hpp"

using namespace std;
using json = nlohmann::json;

class MissStatistics {
/*
 * statistics of the misses by different #request
 */
public:
    //format <#requests including current, <#requests after warmup, #hits after warmup>>
    unordered_map<uint32_t, pair<uint32_t, uint32_t>> n_request_hit;
    void update(uint32_t& n_total_request, bool if_hit) {
        auto bucket_id = static_cast<uint32_t >(log2(n_total_request));
        auto it = n_request_hit.find(bucket_id);
        if (it == n_request_hit.end()) {
            n_request_hit.insert({bucket_id, {1, if_hit}});
        } else {
            it->second.first += 1;
            it->second.second += if_hit;
        }
    }

    string dump() {
        return json(n_request_hit).dump();
    }
};


#endif //WEBCACHESIM_MISS_DECOUPLE_H
