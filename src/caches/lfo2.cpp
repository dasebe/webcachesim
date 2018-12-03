//
// Created by zhenyus on 11/8/18.
//

#include "request.h"
#include "lfo2.h"
#include <chrono>
#include <cmath>

using namespace std;
using namespace chrono;


unordered_map<string, string> LFO2_train_params = {
            {"boosting",                   "gbdt"},
            {"objective",                  "binary"},
            {"metric",                     "binary_logloss,auc"},
            {"metric_freq",                "1"},
            {"is_provide_training_metric", "true"},
            {"max_bin",                    "255"},
            {"num_iterations",             "50"},
            {"learning_rate",              "0.1"},
            {"num_leaves",                 "31"},
            {"tree_learner",               "serial"},
            {"num_threads",                "40"},
            {"feature_fraction",           "0.8"},
            {"bagging_freq",               "5"},
            {"bagging_fraction",           "0.8"},
            {"min_data_in_leaf",           "50"},
            {"min_sum_hessian_in_leaf",    "5.0"},
            {"is_enable_sparse",           "true"},
            {"two_round",                  "false"},
            {"save_binary",                "false"}
};


bool LFOACache::lookup(SimpleRequest& _req) {
    AnnotatedRequest & req = static_cast<AnnotatedRequest&>(_req);

    uint64_t & key = _req._id;
    uint64_t & t = req._t;
    uint64_t & next_t = req._next_t;
    //record timestamp and intervals
    {
        auto it_past_timestamp = past_timestamp.find(key);
        if (it_past_timestamp == past_timestamp.end()) {
            //add timestamp
            past_timestamp.insert({key, t});
            past_intervals.insert({key, list<uint64_t >()});
        } else {
            //update interval
            auto & it_interval = past_intervals.find(key)->second;
            if (it_interval.size() > MAX_N_INTERVAL)
                it_interval.pop_back();
            it_interval.push_front(t - it_past_timestamp->second);
            it_past_timestamp->second = t;
        }

        auto it_future_timestamp = future_timestamp.find(key);
        if (it_future_timestamp == future_timestamp.end())
            future_timestamp.insert({key, next_t});
        else
            it_future_timestamp->second = next_t;
    }


    auto it = _cacheMap.left.find(key);
    if (it != _cacheMap.left.end()) {
        // log hit
        _cacheMap.left.replace_data(it, next_t);
        return true;
    }
    return false;
}

void LFOACache::admit(SimpleRequest& _req) {
    AnnotatedRequest & req = static_cast<AnnotatedRequest&>(_req);

    const uint64_t & size = req._size;
    // object feasible to store?
    if (size > _cacheSize) {
        LOG("L", _cacheSize, req.get_id(), size);
        return;
    }

    // admit new object
    _cacheMap.insert({req._id, req._next_t});
    auto it = object_size.find(req._id);
    if (it == object_size.end())
        object_size.insert({req._id, req._size});
    else
        it->second = size;
    _currentSize += size;

    // check eviction needed
    while (_currentSize > _cacheSize) {
        evict(req._t);
    }
}

void LFOACache::evict(uint64_t &t) {
    auto right_iter = _cacheMap.right.begin();

    //sample
    {
        //key0 is evict (label 0)
        const uint64_t & key0 = right_iter->second;
        labels.push_back(0);
        auto it_intervals = past_intervals.find(key0);
        uint32_t idx = 0;
        if (it_intervals != past_intervals.end()) {
            for (const auto & it_interval: it_intervals->second) {
                indices.push_back(idx);
                data.push_back(it_interval);
                ++idx;
            }
        }
        indices.push_back(MAX_N_INTERVAL);
        data.push_back(t-past_timestamp.find(key0)->second);

        indices.push_back(MAX_N_INTERVAL+1);
        data.push_back(object_size.find(key0)->second);

        //remove future t
        indptr.push_back(indptr[indptr.size() - 1] + idx + 2);
        //add future t
//        indices.push_back(MAX_N_INTERVAL+2);
//        data.push_back(future_timestamp.find(key0)->second-t);
//        indptr.push_back(indptr[indptr.size() - 1] + idx + 3);


        //key 1 is the exact key to keep
        auto it = _cacheMap.right.begin();
        while (it != _cacheMap.right.end() && it->first == right_iter->first) {
            ++it;
        }
        if (it != _cacheMap.right.end()) {
            const uint64_t & key1 = it->second;
            labels.push_back(1);
            auto it_intervals = past_intervals.find(key1);
            uint32_t idx = 0;
            if (it_intervals != past_intervals.end()) {
                for (const auto & it_interval: it_intervals->second) {
                    indices.push_back(idx);
                    data.push_back(it_interval);
                    ++idx;
                }
            }
            indices.push_back(MAX_N_INTERVAL);
            data.push_back(t-past_timestamp.find(key1)->second);

            indices.push_back(MAX_N_INTERVAL+1);
            data.push_back(object_size.find(key1)->second);

            //remove future t
            indptr.push_back(indptr[indptr.size() - 1] + idx + 2);

            //add future t
//            indices.push_back(MAX_N_INTERVAL+2);
//            data.push_back(future_timestamp.find(key1)->second-t);
//            indptr.push_back(indptr[indptr.size() - 1] + idx + 3);
        }
    }

    _currentSize -= object_size.find(right_iter->second)->second;
    _cacheMap.right.erase(right_iter);
}

