//
// Created by zhenyus on 11/8/18.
//

#include "request.h"
#include "lfo2.h"
#include <chrono>

using namespace std;
using namespace chrono;





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
        data.push_back(future_timestamp.find(key0)->second-t);

        indices.push_back(MAX_N_INTERVAL+2);
        data.push_back(object_size.find(key0)->second);

        indptr.push_back(indptr[indptr.size() - 1] + idx + 3);


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
            data.push_back(future_timestamp.find(key1)->second-t);

            indices.push_back(MAX_N_INTERVAL+2);
            data.push_back(object_size.find(key1)->second);

            indptr.push_back(indptr[indptr.size() - 1] + idx + 3);
        }
    }

    _currentSize -= object_size.find(right_iter->second)->second;
    _cacheMap.right.erase(right_iter);
}

void LFOACache::train() {
    unordered_map<string, string> trainParams = {
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

    auto timeBegin = chrono::system_clock::now();
    LGBM_BoosterFree(booster);
    // create training dataset
    DatasetHandle trainData;
    LGBM_DatasetCreateFromCSR(static_cast<void *>(indptr.data()), C_API_DTYPE_INT32, indices.data(),
                                static_cast<void *>(data.data()), C_API_DTYPE_FLOAT64,
                                indptr.size(), data.size(), MAX_N_INTERVAL + 3,
                                trainParams, nullptr, &trainData);
    LGBM_DatasetSetField(trainData, "label", static_cast<void *>(labels.data()), labels.size(), C_API_DTYPE_FLOAT32);

    // init booster
    LGBM_BoosterCreate(trainData, trainParams, &booster);
    // train
    for (int i = 0; i < stoi(trainParams["num_iterations"]); i++) {
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
            MAX_N_INTERVAL+3,
            C_API_PREDICT_NORMAL,
            0,
            trainParams,
            &len,
            result.data());
    int total = 0, correct = 0;
    for (int i = 0; i < result.size(); ++i) {
        if ((result[i] >= 0.5) == labels[i])
            ++ correct;
        ++total;
    }
    cerr<<"training accuracy: "<<double(correct)/total<<endl;

    LGBM_DatasetFree(trainData);
}

bool LFOBCache::lookup(SimpleRequest& _req) {
//    auto & req = static_cast<ClassifiedRequest&>(_req);
//    auto it = _cacheMap.left.find(std::make_pair(req.get_id(), req.get_size()));
//    if (it != _cacheMap.left.end()) {
//        // log hit
//        _cacheMap.left.replace_data(it, (req.rehit_probability));
//        return true;
//    }
    return false;
}

void LFOBCache::admit(SimpleRequest& _req) {
//    auto & req = static_cast<ClassifiedRequest&>(_req);
//
//    const uint64_t size = req.get_size();
//    // object feasible to store?
//    if (size > _cacheSize) {
//        LOG("L", _cacheSize, req.get_id(), size);
//        return;
//    }
//
//    // admit new object
//    _cacheMap.insert({{req.get_id(), req.get_size()}, req.rehit_probability});
//    _currentSize += size;
//
//    // check eviction needed
//    while (_currentSize > _cacheSize) {
//        evict();
//    }
}

void LFOBCache::evict() {
//    auto right_iter = _cacheMap.right.begin();
//    _currentSize -= right_iter->second.second;
//    _cacheMap.right.erase(right_iter);
}
