//
// Created by zhenyus on 1/16/19.
//

#ifndef WEBCACHESIM_WLC_H
#define WEBCACHESIM_WLC_H

#include "cache.h"
#include <unordered_map>
#include "sparsepp/spp.h"
#include <vector>
#include <random>
#include <cmath>
#include <LightGBM/c_api.h>
#include <assert.h>
#include <fstream>
#include <list>
#include "mongocxx/client.hpp"
#include "mongocxx/uri.hpp"
#include <bsoncxx/builder/basic/document.hpp>
#include "bsoncxx/json.hpp"

using namespace webcachesim;
using namespace std;
using spp::sparse_hash_map;
typedef uint64_t WLCKey;
using bsoncxx::builder::basic::kvp;
using bsoncxx::builder::basic::sub_array;

namespace WLC {
    uint8_t max_n_past_timestamps = 32;
    uint8_t max_n_past_distances = 31;
    uint8_t base_edc_window = 10;
    const uint8_t n_edc_feature = 10;
    vector<uint32_t> edc_windows;
    vector<double> hash_edc;
    uint32_t max_hash_edc_idx;
    uint32_t memory_window = 67108864;
    uint32_t n_extra_fields = 0;
    uint32_t batch_size = 131072;
    const uint max_n_extra_feature = 4;
    uint32_t n_feature;
    //TODO: interval clock should tick by event instead of following incoming packet time
#ifdef EVICTION_LOGGING
    unordered_map<uint64_t, uint32_t> future_timestamps;
    uint32_t n_logging_start;
    vector<float> trainings_and_predictions;
    bool start_train_logging = false;
#endif
}

struct WLCMetaExtra {
    //164 byte at most
    //not 1 hit wonder
    float _edc[10];
    vector<uint32_t> _past_distances;
    //the next index to put the distance
    uint8_t _past_distance_idx = 1;

    WLCMetaExtra(const uint32_t &distance) {
        _past_distances = vector<uint32_t>(1, distance);
        for (uint8_t i = 0; i < WLC::n_edc_feature; ++i) {
            uint32_t _distance_idx = min(uint32_t(distance / WLC::edc_windows[i]), WLC::max_hash_edc_idx);
            _edc[i] = WLC::hash_edc[_distance_idx] + 1;
        }
    }

    void update(const uint32_t &distance) {
        uint8_t distance_idx = _past_distance_idx % WLC::max_n_past_distances;
        if (_past_distances.size() < WLC::max_n_past_distances)
            _past_distances.emplace_back(distance);
        else
            _past_distances[distance_idx] = distance;
        assert(_past_distances.size() <= WLC::max_n_past_distances);
        _past_distance_idx = _past_distance_idx + (uint8_t) 1;
        if (_past_distance_idx >= WLC::max_n_past_distances * 2)
            _past_distance_idx -= WLC::max_n_past_distances;
        for (uint8_t i = 0; i < WLC::n_edc_feature; ++i) {
            uint32_t _distance_idx = min(uint32_t(distance / WLC::edc_windows[i]), WLC::max_hash_edc_idx);
            _edc[i] = _edc[i] * WLC::hash_edc[_distance_idx] + 1;
        }
    }
};

class WLCMeta {
public:
    //25 byte
    uint64_t _key;
    uint32_t _size;
    uint32_t _past_timestamp;
    uint16_t _extra_features[WLC::max_n_extra_feature];
    WLCMetaExtra *_extra = nullptr;
    vector<uint32_t> _sample_times;
#ifdef EVICTION_LOGGING
    vector<uint32_t> _eviction_sample_times;
    uint32_t _future_timestamp;
#endif


#ifdef EVICTION_LOGGING
    WLCMeta(const uint64_t &key, const uint64_t &size, const uint64_t &past_timestamp,
            const vector<uint16_t> &extra_features, const uint64_t &future_timestamp) {
        _key = key;
        _size = size;
        _past_timestamp = past_timestamp;
        for (int i = 0; i < WLC::n_extra_fields; ++i)
            _extra_features[i] = extra_features[i];
        _future_timestamp = future_timestamp;
    }

#else
    WLCMeta(const uint64_t &key, const uint64_t &size, const uint64_t &past_timestamp,
            const vector<uint16_t> &extra_features) {
        _key = key;
        _size = size;
        _past_timestamp = past_timestamp;
        for (int i = 0; i < WLC::n_extra_fields; ++i)
            _extra_features[i] = extra_features[i];
    }
#endif

