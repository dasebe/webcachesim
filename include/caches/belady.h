//
// Created by zhenyus on 11/8/18.
//

#ifndef WEBCACHESIM_BELADY_H
#define WEBCACHESIM_BELADY_H

#include "cache.h"
#include <boost/bimap.hpp>
#include <boost/bimap/multiset_of.hpp>

typedef std::pair<std::uint64_t, std::uint64_t> KeyT;
typedef boost::bimap<boost::bimaps::set_of<KeyT>,
        boost::bimaps::multiset_of<uint64_t, std::greater<uint64_t>>> Bimap;

typedef Bimap::left_map::const_iterator left_const_iterator;
typedef Bimap::right_map::const_iterator right_const_iterator;

typedef Bimap::left_map::const_iterator left_iterator;
typedef Bimap::right_map::const_iterator right_iterator;

/*
  Belady: Optimal for unit size
*/
class BeladyCache : public Cache
{
protected:
    // list for recency order
    Bimap _cacheMap;

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
