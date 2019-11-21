//
// Created by zhenyus on 11/2/19.
//

#include "parallel_fifo.h"

void ParallelFIFOCache::async_lookup(const uint64_t &key) {
}

void ParallelFIFOCache::async_admit(const uint64_t &key, const int64_t &size,
                                    const uint16_t extra_features[max_n_extra_feature]) {
    auto it = cache_map.find(key);
    if (it == cache_map.end()) {
        cache_list.push_front(key);
        cache_map[key] = cache_list.begin();
        _currentSize += size;
        //slow to insert before local metadata, because in the future there will be additional fetch step
        auto shard_id = key%n_shard;
        size_map_mutex[shard_id].lock();
        size_map[shard_id].insert({key, size});
        size_map_mutex[shard_id].unlock();
    } else {
        //already in the cache
        goto Lnoop;
    }

    // check more eviction needed?
    while (_currentSize > _cacheSize) {
        evict();
    }
    //no logical op is performed
    Lnoop:
    return;
}

void ParallelFIFOCache::evict() {
    auto lit = --(cache_list.end());
    uint64_t key = *lit;
    //fast to remove before local metadata, because in the future will have async admission
    auto shard_id = key%n_shard;
    auto sit = size_map[shard_id].find(key);
    uint64_t size = sit->second;
    size_map_mutex[shard_id].lock();
    size_map[shard_id].erase(key);
    size_map_mutex[shard_id].unlock();
    _currentSize -= size;
    cache_map.erase(key);
    cache_list.erase(lit);
}