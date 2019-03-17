//
// Created by zhenyus on 1/16/19.
//

#ifndef WEBCACHESIM_GDBT_H
#define WEBCACHESIM_GDBT_H

#include "cache.h"
#include "cache_object.h"
#include <unordered_map>
#include <vector>
#include <random>
#include <cmath>
#include <LightGBM/c_api.h>
#include <assert.h>

using namespace std;

namespace GDBT {
    uint8_t max_n_past_timestamps = 32;
    uint8_t max_n_past_distances = 31;
    uint8_t base_edwt_window = 10;
    uint8_t n_edwt_feature = 10;
    vector<uint32_t > edwt_windows;
    vector<double > hash_edwt;
    uint32_t max_hash_edwt_idx;
    uint64_t forget_window = 10000000;
    uint64_t n_extra_fields = 0;
}


class GDBTMeta {
public:
    uint64_t _key;
    uint64_t _size;
    uint8_t _past_distance_idx;
    uint64_t _past_timestamp;
    vector<uint64_t> _past_distances;
    vector<uint64_t> _extra_features;
    vector<double > _edwt;

    GDBTMeta(const uint64_t & key, const uint64_t & size, const uint64_t & past_timestamp,
            const vector<uint64_t> & extra_features) {
        _key = key;
        _size = size;
        _past_timestamp = past_timestamp;
        _past_distances = vector<uint64_t >(GDBT::max_n_past_distances);
        _past_distance_idx = (uint8_t) 0;
        _extra_features = extra_features;
        _edwt = vector<double >(GDBT::n_edwt_feature, 1);
    }

    inline void update(const uint64_t &past_timestamp) {
        //distance
        uint64_t _distance = past_timestamp - _past_timestamp;
        _past_distances[_past_distance_idx%GDBT::max_n_past_distances] = _distance;
        _past_distance_idx = _past_distance_idx + (uint8_t) 1;
        if (_past_distance_idx >= GDBT::max_n_past_distances * 2)
            _past_distance_idx -= GDBT::max_n_past_distances;
        //timestamp
        _past_timestamp = past_timestamp;
        for (uint8_t i = 0; i < GDBT::n_edwt_feature; ++i) {
            uint32_t _distance_idx = min(uint32_t (_distance/GDBT::edwt_windows[i]), GDBT::max_hash_edwt_idx);
            _edwt[i] = _edwt[i] * GDBT::hash_edwt[_distance_idx] + 1;
        }
    }
};

class GDBTPendingTrainingData {
public:
    vector<uint64_t > past_distances;
    uint64_t size;
    vector<uint64_t > extra_features;
    vector<double > edwt;
    GDBTPendingTrainingData(const GDBTMeta& meta, const uint64_t & t) {
        size = meta._size;
        past_distances.reserve(GDBT::max_n_past_timestamps);
        past_distances.emplace_back(t - meta._past_timestamp);
        uint64_t this_past_distance = 0;
        for (int j = 0; j < meta._past_distance_idx && j < GDBT::max_n_past_distances; ++j) {
            uint8_t past_distance_idx = (meta._past_distance_idx - 1 - j) % GDBT::max_n_past_distances;
            const uint64_t & past_distance = meta._past_distances[past_distance_idx];
            this_past_distance += past_distance;
            if (this_past_distance < GDBT::forget_window) {
                past_distances.emplace_back(past_distance);
            } else
                break;
        }
        extra_features = meta._extra_features;
        for (int k = 0; k < GDBT::n_edwt_feature; ++k) {
            uint32_t _distance_idx = min(uint32_t (t-meta._past_timestamp) / GDBT::edwt_windows[k],
                                         GDBT::max_hash_edwt_idx);
            edwt.emplace_back(meta._edwt[k]*GDBT::hash_edwt[_distance_idx]);
        }
    }
};

class GDBTTrainingData {
public:
    vector<float> labels;
    vector<int32_t> indptr = {0};
    vector<int32_t> indices;
    vector<double> data;
    void append(const GDBTPendingTrainingData& pending, float & future_distance) {
        int32_t counter = indptr.back();
        auto this_data_size = pending.past_distances.size();
        indices.reserve(indices.size() + this_data_size);
        data.reserve(data.size() + this_data_size);
        for (int i = 0; i < this_data_size; ++i) {
            indices.emplace_back(i);
            data.emplace_back(pending.past_distances[i]);
        }
        counter += this_data_size;

        indices.emplace_back(GDBT::max_n_past_timestamps);
        data.push_back(pending.size);
        ++counter;

        for (int k = 0; k < GDBT::n_extra_fields; ++k) {
            indices.push_back(GDBT::max_n_past_timestamps + k + 1);
            data.push_back(pending.extra_features[k]);
        }
        counter += GDBT::n_extra_fields;

        indices.push_back(GDBT::max_n_past_timestamps+GDBT::n_extra_fields+1);
        data.push_back(this_data_size);
        ++counter;

        for (int k = 0; k < GDBT::n_edwt_feature; ++k) {
            indices.push_back(GDBT::max_n_past_timestamps + GDBT::n_extra_fields + 2 + k);
            data.push_back(pending.edwt[k]);
        }
        counter += pending.edwt.size();

        labels.push_back(future_distance);
        indptr.push_back(counter);
    }

