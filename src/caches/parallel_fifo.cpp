//
// Created by zhenyus on 11/2/19.
//

#include "parallel_fifo.h"

bool ParallelFIFOCache::lookup(SimpleRequest &req) {
    static int counter = 0;
    if (!((counter++) % 1000000)) {
        op_queue_mutex.lock();
        auto s = op_queue.size();
        op_queue_mutex.unlock();
        std::cerr << "op queue length: " << s << std::endl;
    }
    //back pressure
    if (counter % 10000) {
        while (true) {
            op_queue_mutex.lock();
            if (op_queue.size() > 1000) {
                op_queue_mutex.unlock();
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            } else {
                op_queue_mutex.unlock();
                break;
            }
        }
    }
    return parallel_lookup(req._id);
}

void ParallelFIFOCache::admit(SimpleRequest &req) {
    int64_t size = req._size;
    parallel_admit(req._id, size, req._extra_features.data());
}

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
        size_map_mutex.lock();
        size_map.insert({key, size});
        size_map_mutex.unlock();
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
    size_map_mutex.lock();
    auto sit = size_map.find(key);
    uint64_t size = sit->second;
    size_map.erase(key);
    size_map_mutex.unlock();
    _currentSize -= size;
    cache_map.erase(key);
    cache_list.erase(lit);
}