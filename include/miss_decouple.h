//
// Created by zhenyus on 3/10/19.
//

#ifndef WEBCACHESIM_MISS_DECOUPLE_H
#define WEBCACHESIM_MISS_DECOUPLE_H

#include <cstdint>
#include <unordered_map>
#include <math.h>

//set this flag to get statistics of miss decouple
#define MISS_DECOUPLE
//#undef MISS_DECOUPLE

using namespace std;

class MissStatistics {
/*
 * statistics of the misses per
 */
public:
    //format <#requests including current, <#requests after warmup, #hits after warmup>>
    unordered_map<uint32_t, pair<uint32_t, uint32_t>> n_request_hit;
    void update(uint32_t& n_total_request, bool if_hit) {
        auto bucket_id = static_cast<uint32_t >(log2(n_total_request));
        auto it = n_request_hit.find(bucket_id);
        if (it == n_request_hit.end()) {
            n_request_hit.insert({n_total_request, {1, if_hit}});
        } else {
            it->second.first += 1;
            it->second.second += if_hit;
        }
    }

    string yaml_dump() {
        string res = "{";
        int counter = 0;
        for (auto& kv: n_request_hit) {
            if (counter)
                res += ",";
            res += to_string(kv.first)+":["+to_string(kv.second.first)+","+to_string(kv.second.second)+"]";
            ++counter;
        }
        res += "}";
        return res;
    }
};


#endif //WEBCACHESIM_MISS_DECOUPLE_H
