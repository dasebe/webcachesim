//
// Created by zhenyus on 11/2/19.
//

#ifndef WEBCACHESIM_PARALLEL_FIFO_H
#define WEBCACHESIM_PARALLEL_FIFO_H

#include "parallel_cache.h"
#include <list>
/*
    ParallelFIFO: allowing get/put concurrently. Internally it is still sequential
*/
using namespace webcachesim;
using namespace std;


typedef std::list<uint64_t>::iterator ListIteratorType;
typedef std::unordered_map<uint64_t, ListIteratorType> lruCacheMapType;

class ParallelFIFOCache : public ParallelCache {
public:
    void async_lookup(const uint64_t &key) override;

    void
    async_admit(const uint64_t &key, const int64_t &size, const uint16_t extra_features[max_n_extra_feature]) override;

    void evict();

    ~ParallelFIFOCache() override {
        keep_running = false;
        if (lookup_get_thread.joinable())
            lookup_get_thread.join();
    }

private:
    // list for recency order
    list<uint64_t> cache_list;
    // map to find objects in list
    lruCacheMapType cache_map;
};

static Factory<ParallelFIFOCache> factoryParallelFIFO("ParallelFIFO");
#endif //WEBCACHESIM_PARALLEL_FIFO_H
