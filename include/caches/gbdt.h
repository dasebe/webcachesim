//
// Created by zhenyus on 1/16/19.
//

#ifndef WEBCACHESIM_GDBT_H
#define WEBCACHESIM_GDBT_H

#include "cache.h"
#include <unordered_map>
#include "sparsepp/spp.h"
#include <vector>
#include <random>
#include <cmath>
#include <LightGBM/c_api.h>
#include <assert.h>

using namespace std;
using spp::sparse_hash_map;

namespace GDBT {
    uint8_t max_n_past_timestamps = 32;
    uint8_t max_n_past_distances = 31;
    uint8_t base_edwt_window = 10;
    const uint8_t n_edwt_feature = 10;
    vector<uint32_t > edwt_windows;
    vector<double > hash_edwt;
    uint32_t max_hash_edwt_idx;
    uint32_t forget_window = 80000000;
    uint32_t n_extra_fields = 0;
    uint32_t batch_size = 100000;
    uint32_t n_feature;
}

struct GDBTMetaExtra {
    //164 byte at most
    //not 1 hit wonder
    float _edwt[10];
    vector<uint32_t> _past_distances;
    //the next index to put the distance
    uint8_t _past_distance_idx = 1;

    GDBTMetaExtra(const uint32_t & distance) {
        _past_distances = vector<uint32_t>(1, distance);
        for (uint8_t i = 0; i < GDBT::n_edwt_feature; ++i) {
            uint32_t _distance_idx = min(uint32_t (distance/GDBT::edwt_windows[i]), GDBT::max_hash_edwt_idx);
            _edwt[i] = GDBT::hash_edwt[_distance_idx] + 1;
        }
    }

    void update(const uint32_t & distance) {
        uint8_t distance_idx = _past_distance_idx%GDBT::max_n_past_distances;
        if (_past_distances.size() < GDBT::max_n_past_distances)
            _past_distances.emplace_back(distance);
        else
            _past_distances[distance_idx] = distance;
        assert(_past_distances.size() <= GDBT::max_n_past_distances);
        _past_distance_idx = _past_distance_idx + (uint8_t) 1;
        if (_past_distance_idx >= GDBT::max_n_past_distances * 2)
            _past_distance_idx -= GDBT::max_n_past_distances;
        for (uint8_t i = 0; i < GDBT::n_edwt_feature; ++i) {
            uint32_t _distance_idx = min(uint32_t (distance/GDBT::edwt_windows[i]), GDBT::max_hash_edwt_idx);
            _edwt[i] = _edwt[i] * GDBT::hash_edwt[_distance_idx] + 1;
        }
    }

};

class GDBTMeta {
public:
    //25 byte
    uint64_t _key;
    uint32_t _size;
    uint32_t _past_timestamp;
    uint16_t _extra_features[2];
    GDBTMetaExtra * _extra = nullptr;
    vector<uint32_t> _sample_times;

    GDBTMeta(const uint64_t & key, const uint64_t & size, const uint64_t & past_timestamp,
            const vector<uint16_t> & extra_features) {
        _key = key;
        _size = size;
        _past_timestamp = past_timestamp;
        for (int i = 0; i < GDBT::n_extra_fields; ++i)
            _extra_features[i] = extra_features[i];
    }

    void emplace_sample(uint32_t & sample_t) {
        _sample_times.emplace_back(sample_t);
    }

    void free() {
        delete _extra;
    }

    void update(const uint32_t &past_timestamp) {
        //distance
        uint32_t _distance = past_timestamp - _past_timestamp;
        assert(_distance);
        if (!_extra) {
            _extra = new GDBTMetaExtra(_distance);
        } else
            _extra->update(_distance);
        //timestamp
        _past_timestamp = past_timestamp;
    }

    int feature_overhead() {
        int ret = sizeof(GDBTMeta);
        if (_extra)
            ret += sizeof(GDBTMetaExtra) - sizeof(_sample_times) + _extra->_past_distances.capacity()*sizeof(uint32_t);
        return ret;
    }

