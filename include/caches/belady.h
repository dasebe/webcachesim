//
// Created by zhenyus on 11/8/18.
//

#ifndef WEBCACHESIM_BELADY_H
#define WEBCACHESIM_BELADY_H

#include "cache.h"
#include <boost/bimap.hpp>
#include <boost/bimap/multiset_of.hpp>
#include <utils.h>
#include <unordered_map>

using namespace boost;
using namespace std;

/*
  Belady: Optimal for unit size
*/
class BeladyCache : public Cache
{
protected:
    // list for recency order
    bimap<bimaps::set_of<uint64_t>, bimaps::multiset_of<uint64_t, std::greater<uint64_t>>> _cacheMap;
    unordered_map<uint64_t, uint64_t> object_size;

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