    virtual ~WLCMeta() = default;

    void emplace_sample(uint32_t &sample_t) {
        _sample_times.emplace_back(sample_t);
    }

    void emplace_eviction_sample(uint32_t &sample_t) {
        _eviction_sample_times.emplace_back(sample_t);
    }

    void free() {
        delete _extra;
    }

#ifdef EVICTION_LOGGING

    void update(const uint32_t &past_timestamp, const uint32_t &future_timestamp) {
        //distance
        uint32_t _distance = past_timestamp - _past_timestamp;
        assert(_distance);
        if (!_extra) {
            _extra = new WLCMetaExtra(_distance);
        } else
            _extra->update(_distance);
        //timestamp
        _past_timestamp = past_timestamp;
        _future_timestamp = future_timestamp;
    }

#else
    void update(const uint32_t &past_timestamp) {
        //distance
        uint32_t _distance = past_timestamp - _past_timestamp;
        assert(_distance);
        if (!_extra) {
            _extra = new WLCMetaExtra(_distance);
        } else
            _extra->update(_distance);
        //timestamp
        _past_timestamp = past_timestamp;
    }
#endif

    int feature_overhead() {
        int ret = sizeof(WLCMeta);
        if (_extra)
            ret += sizeof(WLCMetaExtra) - sizeof(_sample_times) + _extra->_past_distances.capacity() * sizeof(uint32_t);
        return ret;
    }

    int sample_overhead() {
        return sizeof(_sample_times) + sizeof(uint32_t) * _sample_times.capacity();
    }
};


class InCacheMeta : public WLCMeta {
public:
    //pointer to lru0
    list<WLCKey>::const_iterator p_last_request;
    //any change to functions?

#ifdef EVICTION_LOGGING

    InCacheMeta(const uint64_t &key,
                const uint64_t &size,
                const uint64_t &past_timestamp,
                const vector<uint16_t> &extra_features,
                const uint64_t &future_timestamp,
                const list<WLCKey>::const_iterator &it) :
            WLCMeta(key, size, past_timestamp, extra_features, future_timestamp) {
        p_last_request = it;
    };
#else
    InCacheMeta(const uint64_t &key,
                const uint64_t &size,
                const uint64_t &past_timestamp,
                const vector<uint16_t> &extra_features, const list<WLCKey>::const_iterator &it) :
            WLCMeta(key, size, past_timestamp, extra_features) {
        p_last_request = it;
    };
#endif

    InCacheMeta(const WLCMeta &meta, const list<WLCKey>::const_iterator &it) : WLCMeta(meta) {
        p_last_request = it;
    };

};

class InCacheLRUQueue {
public:
    list<WLCKey> dq;

    //size?
    //the hashtable (location information is maintained outside, and assume it is always correct)
    list<WLCKey>::const_iterator request(WLCKey key) {
        dq.emplace_front(key);
        return dq.cbegin();
    }

    list<WLCKey>::const_iterator re_request(list<WLCKey>::const_iterator it) {
        if (it != dq.cbegin()) {
            dq.emplace_front(*it);
            dq.erase(it);
        }
        return dq.cbegin();
    }
};

class WLCTrainingData {
public:
    vector<float> labels;
    vector<int32_t> indptr;
    vector<int32_t> indices;
    vector<double> data;

    WLCTrainingData() {
        labels.reserve(WLC::batch_size);
        indptr.reserve(WLC::batch_size + 1);
        indptr.emplace_back(0);
        indices.reserve(WLC::batch_size * WLC::n_feature);
        data.reserve(WLC::batch_size * WLC::n_feature);
    }

