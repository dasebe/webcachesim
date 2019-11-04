//
// Created by zhenyus on 12/17/18.
//

#ifndef WEBCACHESIM_BELADY_SAMPLE_H
#define WEBCACHESIM_BELADY_SAMPLE_H

#include <cache.h>
#include <unordered_map>
#include <cmath>
#include <random>
#include "mongocxx/client.hpp"
#include "mongocxx/uri.hpp"
#include <bsoncxx/builder/basic/document.hpp>
#include "bsoncxx/json.hpp"

using namespace std;
using bsoncxx::builder::basic::kvp;
using bsoncxx::builder::basic::sub_array;
using namespace webcachesim;

class BeladySampleMeta {
public:
    uint64_t _key;
    uint64_t _size;
    uint64_t _past_timestamp;
    uint64_t _future_timestamp;

    BeladySampleMeta(const uint64_t & key, const uint64_t & size, const uint64_t & past_timestamp,
                     const uint64_t & future_timestamp) {
        _key = key;
        _size = size;
        _past_timestamp = past_timestamp;
        _future_timestamp = future_timestamp;
    }

    inline void update(const uint64_t &past_timestamp, const uint64_t &future_timestamp) {
        _past_timestamp = past_timestamp;
        _future_timestamp = future_timestamp;
    }
};


class BeladySampleCache : public Cache
{
public:
    //key -> (0/1 list, idx)
    unordered_map<uint64_t, pair<bool, uint32_t>> key_map;
    vector<BeladySampleMeta> meta_holder[2];

    // sample_size
    uint sample_rate = 32;
    // threshold
    uint64_t threshold = 10000000;

    default_random_engine _generator = default_random_engine();
    uniform_int_distribution<std::size_t> _distribution = uniform_int_distribution<std::size_t>();
    uint64_t current_t;

#ifdef EVICTION_LOGGING
    vector<int64_t> near_bytes;
    vector<int64_t> middle_bytes;
    vector<int64_t> far_bytes;
#endif

    void init_with_params(const map<string, string> &params) override {
        //set params
        for (auto& it: params) {
            if (it.first == "sample_rate") {
                sample_rate = stoul(it.second);
            } else if (it.first == "threshold") {
                threshold = stoull(it.second);
            } else {
                cerr << "unrecognized parameter: " << it.first << endl;
            }
        }
    }

#ifdef EVICTION_LOGGING

    void update_stat_periodic() override {
        int64_t near_byte = 0, middle_byte = 0, far_byte = 0;
        for (auto &i: meta_holder[0]) {
            if (i._future_timestamp == 0xffffffff) {
                far_byte += i._size;
            } else if (i._future_timestamp - current_t > threshold) {
                middle_byte += i._size;
            } else {
                near_byte += i._size;
            }
        }
        near_bytes.emplace_back(near_byte);
        middle_bytes.emplace_back(middle_byte);
        far_bytes.emplace_back(far_byte);
    }

    virtual void update_stat(bsoncxx::builder::basic::document &doc) {
        doc.append(kvp("near_bytes", [this](sub_array child) {
            for (const auto &element : near_bytes)
                child.append(element);
        }));
        doc.append(kvp("middle_bytes", [this](sub_array child) {
            for (const auto &element : middle_bytes)
                child.append(element);
        }));
        doc.append(kvp("far_bytes", [this](sub_array child) {
            for (const auto &element : far_bytes)
                child.append(element);
        }));
    }

#endif

    virtual bool lookup(SimpleRequest& req);
    virtual void admit(SimpleRequest& req);

    void evict();
    //sample, rank the 1st and return
    pair<uint64_t, uint32_t> rank();
};

static Factory<BeladySampleCache> factoryBeladySample("BeladySample");


//class BeladySampleCacheFilter : public BeladySampleCache
//{
//public:
//    bool out_sample = true;
//    double mean_diff=0;
//    uint64_t n_window_bins = 10;
//    uint64_t size_bin;
//    uint64_t gradient_window = 10000;
//
//    BeladySampleCacheFilter()
//        : BeladySampleCache()
//    {
//    }
//
//    void init_with_params(map<string, string> params) override {
//        BeladySampleCache::init_with_params(params);
//        //set params
//        for (auto& it: params) {
//            if (it.first == "out_sample") {
//                out_sample = (bool) stoul(it.second);
//            } else if (it.first == "gradient_window") {
//                gradient_window = stoull(it.second);
//            } else if (it.first == "n_window_bins") {
//                n_window_bins = stoull(it.second);
//            } else {
//                cerr << "unrecognized parameter: " << it.first << endl;
//            }
//        }
//        size_bin = threshold/n_window_bins;
//    }
//
//    bool lookup(SimpleRequest &_req, vector<vector<Gradient>> & ext_pending_gradients,
//                vector<double> & ext_weights, double & ext_bias);
//    void sample(uint64_t &t, vector<vector<Gradient>> & ext_pending_gradients,
//                vector<double> & ext_weights, double & ext_bias);
//
//};
//
//static Factory<BeladySampleCacheFilter> factoryBeladySampleFilter("BeladySampleFilter");



#endif //WEBCACHESIM_BELADY_SAMPLE_H
