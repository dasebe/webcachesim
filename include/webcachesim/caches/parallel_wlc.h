//
// Created by zhenyus on 1/16/19.
//

#ifndef WEBCACHESIM_PARALLEL_WLC_H
#define WEBCACHESIM_PARALLEL_WLC_H


#include "parallel_cache.h"
#include <atomic>
#include <unordered_map>
#include <vector>
#include <string>
#include <chrono>
#include <random>
#include <assert.h>
#include <LightGBM/c_api.h>
#include <mutex>
#include <thread>
#include <queue>
#include <shared_mutex>
#include <list>
#include "sparsepp/spp.h"

using namespace webcachesim;
using namespace std;
using spp::sparse_hash_map;
typedef uint64_t WLCKey;
using bsoncxx::builder::basic::kvp;
using bsoncxx::builder::basic::sub_array;

namespace ParallelWLC {
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
}

struct ParalllelWLCMetaExtra {
    //164 byte at most
    //not 1 hit wonder
    float _edc[10];
    vector<uint32_t> _past_distances;
    //the next index to put the distance
    uint8_t _past_distance_idx = 1;

    ParalllelWLCMetaExtra(const uint32_t &distance) {
        _past_distances = vector<uint32_t>(1, distance);
        for (uint8_t i = 0; i < ParallelWLC::n_edc_feature; ++i) {
            uint32_t _distance_idx = min(uint32_t(distance / ParallelWLC::edc_windows[i]),
                                         ParallelWLC::max_hash_edc_idx);
            _edc[i] = ParallelWLC::hash_edc[_distance_idx] + 1;
        }
    }

    void update(const uint32_t &distance) {
        uint8_t distance_idx = _past_distance_idx % ParallelWLC::max_n_past_distances;
        if (_past_distances.size() < ParallelWLC::max_n_past_distances)
            _past_distances.emplace_back(distance);
        else
            _past_distances[distance_idx] = distance;
        assert(_past_distances.size() <= ParallelWLC::max_n_past_distances);
        _past_distance_idx = _past_distance_idx + (uint8_t) 1;
        if (_past_distance_idx >= ParallelWLC::max_n_past_distances * 2)
            _past_distance_idx -= ParallelWLC::max_n_past_distances;
        for (uint8_t i = 0; i < ParallelWLC::n_edc_feature; ++i) {
            uint32_t _distance_idx = min(uint32_t(distance / ParallelWLC::edc_windows[i]),
                                         ParallelWLC::max_hash_edc_idx);
            _edc[i] = _edc[i] * ParallelWLC::hash_edc[_distance_idx] + 1;
        }
    }
};

class ParallelWLCMeta {
public:
    //25 byte
    uint64_t _key;
    uint32_t _size;
    uint32_t _past_timestamp;
    uint16_t _extra_features[ParallelWLC::max_n_extra_feature];
    ParalllelWLCMetaExtra *_extra = nullptr;
    vector<uint32_t> _sample_times;


    ParallelWLCMeta(const uint64_t &key, const uint64_t &size, const uint64_t &past_timestamp,
                    const uint16_t *&extra_features) {
        _key = key;
        _size = size;
        _past_timestamp = past_timestamp;
        for (int i = 0; i < ParallelWLC::n_extra_fields; ++i)
            _extra_features[i] = extra_features[i];
    }

    virtual ~ParallelWLCMeta() = default;

    void emplace_sample(uint32_t &sample_t) {
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
            _extra = new ParalllelWLCMetaExtra(_distance);
        } else
            _extra->update(_distance);
        //timestamp
        _past_timestamp = past_timestamp;
    }

    int feature_overhead() {
        int ret = sizeof(ParallelWLCMeta);
        if (_extra)
            ret += sizeof(ParalllelWLCMetaExtra) - sizeof(_sample_times) +
                   _extra->_past_distances.capacity() * sizeof(uint32_t);
        return ret;
    }

    int sample_overhead() {
        return sizeof(_sample_times) + sizeof(uint32_t) * _sample_times.capacity();
    }
};


