//
// Created by zhenyus on 11/19/19.
//

#ifndef WEBCACHESIM_STATIC_PARALLEL_CACHE_H
#define WEBCACHESIM_STATIC_PARALLEL_CACHE_H

#include "parallel_cache.h"
using namespace webcachesim;
using namespace std;


class ParallelStaticCache : public ParallelCache {
public:

    void async_lookup(const uint64_t &key) override {};

    virtual void async_admit(
            const uint64_t &key, const int64_t &size, const uint16_t extra_features[max_n_extra_feature]) override {};

    ~ParallelStaticCache() override {
        keep_running = false;
        if (lookup_get_thread.joinable())
            lookup_get_thread.join();
    }

    void parallel_admit (
            const uint64_t &key,
            const int64_t &size,
            const uint16_t extra_features[max_n_extra_feature]) override {
    }

    uint64_t parallel_lookup(const uint64_t &key) override {
        //give it 33% miss rate, according to GBDT hit rate from wiki
        if (!(key%4))
            return 0;
        //average size is 33k
        return 33000;
    }
};

static Factory<ParallelStaticCache> factoryStatic("ParallelStatic");




#endif //WEBCACHESIM_STATIC_PARALLEL_CACHE_H