    void emplace_back(WLCMeta &meta, uint32_t &sample_timestamp, uint32_t &future_interval, const uint64_t &key) {
        int32_t counter = indptr.back();

        indices.emplace_back(0);
        data.emplace_back(sample_timestamp - meta._past_timestamp);
        ++counter;

        uint32_t this_past_distance = 0;
        int j = 0;
        uint8_t n_within = 0;
        if (meta._extra) {
            for (; j < meta._extra->_past_distance_idx && j < WLC::max_n_past_distances; ++j) {
                uint8_t past_distance_idx = (meta._extra->_past_distance_idx - 1 - j) % WLC::max_n_past_distances;
                const uint32_t &past_distance = meta._extra->_past_distances[past_distance_idx];
                this_past_distance += past_distance;
                indices.emplace_back(j + 1);
                data.emplace_back(past_distance);
                if (this_past_distance < WLC::memory_window) {
                    ++n_within;
                }
            }
        }

        counter += j;

        indices.emplace_back(WLC::max_n_past_timestamps);
        data.push_back(meta._size);
        ++counter;

        for (int k = 0; k < WLC::n_extra_fields; ++k) {
            indices.push_back(WLC::max_n_past_timestamps + k + 1);
            data.push_back(meta._extra_features[k]);
        }
        counter += WLC::n_extra_fields;

        indices.push_back(WLC::max_n_past_timestamps + WLC::n_extra_fields + 1);
        data.push_back(n_within);
        ++counter;

        if (meta._extra) {
            for (int k = 0; k < WLC::n_edc_feature; ++k) {
                indices.push_back(WLC::max_n_past_timestamps + WLC::n_extra_fields + 2 + k);
                uint32_t _distance_idx = std::min(
                        uint32_t(sample_timestamp - meta._past_timestamp) / WLC::edc_windows[k],
                        WLC::max_hash_edc_idx);
                data.push_back(meta._extra->_edc[k] * WLC::hash_edc[_distance_idx]);
            }
        } else {
            for (int k = 0; k < WLC::n_edc_feature; ++k) {
                indices.push_back(WLC::max_n_past_timestamps + WLC::n_extra_fields + 2 + k);
                uint32_t _distance_idx = std::min(
                        uint32_t(sample_timestamp - meta._past_timestamp) / WLC::edc_windows[k],
                        WLC::max_hash_edc_idx);
                data.push_back(WLC::hash_edc[_distance_idx]);
            }
        }

        counter += WLC::n_edc_feature;

        labels.push_back(log1p(future_interval));
        indptr.push_back(counter);


#ifdef EVICTION_LOGGING
        if ((sample_timestamp >= WLC::n_logging_start) && !WLC::start_train_logging && (indptr.size() == 2)) {
            WLC::start_train_logging = true;
        }

        if (WLC::start_train_logging) {
//            training_and_prediction_logic_timestamps.emplace_back(current_t / 65536);
            int i = indptr.size() - 2;
            int current_idx = indptr[i];
            for (int p = 0; p < WLC::n_feature; ++p) {
                if (p == indices[current_idx]) {
                    WLC::trainings_and_predictions.emplace_back(data[current_idx]);
                    if (current_idx + 1 < indptr[i + 1])
                        ++current_idx;
                } else
                    WLC::trainings_and_predictions.emplace_back(NAN);
            }
            WLC::trainings_and_predictions.emplace_back(future_interval);
            WLC::trainings_and_predictions.emplace_back(NAN);
            WLC::trainings_and_predictions.emplace_back(sample_timestamp);
            WLC::trainings_and_predictions.emplace_back(0);
            WLC::trainings_and_predictions.emplace_back(key);
        }
#endif

    }

    void clear() {
        labels.clear();
        indptr.resize(1);
        indices.clear();
        data.clear();
    }
};


class WLCEvictionTrainingData {
public:
    vector<float> labels;
    vector<int32_t> indptr;
    vector<int32_t> indices;
    vector<double> data;

    WLCEvictionTrainingData() {
        labels.reserve(WLC::batch_size);
        indptr.reserve(WLC::batch_size + 1);
        indptr.emplace_back(0);
        indices.reserve(WLC::batch_size * WLC::n_feature);
        data.reserve(WLC::batch_size * WLC::n_feature);
    }

