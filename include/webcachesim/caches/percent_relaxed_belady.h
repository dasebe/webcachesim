//
// Created by zhenyus on 12/17/18.
//

#ifndef WEBCACHESIM_PERCENT_RELAXED_BELADY_H
#define WEBCACHESIM_PERCENT_RELAXED_BELADY_H

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

class PercentRelaxedBeladyMeta {
public:
    uint64_t _key;
    uint64_t _size;
    uint64_t _past_timestamp;
    uint64_t _future_timestamp;

    PercentRelaxedBeladyMeta(const uint64_t &key, const uint64_t &size, const uint64_t &past_timestamp,
                             const uint64_t &future_timestamp) {
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


class PercentRelaxedBeladyCache : public Cache {
public:
    //key -> (list pos)
    unordered_map<uint64_t, uint32_t> key_map;
    vector<PercentRelaxedBeladyMeta> meta_holder;

    // sample_size
    uint sample_rate = 4000;
    // eviction: random select from top p percent
    double p = 0.05;

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
        for (auto &it: params) {
            if (it.first == "sample_rate") {
                sample_rate = stoul(it.second);
            } else if (it.first == "p") {
                p = stof(it.second);
                if (p <= 0 || p > 1) {
                    throw invalid_argument("0 < p <= 1");
                }
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
        for (auto &i: meta_holder) {
            if (i._future_timestamp - current_t >= threshold) {
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

static Factory<PercentRelaxedBeladyCache> factoryPercentRelaxedBelady("PercentRelaxedBelady");

#endif //WEBCACHESIM_PERCENT_RELAXED_BELADY_H