void LFOACache::train() {
    auto timeBegin = chrono::system_clock::now();
    LGBM_BoosterFree(booster);
    // create training dataset
    DatasetHandle trainData;
    LGBM_DatasetCreateFromCSR(
            static_cast<void *>(indptr.data()),
            C_API_DTYPE_INT32,
            indices.data(),
            static_cast<void *>(data.data()),
            C_API_DTYPE_FLOAT64,
            indptr.size(),
            data.size(),
            MAX_N_INTERVAL + 2,  //remove future t
//            MAX_N_INTERVAL + 3,  //add future t
            LFO2_train_params,
            nullptr,
            &trainData);

    LGBM_DatasetSetField(trainData,
            "label",
            static_cast<void *>(labels.data()),
            labels.size(),
            C_API_DTYPE_FLOAT32);

    // init booster
    LGBM_BoosterCreate(trainData, LFO2_train_params, &booster);
    // train
    for (int i = 0; i < stoi(LFO2_train_params["num_iterations"]); i++) {
      int isFinished;
      LGBM_BoosterUpdateOneIter(booster, &isFinished);
      if (isFinished) {
        break;
      }
    }

    int64_t len;
    vector<double > result(indptr.size()-1);
    LGBM_BoosterPredictForCSR(booster,
            static_cast<void *>(indptr.data()),
            C_API_DTYPE_INT32,
            indices.data(),
            static_cast<void *>(data.data()),
            C_API_DTYPE_FLOAT64,
            indptr.size(),
            data.size(),
            MAX_N_INTERVAL + 2,  //remove future t
//            MAX_N_INTERVAL + 3,  //add future t
            C_API_PREDICT_NORMAL,
            0,
            LFO2_train_params,
            &len,
            result.data());
    int total = 0, correct = 0;
    for (int i = 0; i < result.size(); ++i) {
        if ((result[i] >= 0.5) == labels[i])
            ++ correct;
        ++total;
    }
    cerr<<"training accuracy: "<<double(correct)/total<<endl;

    vector<double > importance(MAX_N_INTERVAL+2);
    int succeed = LGBM_BoosterFeatureImportance(booster,
                                            0,
                                            1,
                                            importance.data());
    cerr<<"\nimportance: ";
    for (auto & it: importance) {
        cerr<<it<<endl;
    }

    LGBM_DatasetFree(trainData);
    labels.clear();
    indptr.clear();
    indptr.push_back(0);
    indices.clear();
    data.clear();
}

bool LFOBCache::lookup(SimpleRequest& _req) {
    auto & req = static_cast<AnnotatedRequest &>(_req);
    auto & key = _req._id;
    auto & t = req._t;
    auto & next_t = req._next_t;


    auto it = key_map.find(key);
    if (it != key_map.end()) {
        //update meta
        bool & list_idx = it->second.first;
        uint32_t & pos_idx = it->second.second;
        meta_holder[list_idx][pos_idx].update(t, next_t);

        return !list_idx;
    }
    return false;
}