    void emplace_back(WLCMeta &meta, uint32_t &sample_timestamp, uint32_t &future_interval, const uint64_t &key) {
        int32_t counter = indptr.back();

        indices.emplace_back(0);
        data.emplace_back(sample_timestamp - meta._past_timestamp);
        ++counter;

        uint32_t this_past_distance = 0;
        int j = 0;
        uint8_t n_within = 0;
        if (meta._extra) {
            for (; j < meta._extra->_past_distance_idx && j < WLC::max_n_past_distances; ++j) {
                uint8_t past_distance_idx = (meta._extra->_past_distance_idx - 1 - j) % WLC::max_n_past_distances;
                const uint32_t &past_distance = meta._extra->_past_distances[past_distance_idx];
                this_past_distance += past_distance;
                indices.emplace_back(j + 1);
                data.emplace_back(past_distance);
                if (this_past_distance < WLC::memory_window) {
                    ++n_within;
                }
            }
        }

        counter += j;

        indices.emplace_back(WLC::max_n_past_timestamps);
        data.push_back(meta._size);
        ++counter;

        for (int k = 0; k < WLC::n_extra_fields; ++k) {
            indices.push_back(WLC::max_n_past_timestamps + k + 1);
            data.push_back(meta._extra_features[k]);
        }
        counter += WLC::n_extra_fields;

        indices.push_back(WLC::max_n_past_timestamps + WLC::n_extra_fields + 1);
        data.push_back(n_within);
        ++counter;

        if (meta._extra) {
            for (int k = 0; k < WLC::n_edc_feature; ++k) {
                indices.push_back(WLC::max_n_past_timestamps + WLC::n_extra_fields + 2 + k);
                uint32_t _distance_idx = std::min(
                        uint32_t(sample_timestamp - meta._past_timestamp) / WLC::edc_windows[k],
                        WLC::max_hash_edc_idx);
                data.push_back(meta._extra->_edc[k] * WLC::hash_edc[_distance_idx]);
            }
        } else {
            for (int k = 0; k < WLC::n_edc_feature; ++k) {
                indices.push_back(WLC::max_n_past_timestamps + WLC::n_extra_fields + 2 + k);
                uint32_t _distance_idx = std::min(
                        uint32_t(sample_timestamp - meta._past_timestamp) / WLC::edc_windows[k],
                        WLC::max_hash_edc_idx);
                data.push_back(WLC::hash_edc[_distance_idx]);
            }
        }

        counter += WLC::n_edc_feature;

        labels.push_back(log1p(future_interval));
        indptr.push_back(counter);


#ifdef EVICTION_LOGGING
        if (WLC::start_train_logging) {
//            training_and_prediction_logic_timestamps.emplace_back(current_t / 65536);
            int i = indptr.size() - 2;
            int current_idx = indptr[i];
            for (int p = 0; p < WLC::n_feature; ++p) {
                if (p == indices[current_idx]) {
                    WLC::trainings_and_predictions.emplace_back(data[current_idx]);
                    if (current_idx + 1 < indptr[i + 1])
                        ++current_idx;
                } else
                    WLC::trainings_and_predictions.emplace_back(NAN);
            }
            WLC::trainings_and_predictions.emplace_back(future_interval);
            WLC::trainings_and_predictions.emplace_back(NAN);
            WLC::trainings_and_predictions.emplace_back(sample_timestamp);
            WLC::trainings_and_predictions.emplace_back(2);
            WLC::trainings_and_predictions.emplace_back(key);
        }
#endif

    }

    void clear() {
        labels.clear();
        indptr.resize(1);
        indices.clear();
        data.clear();
    }
};


struct KeyMapEntryT {
    unsigned int list_idx: 1;
    unsigned int list_pos: 31;
};

class WLCCache : public Cache {
public:
    //key -> (0/1 list, idx)
    sparse_hash_map<uint64_t, KeyMapEntryT> key_map;
//    vector<WLCMeta> meta_holder[2];
    vector<InCacheMeta> in_cache_metas;
    vector<WLCMeta> out_cache_metas;

    InCacheLRUQueue in_cache_lru_queue;
    //TODO: negative queue should have a better abstraction, at least hide the round-up
    sparse_hash_map<uint32_t, uint64_t> negative_candidate_queue;
    WLCTrainingData *training_data;
    WLCEvictionTrainingData *eviction_training_data;
    uint32_t current_t;

    // sample_size
    uint sample_rate = 64;
    unsigned int segment_window = 1000000;

    double training_loss = 0;
    uint64_t n_force_eviction = 0;

    double training_time = 0;
    double inference_time = 0;

    BoosterHandle booster = nullptr;

    unordered_map<string, string> training_params = {
            //don't use alias here. C api may not recongize
            {"boosting",         "gbdt"},
            {"objective",        "regression"},
            {"num_iterations",   "32"},
            {"num_leaves",       "32"},
            {"num_threads",      "4"},
            {"feature_fraction", "0.8"},
            {"bagging_freq",     "5"},
            {"bagging_fraction", "0.8"},
            {"learning_rate",    "0.1"},
            {"verbosity",        "0"},
    };

    unordered_map<string, string> inference_params;

    enum ObjectiveT : uint8_t {
        byte_miss_ratio = 0, object_miss_ratio = 1
    };
    ObjectiveT objective = byte_miss_ratio;

