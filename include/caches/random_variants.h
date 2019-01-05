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


class LRMeta {
public:
    static uint8_t _n_past_intervals;
    uint64_t _key;
    uint64_t _size;
    uint64_t _future_timestamp;
    uint8_t _past_timestamp_idx;
    vector<uint64_t> _past_timestamps;

    LRMeta(const uint64_t & key, const uint64_t & size, const uint64_t & past_timestamp,
            const uint64_t & future_timestamp) {
        _key = key;
        _size = size;
        _past_timestamps = vector<uint64_t >(_n_past_intervals);
        _past_timestamps[0] = past_timestamp;
        _future_timestamp = future_timestamp;
        _past_timestamp_idx = (uint8_t) 1;
    }

//    ~Meta() {
//        delete []_past_timestamps;
//    }
// todo: custom assign function

    inline void update(const uint64_t &past_timestamp, const uint64_t &future_timestamp) {
        _past_timestamps[_past_timestamp_idx%_n_past_intervals] = past_timestamp;
        _past_timestamp_idx = _past_timestamp_idx + (uint8_t) 1;
        //todo: can use bit-wise
        // prevent overflow
        if (_past_timestamp_idx >= _n_past_intervals * 2)
            _past_timestamp_idx -= _n_past_intervals;
        _future_timestamp = future_timestamp;
    }
};


class Gradient {
public:
    static uint8_t _n_past_intervals;
    vector<double> weights;
    double bias = 0;
    uint64_t n_update = 0;
    Gradient () {
        weights = vector<double >(_n_past_intervals);
    }
};

class LRCache : public Cache
{
public:
    //key -> (0/1 list, idx)
    unordered_map<uint64_t, pair<bool, uint32_t>> key_map;
    vector<LRMeta> meta_holder[2];

    // sample_size
    uint sample_rate = 32;
    // threshold
    uint64_t threshold = 10000000;
    double log1p_threshold = log1p(threshold);
    uint64_t n_window_bins = 10;
    uint64_t size_bin;
    // learning_rate
    double learning_rate = 0.0001;
    // n_past_interval
    uint8_t n_past_intervals = 4;
    enum BiasPointT : uint8_t {none = 0, edge = 1, center = 2, mid = 3, rebalance = 4, bin0 = 5, sides = 6};
    uint8_t bias_point = none;
    double alpha = 1;
    uint64_t * f_evicted = nullptr;

    vector<double > weights;
    double bias = 0;
    double mean_diff=0;

    vector<vector<Gradient>> pending_gradients;
    uint64_t gradient_window = 10000;

    //todo: seed and generator
    default_random_engine _generator = default_random_engine();
    uniform_int_distribution<std::size_t> _distribution = uniform_int_distribution<std::size_t>();

    LRCache()
        : Cache()
    {
    }

    void init_with_params(map<string, string> params) override {
        //set params
        for (auto& it: params) {
            if (it.first == "sample_rate") {
                sample_rate = stoul(it.second);
            } else if (it.first == "threshold") {
                threshold = stoull(it.second);
                log1p_threshold = std::log1p(threshold);
            } else if (it.first == "learning_rate") {
                learning_rate = stod(it.second);
            } else if (it.first == "n_past_intervals") {
                n_past_intervals = (uint8_t) stoi(it.second);
            } else if (it.first == "gradient_window") {
                gradient_window = stoull(it.second);
            } else if (it.first == "n_window_bins") {
                n_window_bins = stoull(it.second);
            } else if (it.first == "alpha") {
                alpha = stod(it.second);
                assert(alpha > 0);
            } else if (it.first == "bias_point") {
                bias_point = (uint8_t) stoul(it.second);
                assert(bias_point <= 6);
            } else {
                cerr << "unrecognized parameter: " << it.first << endl;
            }
        }

        //init
        LRMeta::_n_past_intervals = n_past_intervals;
        Gradient::_n_past_intervals = n_past_intervals;
        size_bin = threshold/n_window_bins;
        weights = vector<double >(n_past_intervals);
    }

    virtual bool lookup(SimpleRequest& req);
    bool lookup_without_update(SimpleRequest &_req);
    virtual void admit(SimpleRequest& req);
    virtual void evict(const uint64_t & t);
    void evict(SimpleRequest & req) {};
    void evict() {};
    //sample, rank the 1st and return
    pair<uint64_t, uint32_t > rank(const uint64_t & t);
    void try_train(uint64_t & t);
    void sample(uint64_t &t);
    void sample_without_update(uint64_t &t);
};

static Factory<LRCache> factoryLR("LR");





#endif //WEBCACHESIM_RANDOM_VARIANTS_H
