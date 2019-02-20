//
// Created by zhenyus on 11/8/18.
//

#ifndef WEBCACHESIM_BELADY_H
#define WEBCACHESIM_BELADY_H

#include "cache.h"
#include <utils.h>
#include <unordered_map>
#include <map>

using namespace std;

/*
  Belady: Optimal for unit size
*/
class BeladyCache : public Cache
{
protected:
    // list for recency order
    multimap<uint64_t , uint64_t, greater<uint64_t >> _valueMap;
    // only store in-cache object, value is size
    unordered_map<uint64_t, uint64_t> _cacheMap;

public:
    BeladyCache()
            : Cache()
    {
    }
    virtual ~BeladyCache()
    {
    }

    virtual bool lookup(SimpleRequest& req);
    virtual void admit(SimpleRequest& req);
    virtual void evict(SimpleRequest& req) {
        //no need to use it
    };
    virtual void evict();
};

static Factory<BeladyCache> factoryBelady("Belady");


#endif //WEBCACHESIM_BELADY_H