    default_random_engine _generator = default_random_engine();
    uniform_int_distribution<std::size_t> _distribution = uniform_int_distribution<std::size_t>();

#ifdef EVICTION_LOGGING
    vector<uint8_t> eviction_qualities;
    vector<uint16_t> eviction_logic_timestamps;
    uint64_t byte_million_req;
    uint32_t n_req;
    int64_t n_early_stop = -1;
//    vector<uint16_t> training_and_prediction_logic_timestamps;
    string task_id;
    string dburl;
    uint64_t belady_boundary;
    vector<int64_t> near_bytes;
    vector<int64_t> middle_bytes;
    vector<int64_t> far_bytes;
#endif

    void init_with_params(const map<string, string> &params) override {
        //set params
        for (auto &it: params) {
            if (it.first == "sample_rate") {
                sample_rate = stoul(it.second);
            } else if (it.first == "memory_window") {
                WLC::memory_window = stoull(it.second);
            } else if (it.first == "max_n_past_timestamps") {
                WLC::max_n_past_timestamps = (uint8_t) stoi(it.second);
            } else if (it.first == "batch_size") {
                WLC::batch_size = stoull(it.second);
            } else if (it.first == "n_extra_fields") {
                WLC::n_extra_fields = stoull(it.second);
            } else if (it.first == "num_iterations") {
                training_params["num_iterations"] = it.second;
            } else if (it.first == "learning_rate") {
                training_params["learning_rate"] = it.second;
            } else if (it.first == "num_threads") {
                training_params["num_threads"] = it.second;
            } else if (it.first == "num_leaves") {
                training_params["num_leaves"] = it.second;
#ifdef EVICTION_LOGGING
            } else if (it.first == "n_early_stop") {
                n_early_stop = stoll((it.second));
            } else if (it.first == "n_req") {
                n_req = stoull(it.second);
            } else if (it.first == "byte_million_req") {
                byte_million_req = stoull(it.second);
            } else if (it.first == "dburl") {
                dburl = it.second;
            } else if (it.first == "task_id") {
                task_id = it.second;
            } else if (it.first == "belady_boundary") {
                belady_boundary = stoll(it.second);
#endif
            } else if (it.first == "n_edc_feature") {
                if (stoull(it.second) != WLC::n_edc_feature) {
                    cerr << "error: cannot change n_edc_feature because of const" << endl;
                    abort();
                }
//                WLC::n_edc_feature = stoull(it.second);
            } else if (it.first == "objective") {
                if (it.second == "byte_miss_ratio")
                    objective = byte_miss_ratio;
                else if (it.second == "object_miss_ratio")
                    objective = object_miss_ratio;
                else {
                    cerr << "error: unknown objective" << endl;
                    exit(-1);
                }
            } else if (it.first == "segment_window") {
                segment_window = stoul(it.second);
            } else {
                cerr << "WLC unrecognized parameter: " << it.first << endl;
            }
        }

        negative_candidate_queue.reserve(WLC::memory_window);
        WLC::max_n_past_distances = WLC::max_n_past_timestamps - 1;
        //init
        WLC::edc_windows = vector<uint32_t>(WLC::n_edc_feature);
        for (uint8_t i = 0; i < WLC::n_edc_feature; ++i) {
            WLC::edc_windows[i] = pow(2, WLC::base_edc_window + i);
        }
        WLC::max_hash_edc_idx = (uint64_t) (WLC::memory_window / pow(2, WLC::base_edc_window)) - 1;
        WLC::hash_edc = vector<double>(WLC::max_hash_edc_idx + 1);
        for (int i = 0; i < WLC::hash_edc.size(); ++i)
            WLC::hash_edc[i] = pow(0.5, i);

        //interval, distances, size, extra_features, n_past_intervals, edwt
        WLC::n_feature = WLC::max_n_past_timestamps + WLC::n_extra_fields + 2 + WLC::n_edc_feature;
        if (WLC::n_extra_fields) {
            if (WLC::n_extra_fields > WLC::max_n_extra_feature) {
                cerr << "error: only support <= " + to_string(WLC::max_n_extra_feature)
                        + " extra fields because of static allocation" << endl;
                abort();
            }
            string categorical_feature = to_string(WLC::max_n_past_timestamps + 1);
            for (uint i = 0; i < WLC::n_extra_fields - 1; ++i) {
                categorical_feature += "," + to_string(WLC::max_n_past_timestamps + 2 + i);
            }
            training_params["categorical_feature"] = categorical_feature;
        }
        inference_params = training_params;
        //can set number of threads, however the inference time will increase a lot (2x~3x) if use 1 thread
//        inference_params["num_threads"] = "4";
        training_data = new WLCTrainingData();
        eviction_training_data = new WLCEvictionTrainingData();

#ifdef EVICTION_LOGGING
        //logging the training and inference happened in the last 1 million
        if (n_early_stop < 0) {
            WLC::n_logging_start = n_req - 20000000;
        } else {
            WLC::n_logging_start = n_early_stop - 20000000;
        }
#endif
    }

