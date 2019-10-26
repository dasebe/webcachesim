//
// Created by zhenyus on 3/10/19.
//

#ifndef WEBCACHESIM_MISS_DECOUPLE_H
#define WEBCACHESIM_MISS_DECOUPLE_H

#ifdef MISS_DECOUPLE
#include <cstdint>
#include <unordered_map>
#include <math.h>
#include "nlohmann/json.hpp"
#include <tuple>

using namespace std;
using json = nlohmann::json;

class MissStatistics {
/*
 * statistics of the misses by different #request
 */
public:
    //format <#requests including current, <n_req, byte_req, n_hit, byte_hit> after warmup>
    unordered_map<uint32_t, tuple<uint32_t, uint64_t, uint32_t, uint64_t>> n_request_hit;
    void update(uint32_t& n_total_request, uint64_t & size, bool if_hit) {
        auto bucket_id = static_cast<uint32_t >(log2(n_total_request));
        auto it = n_request_hit.find(bucket_id);
        if (it == n_request_hit.end()) {
            n_request_hit.insert({bucket_id, make_tuple(1, size, static_cast<uint32_t >(if_hit), if_hit*size)});
        } else {
            get<0>(it->second) += 1;
            get<1>(it->second) += size;
            get<2>(it->second) += if_hit;
            get<3>(it->second) += if_hit*size;
        }
    }

    string dump() {
        return json(n_request_hit).dump();
    }
};

#endif
#endif //WEBCACHESIM_MISS_DECOUPLE_H
