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
#include <unordered_set>

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
    unordered_map<uint64_t, uint32_t> key_map;
    vector<BeladySampleMeta> meta_holder;

    // sample_size
    uint sample_rate = 32;
    // threshold
    uint64_t threshold = 10000000;

    default_random_engine _generator = default_random_engine();
    uniform_int_distribution<std::size_t> _distribution = uniform_int_distribution<std::size_t>();
    uint64_t current_t;

    bool memorize_sample = false;
    unordered_set<uint64_t> memorize_sample_keys;

#ifdef EVICTION_LOGGING
    // how far an evicted object will access again
    vector<uint8_t> eviction_distances;
    uint64_t byte_million_req;
    string task_id;
    string dburl;
    vector<double> beyond_byte_ratio;
    vector<double> beyond_obj_ratio;
#endif

    void init_with_params(const map<string, string> &params) override {
        //set params
        for (auto& it: params) {
            if (it.first == "sample_rate") {
                sample_rate = stoul(it.second);
            } else if (it.first == "threshold") {
                threshold = stoull(it.second);
            } else if (it.first == "memorize_sample") {
                memorize_sample = static_cast<bool>(stoi(it.second));
#ifdef EVICTION_LOGGING
                } else if (it.first == "byte_million_req") {
                    byte_million_req = stoull(it.second);
                } else if (it.first == "task_id") {
                    task_id = it.second;
                } else if (it.first == "dburl") {
                    dburl = it.second;
#endif
            } else {
                cerr << "unrecognized parameter: " << it.first << endl;
            }
        }
    }

#ifdef EVICTION_LOGGING

    void update_stat_periodic() override {
        int64_t within_byte = 0, beyond_byte = 0;
        int64_t within_obj = 0, beyond_obj = 0;
        for (auto &i: meta_holder[0]) {
            if (i._future_timestamp - current_t > threshold) {
                beyond_byte += i._size;
                ++beyond_obj;
            } else {
                within_byte += i._size;
                ++within_obj;
            }
        }
        beyond_byte_ratio.emplace_back(static_cast<double>(beyond_byte) / (beyond_byte + within_byte));
        beyond_obj_ratio.emplace_back(static_cast<double>(beyond_obj) / (beyond_obj + within_obj));
    }

    void update_stat(bsoncxx::builder::basic::document &doc) override {
        doc.append(kvp("beyond_byte_ratio", [this](sub_array child) {
            for (const auto &element : beyond_byte_ratio)
                child.append(element);
        }));
        doc.append(kvp("beyond_obj_ratio", [this](sub_array child) {
            for (const auto &element : beyond_obj_ratio)
                child.append(element);
        }));
        //Log to GridFs because the value is too big to store in mongodb
        try {
            mongocxx::client client = mongocxx::client{mongocxx::uri(dburl)};
            mongocxx::database db = client["webcachesim"];
            auto bucket = db.gridfs_bucket();

            auto uploader = bucket.open_upload_stream(task_id + ".evictions");
            for (auto &b: eviction_distances)
                uploader.write((uint8_t *) (&b), sizeof(uint8_t));
            uploader.close();
        } catch (const std::exception &xcp) {
            cerr << "error: db connection failed: " << xcp.what() << std::endl;
            abort();
        }
    }

#endif

    bool lookup(SimpleRequest &req) override;

    void admit(SimpleRequest &req) override;

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
