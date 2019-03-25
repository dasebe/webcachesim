//
// Created by zhenyus on 3/21/19.
//

#ifndef WEBCACHESIM_CACHE_SIZE_DECOUPLE_H
#define WEBCACHESIM_CACHE_SIZE_DECOUPLE_H


#include <cstdint>
#include <unordered_map>
#include <math.h>
#include "nlohmann/json.hpp"

using namespace std;
using json = nlohmann::json;

class CacheSizeStatistics {
public:
    //format <<snapshot id, #requests including current>, <n_objs, byte_objs >>
    unordered_map<pair<uint32_t, uint32_t>, pair<uint32_t, uint64_t> > cache_size_map;

    void update(uint32_t & snapshot_id, uint32_t& n_total_request, uint64_t size) {
        auto bucket_id = static_cast<uint32_t >(log2(n_total_request));
        pair<uint32_t, uint32_t > key = make_pair(snapshot_id, bucket_id);
        auto it = cache_size_map.find(key);
        if (it == cache_size_map.end()) {
            cache_size_map.insert({key, {1, size}});
        } else {
            it->second.first += 1;
            it->second.second += size;
        }
    }

    string dump() {
        return json(cache_size_map).dump();
    }
};
#endif //WEBCACHESIM_CACHE_SIZE_DECOUPLE_H
