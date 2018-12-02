//
// Created by zhenyus on 11/8/18.
//

#ifndef WEBCACHESIM_LFO2_H
#define WEBCACHESIM_LFO2_H

#include "cache.h"
#include <boost/bimap.hpp>
#include <boost/bimap/multiset_of.hpp>
#include <unordered_map>

using namespace std;


class LFOACache : public Cache
{
public:
    // list for recency order
    boost::bimap<boost::bimaps::set_of<uint64_t>, boost::bimaps::multiset_of<uint64_t, std::greater<uint64_t>>> _cacheMap;
    unordered_map<uint64_t, uint64_t> object_size;


    LFOACache()
            : Cache()
    {
    }

    virtual bool lookup(SimpleRequest& req);
    virtual void admit(SimpleRequest& req);
    virtual void evict(SimpleRequest& req) {
        //no need to use it
    };
    virtual void evict();

    void train() {};

};

static Factory<LFOACache> factoryLFOA("LFOA");


class LFOBCache : public Cache
{
protected:

public:
    LFOBCache()
            : Cache()
    {
    }

    virtual bool lookup(SimpleRequest& req);
    virtual void admit(SimpleRequest& req);
    virtual void evict(SimpleRequest& req) {
        //no need to use it
    };
    virtual void evict();
};

static Factory<LFOBCache> factoryLFOB("LFOB");

#endif //WEBCACHESIM_LFO2_H
