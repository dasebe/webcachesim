//
// Created by zhenyus on 11/18/18.
//

#ifndef WEBCACHESIM_UCB_H
#define WEBCACHESIM_UCB_H

#include <unordered_set>
#include <unordered_map>
#include <list>
#include "cache.h"
#include <boost/bimap.hpp>
#include <boost/bimap/multiset_of.hpp>

typedef uint64_t KeyT;

using namespace std;
using namespace webcachesim;

class UCBCache : public Cache
{
public:
    // from id to intervals
    boost::bimap<boost::bimaps::set_of<KeyT>, boost::bimaps::multiset_of<double , std::greater<double >>> mlcache_score;
    std::unordered_map<KeyT, uint64_t> mlcache_plays;
    unordered_map<uint64_t, uint64_t> size_map;
    uint64_t t = 0;

    UCBCache()
            : Cache()
    {
    }
    virtual ~UCBCache()
    {
    }

    virtual bool lookup(SimpleRequest& req);
    virtual void admit(SimpleRequest& req);

    void evict();
};

static Factory<UCBCache> factoryUCB("UCB");


#endif //WEBCACHESIM_UCB_H
