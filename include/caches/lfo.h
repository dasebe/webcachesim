//
// Created by zhenyus on 11/8/18.
//

#ifndef WEBCACHESIM_LFO_H
#define WEBCACHESIM_LFO_H

#include "cache.h"
#include <boost/bimap.hpp>
#include <boost/bimap/multiset_of.hpp>

typedef std::pair<std::uint64_t, double > KeyT;
typedef boost::bimap<boost::bimaps::set_of<KeyT>,
        boost::bimaps::multiset_of<double >> Bimap;

typedef Bimap::left_map::const_iterator left_const_iterator;
typedef Bimap::right_map::const_iterator right_const_iterator;

typedef Bimap::left_map::const_iterator left_iterator;
typedef Bimap::right_map::const_iterator right_iterator;

class LFOCache : public Cache
{
protected:
    // list for recency order
    Bimap _cacheMap;

public:
    LFOCache()
            : Cache()
    {
    }
    virtual ~LFOCache()
    {
    }

    virtual bool lookup(SimpleRequest& req);
    virtual void admit(SimpleRequest& req);
    virtual void evict(SimpleRequest& req) {
        //no need to use it
    };
    virtual void evict();
};

static Factory<LFOCache> factoryBelady("LFO");

#endif //WEBCACHESIM_LFO_H
