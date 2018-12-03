//
// Created by zhenyus on 11/8/18.
//

#ifndef WEBCACHESIM_LFO2_H
#define WEBCACHESIM_LFO2_H

#include "cache.h"
#include <boost/bimap.hpp>
#include <boost/bimap/multiset_of.hpp>
#include <unordered_map>
#include <list>
#include <LightGBM/c_api.h>
#include <random>

using namespace std;


#define MAX_N_INTERVAL 50

class LFOACache : public Cache
{
public:
    // list for recency order
    boost::bimap<boost::bimaps::set_of<uint64_t>, boost::bimaps::multiset_of<uint64_t, std::greater<uint64_t>>> _cacheMap;
    unordered_map<uint64_t, uint64_t> object_size;

    unordered_map<uint64_t, uint64_t > past_timestamp;
    unordered_map<uint64_t, list<uint64_t > > past_intervals;
    unordered_map<uint64_t, uint64_t > future_timestamp;

    // training info
    vector<float> labels;
    vector<int32_t> indptr = {0};
    vector<int32_t> indices;
    vector<double> data;

    //model
    BoosterHandle booster = nullptr;


    LFOACache()
            : Cache()
    {

    }

    virtual bool lookup(SimpleRequest& req);
    virtual void admit(SimpleRequest& req);
    virtual void evict(SimpleRequest& req) {
        //no need to use it
    };
    virtual void evict() {};
    void evict(uint64_t & t);

    void train();

};

static Factory<LFOACache> factoryLFOA("LFOA");


class Meta {
public:
    //todo: currently set as a constant
    //idx should increase 0 -> max, and then periodicaly max -> 2 * max. This prevent ambiguity of 0 intervals
    // and max intervals
//    static uint8_t _n_past_intervals;
    uint64_t _key;
    uint64_t _size;
    uint64_t _future_timestamp;
    uint64_t _past_timestamp;
    uint8_t _past_interval_idx;
    uint64_t _past_intervals[MAX_N_INTERVAL];

    Meta(const uint64_t & key, const uint64_t & size, const uint64_t & past_timestamp, const uint64_t & future_timestamp) {
        _key = key;
        _size = size;
        _past_timestamp = past_timestamp;
        _future_timestamp = future_timestamp;
        _past_interval_idx = (uint8_t) 0;
    }

    Meta(const uint64_t & key, const uint64_t & size, const uint64_t & past_timestamp,
            const uint64_t & future_timestamp, const list<uint64_t > & past_intervals) {
        _key = key;
        _size = size;
        _past_timestamp = past_timestamp;
        _future_timestamp = future_timestamp;
        uint8_t i = 0;
        for (auto interval_it = past_intervals.rbegin(); interval_it != past_intervals.rend(); ++interval_it) {
            _past_intervals[i] = *interval_it;
            ++i;
        }
        _past_interval_idx += i;
    }

//    ~Meta() {
//        delete []_past_timestamps;
//    }

    void update(const uint64_t & past_timestamp, const uint64_t & future_timestamp) {
        _past_intervals[_past_interval_idx%MAX_N_INTERVAL] = past_timestamp - _past_timestamp;
        _past_interval_idx += 1;
        //todo: can use bit-wise
        // prevent overflow
        if (_past_interval_idx >= MAX_N_INTERVAL * 2)
            _past_interval_idx -= MAX_N_INTERVAL;
        _past_timestamp = past_timestamp;
    }
};



class LFOBCache : public Cache
{
public:
    //key -> (0/1 list, idx)
    unordered_map<uint64_t, pair<bool, uint32_t>> key_map;
    vector<Meta> meta_holder[2];

    // sample_size
    uint8_t sample_rate = 32;
    //todo: seed and generator
    default_random_engine _generator = default_random_engine();
    uniform_int_distribution<std::size_t> _distribution = uniform_int_distribution<std::size_t>();



    BoosterHandle booster = nullptr;
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