class ParallelInCacheMeta : public ParallelWLCMeta {
public:
    //pointer to lru0
    list<WLCKey>::const_iterator p_last_request;
    //any change to functions?

    ParallelInCacheMeta(const uint64_t &key,
                        const uint64_t &size,
                        const uint64_t &past_timestamp,
                        const uint16_t *&extra_features, const list<WLCKey>::const_iterator &it) :
            ParallelWLCMeta(key, size, past_timestamp, extra_features) {
        p_last_request = it;
    };

    ParallelInCacheMeta(const ParallelWLCMeta &meta, const list<WLCKey>::const_iterator &it) : ParallelWLCMeta(meta) {
        p_last_request = it;
    };

};

class ParallelInCacheLRUQueue {
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

class ParallelWLCTrainingData {
public:
    vector<float> labels;
    vector<int32_t> indptr;
    vector<int32_t> indices;
    vector<double> data;

    ParallelWLCTrainingData() {
        labels.reserve(ParallelWLC::batch_size);
        indptr.reserve(ParallelWLC::batch_size + 1);
        indptr.emplace_back(0);
        indices.reserve(ParallelWLC::batch_size * ParallelWLC::n_feature);
        data.reserve(ParallelWLC::batch_size * ParallelWLC::n_feature);
    }

    void emplace_back(ParallelWLCMeta &meta, uint32_t &sample_timestamp, uint32_t &future_interval) {
        int32_t counter = indptr.back();

        indices.emplace_back(0);
        data.emplace_back(sample_timestamp - meta._past_timestamp);
        ++counter;

        uint32_t this_past_distance = 0;
        int j = 0;
        uint8_t n_within = 0;
        if (meta._extra) {
            for (; j < meta._extra->_past_distance_idx && j < ParallelWLC::max_n_past_distances; ++j) {
                uint8_t past_distance_idx =
                        (meta._extra->_past_distance_idx - 1 - j) % ParallelWLC::max_n_past_distances;
                const uint32_t &past_distance = meta._extra->_past_distances[past_distance_idx];
                this_past_distance += past_distance;
                indices.emplace_back(j + 1);
                data.emplace_back(past_distance);
                if (this_past_distance < ParallelWLC::memory_window) {
                    ++n_within;
                }
            }
        }

        counter += j;

        indices.emplace_back(ParallelWLC::max_n_past_timestamps);
        data.push_back(meta._size);
        ++counter;

        for (int k = 0; k < ParallelWLC::n_extra_fields; ++k) {
            indices.push_back(ParallelWLC::max_n_past_timestamps + k + 1);
            data.push_back(meta._extra_features[k]);
        }
        counter += ParallelWLC::n_extra_fields;

        indices.push_back(ParallelWLC::max_n_past_timestamps + ParallelWLC::n_extra_fields + 1);
        data.push_back(n_within);
        ++counter;

        if (meta._extra) {
            for (int k = 0; k < ParallelWLC::n_edc_feature; ++k) {
                indices.push_back(ParallelWLC::max_n_past_timestamps + ParallelWLC::n_extra_fields + 2 + k);
                uint32_t _distance_idx = std::min(
                        uint32_t(sample_timestamp - meta._past_timestamp) / ParallelWLC::edc_windows[k],
                        ParallelWLC::max_hash_edc_idx);
                data.push_back(meta._extra->_edc[k] * ParallelWLC::hash_edc[_distance_idx]);
            }
        } else {
            for (int k = 0; k < ParallelWLC::n_edc_feature; ++k) {
                indices.push_back(ParallelWLC::max_n_past_timestamps + ParallelWLC::n_extra_fields + 2 + k);
                uint32_t _distance_idx = std::min(
                        uint32_t(sample_timestamp - meta._past_timestamp) / ParallelWLC::edc_windows[k],
                        ParallelWLC::max_hash_edc_idx);
                data.push_back(ParallelWLC::hash_edc[_distance_idx]);
            }
        }

        counter += ParallelWLC::n_edc_feature;

        labels.push_back(log1p(future_interval));
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

class ParallelWLCCache : public ParallelCache {
public:
    //key -> (0/1 list, idx)
    sparse_hash_map<uint64_t, KeyMapEntryT> key_map;
    vector<ParallelInCacheMeta> in_cache_metas;
    vector<ParallelWLCMeta> out_cache_metas;

    ParallelInCacheLRUQueue in_cache_lru_queue;
    //TODO: negative queue should have a better abstraction, at least hide the round-up
    sparse_hash_map<uint32_t, uint64_t> negative_candidate_queue;
    ParallelWLCTrainingData *training_data;
    ParallelWLCTrainingData *background_training_data;
    std::mutex training_data_mutex;

    // sample_size
    uint sample_rate = 64;
    uint64_t training_sample_interval = 64;
//    unsigned int segment_window = 1000000;
//
//    double training_loss = 0;

    //mutex guarantee the concurrency control, so counter doesn't need to be atomic
    uint32_t t_counter = 0;
    std::thread training_thread;

    BoosterHandle booster = nullptr;
    std::mutex booster_mutex;
    bool if_trained = false;

    unordered_map<string, string> WLC_train_params = {
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

    std::unordered_map<std::string, std::string> WLC_inference_params;

//    enum ObjectiveT: uint8_t {byte_hit_rate=0, object_hit_rate=1};
//    ObjectiveT objective = byte_hit_rate;

    std::default_random_engine _generator = std::default_random_engine();
    std::uniform_int_distribution<std::size_t> _distribution = std::uniform_int_distribution<std::size_t>();

    ~ParallelWLCCache() {
        keep_running.clear();
        if (lookup_get_thread.joinable())
            lookup_get_thread.join();
        if (training_thread.joinable())
            training_thread.join();
        if (print_status_thread.joinable())
            print_status_thread.join();
    }

    void init_with_params(const map<string, string> &params) override {
        //set params
        for (auto &it: params) {
            if (it.first == "sample_rate") {
                sample_rate = stoul(it.second);
            } else if (it.first == "memory_window") {
                ParallelWLC::memory_window = stoull(it.second);
            } else if (it.first == "max_n_past_timestamps") {
                ParallelWLC::max_n_past_timestamps = (uint8_t) stoi(it.second);
            } else if (it.first == "batch_size") {
                ParallelWLC::batch_size = stoull(it.second);
            } else if (it.first == "n_extra_fields") {
                ParallelWLC::n_extra_fields = stoull(it.second);
            } else if (it.first == "num_iterations") {
                WLC_train_params["num_iterations"] = it.second;
            } else if (it.first == "learning_rate") {
                WLC_train_params["learning_rate"] = it.second;
            } else if (it.first == "num_threads") {
                WLC_train_params["num_threads"] = it.second;
            } else if (it.first == "num_leaves") {
                WLC_train_params["num_leaves"] = it.second;
            } else if (it.first == "training_sample_interval") {
                training_sample_interval = stoull(it.second);
            } else if (it.first == "n_edc_feature") {
                if (stoull(it.second) != ParallelWLC::n_edc_feature) {
                    cerr << "error: cannot change n_edc_feature because of const" << endl;
                    abort();
                }
//                WLC::n_edc_feature = stoull(it.second);
//            } else if (it.first == "segment_window") {
//                segment_window = stoul(it.second);
            } else {
                cerr << "WLC unrecognized parameter: " << it.first << endl;
            }
        }

        negative_candidate_queue.reserve(ParallelWLC::memory_window);
        ParallelWLC::max_n_past_distances = ParallelWLC::max_n_past_timestamps - 1;
        //init
        ParallelWLC::edc_windows = vector<uint32_t>(ParallelWLC::n_edc_feature);
        for (uint8_t i = 0; i < ParallelWLC::n_edc_feature; ++i) {
            ParallelWLC::edc_windows[i] = pow(2, ParallelWLC::base_edc_window + i);
        }
        ParallelWLC::max_hash_edc_idx =
                (uint64_t) (ParallelWLC::memory_window / pow(2, ParallelWLC::base_edc_window)) - 1;
        ParallelWLC::hash_edc = vector<double>(ParallelWLC::max_hash_edc_idx + 1);
        for (int i = 0; i < ParallelWLC::hash_edc.size(); ++i)
            ParallelWLC::hash_edc[i] = pow(0.5, i);

        //interval, distances, size, extra_features, n_past_intervals, edwt
        ParallelWLC::n_feature =
                ParallelWLC::max_n_past_timestamps + ParallelWLC::n_extra_fields + 2 + ParallelWLC::n_edc_feature;
        if (ParallelWLC::n_extra_fields) {
            if (ParallelWLC::n_extra_fields > ParallelWLC::max_n_extra_feature) {
                cerr << "error: only support <= " + to_string(ParallelWLC::max_n_extra_feature)
                        + " extra fields because of static allocation" << endl;
                abort();
            }
            string categorical_feature = to_string(ParallelWLC::max_n_past_timestamps + 1);
            for (uint i = 0; i < ParallelWLC::n_extra_fields - 1; ++i) {
                categorical_feature += "," + to_string(ParallelWLC::max_n_past_timestamps + 2 + i);
            }
            WLC_train_params["categorical_feature"] = categorical_feature;
        }
        WLC_inference_params = WLC_train_params;
        training_data = new ParallelWLCTrainingData();
        background_training_data = new ParallelWLCTrainingData();
        WLC_inference_params = WLC_train_params;
        //TODO: don't believe inference need so large number of threads
//        WLC_inference_params["num_threads"] = "4";
        training_thread = std::thread(&ParallelWLCCache::async_training, this);
        ParallelCache::init_with_params(params);
    }


    void print_stats() override {
        std::cerr << "\nop queue length: " << op_queue.size() << std::endl;
        std::cerr << "async size_map len: " << size_map.size() << std::endl;
        std::cerr << "cache size: " << _currentSize << "/" << _cacheSize << " (" << ((double) _currentSize) / _cacheSize
                  << ")" << std::endl
                  << "in/out metadata " << in_cache_metas.size() << " / " << out_cache_metas.size() << std::endl;
//        std::cerr << "cache size: "<<_currentSize<<"/"<<_cacheSize<<std::endl;
//        std::cerr << "n_metadata: "<<key_map.size()<<std::endl;
        std::cerr << "n_training: " << training_data->labels.size() << std::endl;

//        std::cerr << "training loss: " << training_loss << std::endl;
//        std::cerr << "n_force_eviction: " << n_force_eviction <<std::endl;
//        std::cerr << "training time: " << training_time <<std::endl;
//        std::cerr << "inference time: " << inference_time <<std::endl;
    }

    void async_training() {
        while (true) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            if (training_data->labels.size() >= ParallelWLC::batch_size) {
                //assume back ground training data is already clear
                training_data_mutex.lock();
                std::swap(training_data, background_training_data);
                training_data_mutex.unlock();
                train();
                background_training_data->clear();
            }
//            printf("async training\n");
        }
    }

    void async_lookup(const uint64_t &key) override;

    void
    async_admit(const uint64_t &key, const int64_t &size, const uint16_t extra_features[max_n_extra_feature]) override;


    void evict();

    void forget();

    //sample, rank the 1st and return
    pair<uint64_t, uint32_t> rank();

    void train();

    void sample();

    bool has(const uint64_t &id) {
        auto it = key_map.find(id);
        if (it == key_map.end())
            return false;
        return !it->second.list_idx;
    }

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

        int res;
        auto importances = vector<double>(ParallelWLC::n_feature, 0);

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
    }

};

static Factory<ParallelWLCCache> factoryParallelWLC("ParallelWLC");
#endif //WEBCACHESIM_WLC_H
