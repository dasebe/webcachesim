//
// Created by zhenyus on 1/13/19.
//

#ifndef WEBCACHESIM_BINS_H
#define WEBCACHESIM_BINS_H

#include "cache.h"
#include <vector>
#include <unordered_map>
#include <random>

using namespace std;


class BinsMeta {
public:
    static uint8_t _max_n_past_timestamps;
    uint64_t _key;
    uint64_t _size;
    uint8_t _past_distance_idx;
    uint64_t _past_timestamp;
    vector<uint64_t> _past_distances;

    BinsMeta(const uint64_t & key, const uint64_t & size, const uint64_t & past_timestamp) {
        _key = key;
        _size = size;
        _past_timestamp = past_timestamp;
        _past_distances = vector<uint64_t >(_max_n_past_timestamps);
        _past_distance_idx = (uint8_t) 0;
    }

    inline void update(const uint64_t &past_timestamp) {
        //distance
        _past_distances[_past_distance_idx%_max_n_past_timestamps] = past_timestamp - _past_timestamp;
        _past_distance_idx = _past_distance_idx + (uint8_t) 1;
        if (_past_distance_idx >= _max_n_past_timestamps * 2)
            _past_distance_idx -= _max_n_past_timestamps;
        //timestamp
        _past_timestamp = past_timestamp;
    }
};


class BinsCache : public Cache
{
public:
    //key -> (0/1 list, idx)
    unordered_map<uint64_t, pair<bool, uint32_t>> key_map;
    vector<BinsMeta> meta_holder[2];

    // sample_size
    uint sample_rate = 32;
    // threshold
    uint64_t threshold = 10000000;
    uint64_t bin_width =  1000000;
    uint64_t n_window_bins = threshold/bin_width;
    // n_past_interval
    uint8_t max_n_past_intervals = 4;

    vector<double > future_expections;
    vector<double > e_weights;
    default_random_engine _generator = default_random_engine();
    uniform_int_distribution<std::size_t> _distribution = uniform_int_distribution<std::size_t>();

    BinsCache()
        : Cache()
    {
    }

    void init_with_params(map<string, string> params) override {
        e_weights = vector<double >(n_window_bins, 0);
        e_weights[0] = 1;
        //set params
        for (auto& it: params) {
            if (it.first == "sample_rate") {
                sample_rate = stoul(it.second);
            } else if (it.first == "threshold") {
                threshold = stoull(it.second);
            } else if (it.first == "n_past_intervals") {
                max_n_past_intervals = (uint8_t) stoi(it.second);
            } else if (it.first == "w0") {
                e_weights[0] = stod(it.second);
            } else if (it.first == "w1") {
                e_weights[1] = stod(it.second);
            } else if (it.first == "w2") {
                e_weights[2] = stod(it.second);
            } else if (it.first == "w3") {
                e_weights[3] = stod(it.second);
            } else if (it.first == "w4") {
                e_weights[4] = stod(it.second);
            } else if (it.first == "w5") {
                e_weights[5] = stod(it.second);
            } else if (it.first == "w6") {
                e_weights[6] = stod(it.second);
            } else if (it.first == "w7") {
                e_weights[7] = stod(it.second);
            } else if (it.first == "w8") {
                e_weights[8] = stod(it.second);
            } else if (it.first == "w9") {
                e_weights[9] = stod(it.second);
            } else {
                cerr << "unrecognized parameter: " << it.first << endl;
            }
        }

        //we want the w to be sum to 1 and monotonically decreasing
        double sum_w = 0;
        for (auto i : e_weights) {
            sum_w += i;
        }
        if (abs(sum_w - 1) > 0.01) {
            exit(-1);
        }
        for (uint i = 0; i < n_window_bins; ++i) {
            for (uint j = i+1; j < n_window_bins; ++j) {
                if (e_weights[i] < e_weights[j])
                    exit(-1);
            }
        }
//        e_weights[0] = 0;

        //init
        //distance can be 10, but interval can only be 9
        future_expections = vector<double >(pow(n_window_bins, 2)*pow(n_window_bins+1, max_n_past_intervals-1));
        BinsMeta::_max_n_past_timestamps = max_n_past_intervals;
    }

    virtual bool lookup(SimpleRequest& req);
    virtual void admit(SimpleRequest& req);
    virtual void evict(const uint64_t & t);
    void evict(SimpleRequest & req) {};
    void evict() {};
    void set_future_expections(const uint64_t & t);
    //sample, rank the 1st and return
    pair<uint64_t, uint32_t > rank(const uint64_t & t);
};

static Factory<BinsCache> factoryBins("Bins");


#endif //WEBCACHESIM_BINS_H