void LFOBCache::admit(SimpleRequest& _req) {
    AnnotatedRequest & req = static_cast<AnnotatedRequest &>(_req);
    const uint64_t & size = req._size;
    // object feasible to store?
    if (size > _cacheSize) {
        LOG("L", _cacheSize, req.get_id(), size);
        return;
    }

    auto it = key_map.find(req._id);
    if (it == key_map.end()) {
        //fresh insert
        key_map.insert({req._id, {0, (uint32_t) meta_holder[0].size()}});
        meta_holder[0].emplace_back(req._id, req._size, req._t, req._next_t);
        _currentSize += size;
        if (_currentSize <= _cacheSize)
            return;
    } else if (size + _currentSize <= _cacheSize){
        //bring list 1 to list 0
        //first move meta data, then modify hash table
        uint32_t tail0_pos = meta_holder[0].size();
        meta_holder[0].emplace_back(meta_holder[1][it->second.second]);
        uint32_t tail1_pos = meta_holder[1].size()-1;
        if (it->second.second !=  tail1_pos) {
            //swap tail
            meta_holder[1][it->second.second] = meta_holder[1][tail1_pos];
            key_map.find(meta_holder[1][tail1_pos]._key)->second.second = it->second.second;
        }
        meta_holder[1].pop_back();
        it->second = {0, tail0_pos};
        _currentSize += size;
        return;
    } else {
        //insert-evict
        auto epair = rank(req._t);
        auto & key0 = epair.first;
        auto & pos0 = epair.second;
        auto & pos1 = it->second.second;
        _currentSize = _currentSize - meta_holder[0][pos0]._size + req._size;
        swap(meta_holder[0][pos0], meta_holder[1][pos1]);
        swap(it->second, key_map.find(key0)->second);
    }
    // check more eviction needed?
    while (_currentSize > _cacheSize) {
        evict(req._t);
    }
}


pair<uint64_t, uint32_t> LFOBCache::rank(const uint64_t & t) {
    uint32_t rand_idx = _distribution(_generator) % meta_holder[0].size();
    uint8_t n_sample;
    if (sample_rate < meta_holder[0].size())
        n_sample = sample_rate;
    else
        n_sample = meta_holder[0].size();


    vector<float> labels;
    vector<int32_t> indptr = {0};
    vector<int32_t> indices;
    vector<double> data;

    for (uint32_t i = 0; i < n_sample; i++) {
        uint32_t pos = (i+rand_idx)%meta_holder[0].size();
        auto & meta = meta_holder[0][pos];
        //fill in past_interval
        uint8_t j = 0;
        for (; j < meta._past_interval_idx && j < MAX_N_INTERVAL; ++j) {
            uint8_t past_timestamp_idx = (meta._past_interval_idx - 1 - j) % MAX_N_INTERVAL;
            indices.push_back(j);
            data.push_back(meta._past_intervals[past_timestamp_idx]);
        }
        indices.push_back(MAX_N_INTERVAL);
        data.push_back(t-meta._past_timestamp);

        indices.push_back(MAX_N_INTERVAL+1);
        data.push_back(meta._size);

        //remove future t
        indptr.push_back(indptr[indptr.size() - 1] + j + 2);

        //add future t
//        indices.push_back(MAX_N_INTERVAL+2);
//        data.push_back(meta._future_timestamp-t);
//        indptr.push_back(indptr[indptr.size() - 1] + j + 3);
    }
    int64_t len;
    vector<double> result;
    result.resize(indptr.size() - 1);
    LGBM_BoosterPredictForCSR(booster,
            static_cast<void *>(indptr.data()),
            C_API_DTYPE_INT32,
            indices.data(),
            static_cast<void *>(data.data()),
            C_API_DTYPE_FLOAT64,
            indptr.size(),
            data.size(),
            MAX_N_INTERVAL + 2,  //remove future t
//            MAX_N_INTERVAL + 3,  //add future t
            C_API_PREDICT_NORMAL,
            0,
            LFO2_train_params,
            &len,
            result.data());

    auto min_it = min_element(result.begin(), result.end());
    auto sample_pos = distance(result.begin(), min_it);
    uint32_t min_pos = (sample_pos+rand_idx)%meta_holder[0].size();
    auto & meta = meta_holder[0][min_pos];
    auto & min_key = meta._key;

    return {min_key, min_pos};
}

void LFOBCache::evict(const uint64_t & t) {
    auto epair = rank(t);
    uint64_t & key = epair.first;
    uint32_t & old_pos = epair.second;

    //bring list 0 to list 1
    uint32_t new_pos = meta_holder[1].size();

    meta_holder[1].emplace_back(meta_holder[0][old_pos]);
    uint32_t activate_tail_idx = meta_holder[0].size()-1;
    if (old_pos !=  activate_tail_idx) {
        //move tail
        meta_holder[0][old_pos] = meta_holder[0][activate_tail_idx];
        key_map.find(meta_holder[0][activate_tail_idx]._key)->second.second = old_pos;
    }
    meta_holder[0].pop_back();

    auto it = key_map.find(key);
    it->second.first = 1;
    it->second.second = new_pos;
    _currentSize -= meta_holder[1][new_pos]._size;
}