    void clear() {
        labels.clear();
        indptr.clear();
        indptr.emplace_back(0);
        indices.clear();
        data.clear();
    }
};

class GDBTCache : public Cache
{
public:
    //key -> (0/1 list, idx)
    unordered_map<uint64_t, pair<bool, uint32_t>> key_map;
    vector<GDBTMeta> meta_holder[2];

    unordered_map<uint64_t, uint64_t> forget_table;
    //one object can be sample multiple times
    unordered_multimap<uint64_t, GDBTPendingTrainingData> pending_training_data;
    GDBTTrainingData training_data;

    // sample_size
    uint sample_rate = 32;
    uint64_t training_sample_interval = 1;

    uint64_t batch_size = 100000;
    uint64_t n_feature;
    uint64_t num_threads = 1;
    double training_loss = 0;

    BoosterHandle booster = nullptr;

    unordered_map<string, string> GDBT_train_params = {
            {"boosting",                   "gbdt"},
            {"objective",                  "regression"},
            {"num_iterations",             "1"},
            {"num_leaves",                  "32"},
            {"num_threads",                "1"},
            {"shrinkage_rate",           "0.1"},
            {"feature_fraction",           "0.8"},
            {"bagging_freq",               "5"},
            {"bagging_fraction",           "0.8"},
            {"learning_rate",              "0.1"},
    };

    unordered_map<string, string> GDBT_inference_params;

    double training_error = 0;
    double inference_error = 0;

    enum ObjectiveT: uint8_t {byte_hit_rate=0, object_hit_rate=1};
    ObjectiveT objective = byte_hit_rate;

    default_random_engine _generator = default_random_engine();
    uniform_int_distribution<std::size_t> _distribution = uniform_int_distribution<std::size_t>();

    GDBTCache()
        : Cache()
    {
    }

    void init_with_params(map<string, string> params) override {
        //set params
        for (auto& it: params) {
            if (it.first == "sample_rate") {
                sample_rate = stoul(it.second);
            } else if (it.first == "forget_window") {
                GDBT::forget_window = stoull(it.second);
            } else if (it.first == "max_n_past_timestamps") {
                GDBT::max_n_past_timestamps = (uint8_t) stoi(it.second);
            } else if (it.first == "batch_size") {
                batch_size = stoull(it.second);
            } else if (it.first == "n_extra_fields") {
                GDBT::n_extra_fields = stoull(it.second);
            } else if (it.first == "num_iterations") {
                GDBT_train_params["num_iterations"] = it.second;
            } else if (it.first == "learning_rate") {
                GDBT_train_params["learning_rate"] = it.second;
            } else if (it.first == "num_threads") {
                GDBT_train_params["num_threads"] = it.second;
            } else if (it.first == "training_sample_interval") {
                training_sample_interval = stoull(it.second);
            } else if (it.first == "n_edwt_feature") {
                GDBT::n_edwt_feature = stoull(it.second);
            } else if (it.first == "objective") {
                if (it.second == "byte_hit_rate")
                    objective = byte_hit_rate;
                else if (it.second == "object_hit_rate")
                    objective = object_hit_rate;
                else {
                    cerr<<"error: unknown objective"<<endl;
                    exit(-1);
                }
            } else {
                cerr << "unrecognized parameter: " << it.first << endl;
            }
        }

        GDBT::max_n_past_distances = GDBT::max_n_past_timestamps-1;
        //init
        GDBT::edwt_windows = vector<uint32_t >(GDBT::n_edwt_feature);
        for (uint8_t i = 0; i < GDBT::n_edwt_feature; ++i) {
            GDBT::edwt_windows[i] = pow(2, GDBT::base_edwt_window+i);
        }
        GDBT::max_hash_edwt_idx = (uint64_t) (GDBT::forget_window/pow(2, GDBT::base_edwt_window))-1;
        GDBT::hash_edwt = vector<double >(GDBT::max_hash_edwt_idx+1);
        for (int i = 0; i < GDBT::hash_edwt.size(); ++i)
            GDBT::hash_edwt[i] = pow(0.5, i);

        //interval, distances, size, extra_features, n_past_intervals, edwt
        n_feature = GDBT::max_n_past_timestamps + GDBT::n_extra_fields + 2 + GDBT::n_edwt_feature;
        if (GDBT::n_extra_fields) {
            string categorical_feature = to_string(GDBT::max_n_past_timestamps+1);
            for (uint i = 0; i < GDBT::n_extra_fields-1; ++i) {
                categorical_feature += ","+to_string(GDBT::max_n_past_timestamps+2+i);
            }
            GDBT_train_params["categorical_feature"] = categorical_feature;
        }
        GDBT_inference_params = GDBT_train_params;
    }

    bool lookup(SimpleRequest& req);
    void admit(SimpleRequest& req);
    void evict(const uint64_t & t);
    void evict(SimpleRequest & req) {};
    void evict() {};
    void forget(uint64_t & t);
    //sample, rank the 1st and return
    pair<uint64_t, uint32_t > rank(const uint64_t & t);
    void train();
    void sample(uint64_t &t);
};

static Factory<GDBTCache> factoryGBDT("GDBT");
#endif //WEBCACHESIM_GDBT_H