    bool lookup(SimpleRequest &req);

    void admit(SimpleRequest &req);

    void evict();

    void forget();

    //sample, rank the 1st and return
    pair<uint64_t, uint32_t> rank();

    void train();

    void sample();

    void print_stats();

    bool has(const uint64_t &id) {
        auto it = key_map.find(id);
        if (it == key_map.end())
            return false;
        return !it->second.list_idx;
    }

#ifdef EVICTION_LOGGING

    void update_stat_periodic() override {
        int64_t near_byte = 0, middle_byte = 0, far_byte = 0;
        for (auto &i: in_cache_metas) {
            if (i._future_timestamp == 0xffffffff) {
                far_byte += i._size;
            } else if (i._future_timestamp - current_t > belady_boundary) {
                middle_byte += i._size;
            } else {
                near_byte += i._size;
            }
        }
        near_bytes.emplace_back(near_byte);
        middle_bytes.emplace_back(middle_byte);
        far_bytes.emplace_back(far_byte);
    }

#endif

    void update_stat(bsoncxx::v_noabi::builder::basic::document &doc) override {
        uint64_t feature_overhead = 0;
        uint64_t sample_overhead = 0;
        for (auto &m: in_cache_metas) {
            feature_overhead += m.feature_overhead();
            sample_overhead += m.sample_overhead();
        }
        for (auto &m: out_cache_metas) {
            feature_overhead += m.feature_overhead();
            sample_overhead += m.sample_overhead();
        }

        doc.append(kvp("n_metadata", to_string(key_map.size())));
        doc.append(kvp("feature_overhead", to_string(feature_overhead)));
        doc.append(kvp("sample ", to_string(sample_overhead)));
        doc.append(kvp("n_force_eviction", to_string(n_force_eviction)));

        int res;
        auto importances = vector<double>(WLC::n_feature, 0);

        if (booster) {
            res = LGBM_BoosterFeatureImportance(booster,
                                                0,
                                                1,
                                                importances.data());
            if (res == -1) {
                cerr << "error: get model importance fail" << endl;
                abort();
            }
        }

        doc.append(kvp("model_importance", [importances](sub_array child) {
            for (const auto &element : importances)
                child.append(element);
        }));


#ifdef EVICTION_LOGGING
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
        try {
            mongocxx::client client = mongocxx::client{mongocxx::uri(dburl)};
            mongocxx::database db = client["webcachesim"];
            auto bucket = db.gridfs_bucket();

            auto uploader = bucket.open_upload_stream(task_id + ".evictions");
            for (auto &b: eviction_qualities)
                uploader.write((uint8_t *) (&b), sizeof(uint8_t));
            uploader.close();
            uploader = bucket.open_upload_stream(task_id + ".eviction_timestamps");
            for (auto &b: eviction_logic_timestamps)
                uploader.write((uint8_t *) (&b), sizeof(uint16_t));
            uploader.close();
            uploader = bucket.open_upload_stream(task_id + ".trainings_and_predictions");
            for (auto &b: WLC::trainings_and_predictions)
                uploader.write((uint8_t *) (&b), sizeof(float));
            uploader.close();
//            uploader = bucket.open_upload_stream(task_id + ".training_and_prediction_timestamps");
//            for (auto &b: training_and_prediction_logic_timestamps)
//                uploader.write((uint8_t *) (&b), sizeof(uint16_t));
//            uploader.close();
        } catch (const std::exception &xcp) {
            cerr << "error: db connection failed: " << xcp.what() << std::endl;
            //continue to upload the simulation summaries
//            abort();
        }
#endif
    }

};

static Factory<WLCCache> factoryWLC("WLC");
#endif //WEBCACHESIM_WLC_H
