//
// Created by zhenyus on 11/8/18.
//

#ifndef WEBCACHESIM_RANDOM_VARIANTS_H
#define WEBCACHESIM_RANDOM_VARIANTS_H

#include "cache.h"
#include "cache_object.h"
#include "pickset.h"
#include <unordered_map>
#include <list>
#include <cmath>
#include <boost/bimap.hpp>
#include <boost/bimap/multiset_of.hpp>

const uint8_t max_n_past_intervals = 4;


using namespace std;

class RandomCache : public Cache
{
public:
    PickSet<uint64_t> key_space;
    unordered_map<uint64_t, uint64_t> object_size;

    RandomCache()
        : Cache()
    {
    }
    virtual ~RandomCache()
    {
    }

    virtual bool lookup(SimpleRequest& req);
    virtual void admit(SimpleRequest& req);
    virtual void evict();
    virtual void evict(SimpleRequest& req) {
        //no need to use it
    };
};

static Factory<RandomCache> factoryRandom("Random");


class Meta {
public:
    static uint8_t _n_past_intervals;
    uint64_t _key;
    uint64_t _size;
    uint64_t _future_timestamp;
    uint8_t _past_timestamp_idx;
    uint64_t _past_timestamps[max_n_past_intervals];

    Meta(const uint64_t & key, const uint64_t & size, const uint64_t & past_timestamp, const uint64_t & future_timestamp) {
        _key = key;
        _size = size;
//        _past_timestamps = new uint64_t[_n_past_intervals];
        _past_timestamps[0] = past_timestamp;
        _future_timestamp = future_timestamp;
        _past_timestamp_idx = _past_timestamp_idx + (uint8_t) 1;
    }

//    ~Meta() {
//        delete []_past_timestamps;
//    }
// todo: custom assign function

    inline void append_past_timestamp(const uint64_t & past_timestamp) {
        _past_timestamps[_past_timestamp_idx%_n_past_intervals] = past_timestamp;
        _past_timestamp_idx = _past_timestamp_idx + (uint8_t) 1;
        //todo: can use bit-wise
        // prevent overflow
        if (_past_timestamp_idx >= _n_past_intervals * 2)
            _past_timestamp_idx -= _n_past_intervals;
    }
};


class LRCache : public Cache
{
public:
    // from id to intervals
//    std::unordered_map<std::pair<uint64_t, uint64_t >, std::list<uint64_t> > past_timestamps;
//    boost::bimap<boost::bimaps::set_of<KeyT>, boost::bimaps::multiset_of<uint64_t>> future_timestamp;
    map<uint64_t, set<uint64_t>> gc_timestamp;
    //key -> (0/1 list, idx)
    unordered_map<uint64_t, pair<bool, uint32_t>> key_map;
    vector<Meta> meta_holder[2];

//    std::unordered_map<KeyT, uint64_t> unordered_future_timestamp;
    // sample_size
    uint8_t sample_rate;
    // threshold
    uint64_t threshold;
    double log1p_threshold;
    // batch_size
    uint64_t batch_size;
    // learning_rate
    double learning_rate;
    // n_past_interval
    uint8_t n_past_intervals;

    double * weights;
    double bias = 0;
    double mean_diff=0;
    std::unordered_map<uint64_t, double *> pending_gradients;

    //todo: seed and generator
    default_random_engine _generator = default_random_engine();
    uniform_int_distribution<std::size_t> _distribution = uniform_int_distribution<std::size_t>();

    LRCache()
        : Cache()
    {
    }
    virtual ~LRCache()
    {
        delete []weights;
    }

    virtual void setPar(std::string parName, std::string parValue) {
        if (parName == "sample_rate")
            sample_rate = stoull(parValue);
        else if(parName == "threshold") {
            threshold = stoull(parValue);
            log1p_threshold = std::log1p(threshold);
        }
        else if(parName == "batch_size")
            batch_size = stoull(parValue);
        else if(parName == "learning_rate")
            learning_rate = stod(parValue);
        else if(parName == "n_past_intervals") {
            n_past_intervals = (uint8_t) stoi(parValue);
            if (n_past_intervals > max_n_past_intervals) {
                cerr << "n_past_intervals exceeds max limitation: "<<max_n_past_intervals<<endl;
                return;
            }
            weights = new double[n_past_intervals];
            for (int i = 0; i < n_past_intervals; i++) {
                weights[i] = 0;
            }
            Meta::_n_past_intervals = n_past_intervals;
        }
        else {
       std::cerr << "unrecognized parameter: " << parName << std::endl;
   }
}
    virtual bool lookup(SimpleRequest& req);
    virtual void admit(SimpleRequest& req);
    virtual void evict(const uint64_t & t);
    void evict(SimpleRequest & req) {};
    void evict() {};
    //sample, rank the 1st and return
    pair<uint64_t, uint32_t > rank(const uint64_t & t);
//    void try_train(double * gradients);
//    void try_gc(uint64_t t);
};

static Factory<LRCache> factoryLR("LR");


class BeladySampleCache : public RandomCache
{
public:
    unordered_map<uint64_t, uint64_t > future_timestamp;
    // sample_size
    uint64_t sample_rate=32;
    // threshold
    uint64_t threshold=1000000;
    double log1p_threshold=log1p(threshold);

    BeladySampleCache()
        : RandomCache()
    {
    }

    virtual void setPar(std::string parName, std::string parValue) {
        if (parName == "sample_rate")
            sample_rate = stoull(parValue);
        else if(parName == "threshold") {
            threshold = stoull(parValue);
            log1p_threshold = std::log1p(threshold);
        }
        else {
       std::cerr << "unrecognized parameter: " << parName << std::endl;
   }
}
    virtual bool lookup(SimpleRequest& req);
    virtual void admit(SimpleRequest& req);
    virtual void evict();
};

static Factory<BeladySampleCache> factoryBeladySample("BeladySample");

#endif //WEBCACHESIM_RANDOM_VARIANTS_H
