//
// Created by zhenyus on 11/2/19.
//

#ifndef WEBCACHESIM_PARALLEL_LRU_H
#define WEBCACHESIM_PARALLEL_LRU_H

#include "parallel_cache.h"
#include <list>
#include <bloom_filter.h>
/*
    ParallelLRU: allowing get/put concurrently. Internally it is still sequential
*/
using namespace webcachesim;
using namespace std;


typedef std::list<uint64_t>::iterator ListIteratorType;
typedef std::unordered_map<uint64_t, ListIteratorType> lruCacheMapType;

class ParallelLRUCache : public ParallelCache {
public:
    void async_lookup(const uint64_t &key) override;

    void
    async_admit(const uint64_t &key, const int64_t &size, const uint16_t extra_features[max_n_extra_feature]) override;

    void evict();

    ~ParallelLRUCache() override {
        keep_running = false;
        if (lookup_get_thread.joinable())
            lookup_get_thread.join();
    }
private:
    AkamaiBloomFilter filter;
    // list for recency order
    list<uint64_t> cache_list;
    // map to find objects in list
    lruCacheMapType cache_map;

    void hit(lruCacheMapType::const_iterator it);
};

static Factory<ParallelLRUCache> factoryLRU("ParallelLRU");
#endif //WEBCACHESIM_PARALLEL_LRU_H