    int sample_overhead() {
        return sizeof(_sample_times) + sizeof(uint32_t) * _sample_times.capacity();
    }
};

class GDBTTrainingData {
public:
    vector<float> labels;
    vector<int32_t> indptr;
    vector<int32_t> indices;
    vector<double> data;
    GDBTTrainingData() {
        labels.reserve(GDBT::batch_size);
        indptr.reserve(GDBT::batch_size+1);
        indptr.emplace_back(0);
        indices.reserve(GDBT::batch_size*GDBT::n_feature);
        data.reserve(GDBT::batch_size*GDBT::n_feature);
    }

    void emplace_back(GDBTMeta &meta, uint32_t & sample_timestamp, uint32_t & future_interval) {
        int32_t counter = indptr.back();

        indices.emplace_back(0);
        data.emplace_back(sample_timestamp-meta._past_timestamp);
        ++counter;

        uint32_t this_past_distance = 0;
        int j = 0;
        uint8_t n_within = 0;
        if (meta._extra) {
            for (; j < meta._extra->_past_distance_idx && j < GDBT::max_n_past_distances; ++j) {
                uint8_t past_distance_idx = (meta._extra->_past_distance_idx - 1 - j) % GDBT::max_n_past_distances;
                const uint32_t & past_distance = meta._extra->_past_distances[past_distance_idx];
                this_past_distance += past_distance;
                    indices.emplace_back(j+1);
                    data.emplace_back(past_distance);
                if (this_past_distance < GDBT::forget_window) {
                    ++n_within;
                }
//                } else
//                    break;
            }
//            //garbage collection
//            if (n_within) {
//                if (n_within != meta._extra->_past_distances.size()) {
//                    uint8_t latest_idx = (meta._extra->_past_distance_idx - 1) % GDBT::max_n_past_distances;
//                    if (latest_idx == n_within - 1) {
//                        //no memcpy
//                    } else if (latest_idx > n_within - 1) {
//                        //one memcpy cpy
//                        memmove(meta._extra->_past_distances.data(),
//                                meta._extra->_past_distances.data()+latest_idx-n_within+1,
//                                sizeof(uint32_t)*n_within);
//                    } else {
//                        //rotate
//                        rotate(meta._extra->_past_distances.begin(),
//                                meta._extra->_past_distances.end()-(n_within-latest_idx-1),
//                                meta._extra->_past_distances.end());
//                    }
//                    meta._extra->_past_distances.resize(n_within);
//                    meta._extra->_past_distances.shrink_to_fit();
//                    meta._extra->_past_distance_idx = n_within;
//                } else {
//                    //all distances are fresh
//                }
//            } else {
//                //completely remove extra
//                delete meta._extra;
//                meta._extra = nullptr;
//            }
        }

        counter += j;

        indices.emplace_back(GDBT::max_n_past_timestamps);
        data.push_back(meta._size);
        ++counter;

        for (int k = 0; k < GDBT::n_extra_fields; ++k) {
            indices.push_back(GDBT::max_n_past_timestamps + k + 1);
            data.push_back(meta._extra_features[k]);
        }
        counter += GDBT::n_extra_fields;

        indices.push_back(GDBT::max_n_past_timestamps+GDBT::n_extra_fields+1);
        data.push_back(n_within);
        ++counter;

        if (meta._extra) {
            for (int k = 0; k < GDBT::n_edwt_feature; ++k) {
                indices.push_back(GDBT::max_n_past_timestamps + GDBT::n_extra_fields + 2 + k);
                uint32_t _distance_idx = std::min(
                        uint32_t(sample_timestamp - meta._past_timestamp) / GDBT::edwt_windows[k],
                        GDBT::max_hash_edwt_idx);
                data.push_back(meta._extra->_edwt[k] * GDBT::hash_edwt[_distance_idx]);
            }
        } else {
            for (int k = 0; k < GDBT::n_edwt_feature; ++k) {
                indices.push_back(GDBT::max_n_past_timestamps + GDBT::n_extra_fields + 2 + k);
                uint32_t _distance_idx = std::min(
                        uint32_t(sample_timestamp - meta._past_timestamp) / GDBT::edwt_windows[k],
                        GDBT::max_hash_edwt_idx);
                data.push_back(GDBT::hash_edwt[_distance_idx]);
            }
        }

        counter += GDBT::n_edwt_feature;

        labels.push_back(future_interval);
        indptr.push_back(counter);
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

class GDBTCache : public Cache
{
public:
    //key -> (0/1 list, idx)
    sparse_hash_map<uint64_t, KeyMapEntryT> key_map;
    vector<GDBTMeta> meta_holder[2];

    sparse_hash_map<uint32_t, uint64_t> forget_table;
    GDBTTrainingData * training_data;

    // sample_size
    uint sample_rate = 64;
    uint64_t training_sample_interval = 64;

    double training_loss = 0;
    uint64_t n_force_eviction = 0;

    BoosterHandle booster = nullptr;

    unordered_map<string, string> GDBT_train_params = {
            {"boosting",                   "gbdt"},
            {"objective",                  "regression"},
            {"num_iterations",             "32"},
            {"num_leaves",                  "32"},
            {"num_threads",                "4"},
            {"shrinkage_rate",           "0.1"},
            {"feature_fraction",           "0.8"},
            {"bagging_freq",               "5"},
            {"bagging_fraction",           "0.8"},
            {"learning_rate",              "0.1"},
    };

    unordered_map<string, string> GDBT_inference_params;

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
                GDBT::batch_size = stoull(it.second);
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
                if (stoull(it.second) != GDBT::n_edwt_feature) {
                    cerr<<"error: cannot change n_edwt_feature because of const"<<endl;
                    abort();
                }
//                GDBT::n_edwt_feature = stoull(it.second);
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

        forget_table.reserve(GDBT::forget_window);
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
        GDBT::n_feature = GDBT::max_n_past_timestamps + GDBT::n_extra_fields + 2 + GDBT::n_edwt_feature;
        if (GDBT::n_extra_fields) {
            if (GDBT::n_extra_fields>2) {
                cerr<<"error: only support <= 2 extra fields because of static allocation"<<endl;
                abort();
            }
            string categorical_feature = to_string(GDBT::max_n_past_timestamps+1);
            for (uint i = 0; i < GDBT::n_extra_fields-1; ++i) {
                categorical_feature += ","+to_string(GDBT::max_n_past_timestamps+2+i);
            }
            GDBT_train_params["categorical_feature"] = categorical_feature;
        }
        GDBT_inference_params = GDBT_train_params;
        training_data = new GDBTTrainingData();
    }

    bool lookup(SimpleRequest& req);
    void admit(SimpleRequest& req);
    void evict(const uint32_t t);
    void evict(SimpleRequest & req) {};
    void evict() {};
    void forget(uint32_t t);
    //sample, rank the 1st and return
    pair<uint64_t, uint32_t > rank(const uint32_t t);
    void train();
    void sample(uint32_t t);
    void print_stats();
    bool has(const uint64_t& id) {
        auto it = key_map.find(id);
        if (it == key_map.end())
            return false;
        return !it->second.list_idx;
    }

    void update_stat(std::map<std::string, std::string> &res) override {
        uint64_t feature_overhead = 0;
        uint64_t sample_overhead = 0;
        for (auto &m: meta_holder[0]) {
            feature_overhead += m.feature_overhead();
            sample_overhead += m.sample_overhead();
        }
        for (auto &m: meta_holder[1]) {
            feature_overhead += m.feature_overhead();
            sample_overhead += m.sample_overhead();
        }

        res["n_metadata"] = to_string(key_map.size());
        res["feature_overhead"] = to_string(feature_overhead);
        res["sample overhead"] = to_string(sample_overhead);
        res["n_force_eviction"] = to_string(n_force_eviction);
    }

};

static Factory<GDBTCache> factoryGBDT("GDBT");
#endif //WEBCACHESIM_GDBT_H
