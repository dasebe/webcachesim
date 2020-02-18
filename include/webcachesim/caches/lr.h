//
// Created by zhenyus on 3/15/19.
//

#ifndef WEBCACHESIM_LR_H
#define WEBCACHESIM_LR_H

#include "cache.h"
#include <unordered_map>
#include <vector>
#include <cmath>
#include <random>
#include <assert.h>


using namespace std;
using namespace webcachesim;

namespace LR {
    uint8_t max_n_past_timestamps = 4;
    uint64_t forget_window = 10000000;
    double log1p_forget_window = log1p(forget_window);
}

class LRMeta {
public:
    uint64_t _key;
    uint8_t _past_timestamp_idx;
    vector<uint64_t> _past_timestamps;
    uint64_t _size;

    LRMeta(const uint64_t &key, const uint64_t &size, const uint64_t &past_timestamp) {
        _key = key;
        _size = size;
        _past_timestamps = vector<uint64_t>(LR::max_n_past_timestamps);
        _past_timestamps[0] = past_timestamp;
        _past_timestamp_idx = (uint8_t) 1;
    }

    inline void update(const uint64_t &past_timestamp) {
        _past_timestamps[_past_timestamp_idx % LR::max_n_past_timestamps] = past_timestamp;
        _past_timestamp_idx = _past_timestamp_idx + (uint8_t) 1;
        //todo: can use bit-wise prevent overflow
        if (_past_timestamp_idx >= LR::max_n_past_timestamps * 2)
            _past_timestamp_idx -= LR::max_n_past_timestamps;
    }
};

class LRPendingTrainingData {
public:
    //past timestamp0 is in metadata
    vector<double> past_distances;
    uint64_t sample_time;
    uint64_t size;

    LRPendingTrainingData(const LRMeta &meta, const uint64_t &t) {
        size = meta._size;
        sample_time = t;
        uint8_t n_past_timestamps = min(LR::max_n_past_timestamps, meta._past_timestamp_idx);
        for (uint8_t i = 0; i < n_past_timestamps; ++i) {
            uint8_t past_timestamp_idx = static_cast<uint8_t>(meta._past_timestamp_idx - 1 - i)
                                         % LR::max_n_past_timestamps;
            uint64_t past_distance = t - meta._past_timestamps[past_timestamp_idx];
            if (past_distance < LR::forget_window)
                past_distances.emplace_back(log1p(past_distance));
            else
                break;
        }
    }
};

class LRTrainingData {
public:
    vector<double> past_distances;
    uint64_t size;
    double future_distance;

    LRTrainingData(const LRPendingTrainingData &pending, double &_future_distance) {
        size = pending.size;
        past_distances = pending.past_distances;
        future_distance = _future_distance;
    }
};

class LRCache : public Cache {
public:
    //key -> (0/1 list, idx)
    unordered_map<uint64_t, pair<bool, uint32_t>> key_map;
    vector<LRMeta> meta_holder[2];

    unordered_map<uint64_t, uint64_t> forget_table;
    //one object can be sample multiple times
    unordered_multimap<uint64_t, LRPendingTrainingData> pending_training_data;
    vector<LRTrainingData> training_data;

    // sample_size
    uint32_t sample_rate = 32;
    uint64_t training_sample_interval = 32;

    double learning_rate = 0.0001;

    vector<double> weights;
    double bias = 0;
    double training_loss = 0;

    uint64_t batch_size = 10000;

    enum ObjectiveT : uint8_t {
        byte_hit_rate = 0, object_hit_rate = 1
    };
    ObjectiveT objective = byte_hit_rate;

    default_random_engine _generator = default_random_engine();
    uniform_int_distribution<std::size_t> _distribution = uniform_int_distribution<std::size_t>();

    LRCache()
            : Cache() {
    }

    void init_with_params(const map<string, string> &params) override {
        //set params
        for (auto &it: params) {
            if (it.first == "sample_rate") {
                sample_rate = static_cast<uint32_t>(stoul(it.second));
            } else if (it.first == "training_sample_interval") {
                training_sample_interval = stoull(it.second);
            } else if (it.first == "forget_window") {
                LR::forget_window = stoull(it.second);
                LR::log1p_forget_window = std::log1p(LR::forget_window);
            } else if (it.first == "learning_rate") {
                learning_rate = stod(it.second);
            } else if (it.first == "max_n_past_timestamps") {
                LR::max_n_past_timestamps = (uint8_t) stoi(it.second);
            } else if (it.first == "batch_size") {
                batch_size = stoull(it.second);
            } else if (it.first == "objective") {
                if (it.second == "byte_hit_rate")
                    objective = byte_hit_rate;
                else if (it.second == "object_hit_rate")
                    objective = object_hit_rate;
                else {
                    cerr << "error: unknown objective" << endl;
                    exit(-1);
                }
            } else {
                cerr << "unrecognized parameter: " << it.first << endl;
            }
        }

        training_data.reserve(batch_size);
    }

    bool lookup(SimpleRequest &req);

    void admit(SimpleRequest &req);

    /*
     * rank and evict from cache
     */
    void evict(const uint64_t &t);

    void forget(uint64_t &t);

    //sample, rank the 1st and return
    pair<uint64_t, uint32_t> rank(const uint64_t &t);

    void train();

    void sample(uint64_t &t);
};

static Factory<LRCache> factoryLR("LR");


#endif //WEBCACHESIM_LR_H
