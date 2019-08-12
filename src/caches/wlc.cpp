//
// Created by zhenyus on 1/16/19.
//

#include "wlc.h"
#include <algorithm>
#include "utils.h"
#include <chrono>

using namespace chrono;
using namespace std;

void WLCCache::train() {
    auto timeBegin = chrono::system_clock::now();
    if (booster)
        LGBM_BoosterFree(booster);
    // create training dataset
    DatasetHandle trainData;
    LGBM_DatasetCreateFromCSR(
            static_cast<void *>(training_data->indptr.data()),
            C_API_DTYPE_INT32,
            training_data->indices.data(),
            static_cast<void *>(training_data->data.data()),
            C_API_DTYPE_FLOAT64,
            training_data->indptr.size(),
            training_data->data.size(),
            WLC::n_feature,  //remove future t
            training_params,
            nullptr,
            &trainData);

    LGBM_DatasetSetField(trainData,
                         "label",
                         static_cast<void *>(training_data->labels.data()),
                         training_data->labels.size(),
                         C_API_DTYPE_FLOAT32);

    // init booster
    LGBM_BoosterCreate(trainData, training_params, &booster);
    // train
    for (int i = 0; i < stoi(training_params["num_iterations"]); i++) {
        int isFinished;
        LGBM_BoosterUpdateOneIter(booster, &isFinished);
        if (isFinished) {
            break;
        }
    }

    int64_t len;
    vector<double > result(training_data->indptr.size()-1);
    LGBM_BoosterPredictForCSR(booster,
                              static_cast<void *>(training_data->indptr.data()),
                              C_API_DTYPE_INT32,
                              training_data->indices.data(),
                              static_cast<void *>(training_data->data.data()),
                              C_API_DTYPE_FLOAT64,
                              training_data->indptr.size(),
                              training_data->data.size(),
                              WLC::n_feature,  //remove future t
                              C_API_PREDICT_NORMAL,
                              0,
                              training_params,
                              &len,
                              result.data());
    double se = 0;
    for (int i = 0; i < result.size(); ++i) {
        auto diff = result[i] - training_data->labels[i];
        se += diff * diff;
    }
    training_loss = training_loss * 0.99 + se/WLC::batch_size*0.01;

    LGBM_DatasetFree(trainData);
    training_time = 0.95 * training_time +
                    0.05 * chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - timeBegin).count();
}

void WLCCache::sample(uint32_t t) {
    // warmup not finish
    if (meta_holder[0].empty() || meta_holder[1].empty())
        return;
#ifdef LOG_SAMPLE_RATE
    bool log_flag = ((double) rand() / (RAND_MAX)) < LOG_SAMPLE_RATE;
#endif
    auto n_l0 = static_cast<uint32_t>(meta_holder[0].size());
    auto n_l1 = static_cast<uint32_t>(meta_holder[1].size());
    auto rand_idx = _distribution(_generator);
    // at least sample 1 from the list, at most size of the list
    auto n_sample_l0 = min(max(uint32_t (training_sample_interval*n_l0/(n_l0+n_l1)), (uint32_t) 1), n_l0);
    auto n_sample_l1 = min(max(uint32_t (training_sample_interval - n_sample_l0), (uint32_t) 1), n_l1);

    //sample list 0
    for (uint32_t i = 0; i < n_sample_l0; i++) {
        uint32_t pos = (uint32_t) (i + rand_idx) % n_l0;
        auto &meta = meta_holder[0][pos];
        meta.emplace_sample(t);
    }

    //sample list 1
    for (uint32_t i = 0; i < n_sample_l1; i++) {
        uint32_t pos = (uint32_t) (i + rand_idx) % n_l1;
        auto &meta = meta_holder[1][pos];
        meta.emplace_sample(t);
    }
}


void WLCCache::print_stats() {
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
    cerr
            << "cache size: " << _currentSize << "/" << _cacheSize << " (" << ((double) _currentSize) / _cacheSize
            << ")" << endl
            << "n_metadata: " << key_map.size() << endl
//    cerr << "feature overhead: "<<feature_overhead<<endl;
            << "feature overhead per entry: " << feature_overhead / key_map.size() << endl
//    cerr << "sample overhead: "<<sample_overhead<<endl;
            << "sample overhead per entry: " << sample_overhead / key_map.size() << endl
            << "n_training: " << training_data->labels.size() << endl
            << "training loss: " << training_loss << endl
            << "n_force_eviction: " << n_force_eviction << endl
            << "training_time: " << training_time << " ms" << endl
            << "inference_time: " << inference_time << " us" << endl;
    assert(meta_holder[0].size() + meta_holder[1].size() == key_map.size());
}


bool WLCCache::lookup(SimpleRequest &req) {
    bool ret;
    //piggy back
    if (req._t && !((req._t)%segment_window))
        print_stats();

    //first update the metadata: insert/update, which can trigger pending data.mature
    auto it = key_map.find(req._id);
    if (it != key_map.end()) {
        auto list_idx = it->second.list_idx;
        auto list_pos = it->second.list_pos;
        //update past timestamps
        WLCMeta &meta = meta_holder[list_idx][list_pos];
        assert(meta._key == req._id);
        uint64_t last_timestamp = meta._past_timestamp;
        uint64_t forget_timestamp = last_timestamp + WLC::memory_window;
        //if the key in key_map, it must also in forget table
        assert(negative_candidate_queue.find(forget_timestamp % WLC::memory_window) != negative_candidate_queue.end());
        //re-request
        if (!meta._sample_times.empty()) {
            //mature
            uint32_t future_distance = req._t - last_timestamp;
            for (auto & sample_time: meta._sample_times) {
                //don't use label within the first forget window because the data is not static
                training_data->emplace_back(meta, sample_time, future_distance);
                //training
                if (training_data->labels.size() == WLC::batch_size) {
                    train();
                    training_data->clear();
                }
            }
            meta._sample_times.clear();
            meta._sample_times.shrink_to_fit();
        }

        if ((req._t + WLC::memory_window - forget_timestamp)%WLC::memory_window) {
            //update
            //The else case is very rate, re-request at the end of memory window, and do not need modification or forget
            //make this update after update training, otherwise the last timestamp will change
            meta.update(req._t);
            //first forget, then insert. This prevent overwriting the older request a memory window before
            forget(req._t);
            negative_candidate_queue.erase(forget_timestamp % WLC::memory_window);
            negative_candidate_queue.insert({(req._t + WLC::memory_window) % WLC::memory_window, req._id});
            assert(negative_candidate_queue.find((req._t + WLC::memory_window)%WLC::memory_window) != negative_candidate_queue.end());
        } else {
            //make this update after update training, otherwise the last timestamp will change
            meta.update(req._t);
        }
        //update negative_candidate_queue
        ret = !list_idx;
    } else {
        ret = false;
        forget(req._t);
    }

    //sampling
    if (!(req._t % training_sample_interval))
        sample(req._t);
    return ret;
}


void WLCCache::forget(uint32_t t) {
    //remove item from forget table, which is not going to be affect from update
    auto it = negative_candidate_queue.find(t%WLC::memory_window);
    if (it != negative_candidate_queue.end()) {
        auto forget_key = it->second;
        auto meta_it = key_map.find(forget_key);
        auto pos = meta_it->second.list_pos;
        bool meta_id = meta_it->second.list_idx;
        auto &meta = meta_holder[meta_id][pos];

        //timeout mature
        if (!meta._sample_times.empty()) {
            //mature
            uint32_t future_distance = WLC::memory_window * 2;
            for (auto & sample_time: meta._sample_times) {
                //don't use label within the first forget window because the data is not static
                training_data->emplace_back(meta, sample_time, future_distance);
                //training
                if (training_data->labels.size() == WLC::batch_size) {
                    train();
                    training_data->clear();
                }
            }
            meta._sample_times.clear();
            meta._sample_times.shrink_to_fit();
        }

        assert(meta._key == forget_key);
        if (!meta_id) {
            ++n_force_eviction;
            _currentSize -= meta._size;
        }
        //free the actual content
        meta.free();
        //evict
        uint32_t tail_pos = meta_holder[meta_id].size() - 1;
        if (pos != tail_pos) {
            //swap tail
            meta_holder[meta_id][pos] = meta_holder[meta_id][tail_pos];
            key_map.find(meta_holder[meta_id][tail_pos]._key)->second.list_pos= pos;
        }
        meta_holder[meta_id].pop_back();
        key_map.erase(forget_key);
        negative_candidate_queue.erase(t%WLC::memory_window);
    }
}

void WLCCache::admit(SimpleRequest &req) {
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
        meta_holder[0].emplace_back(req._id, req._size, req._t, req._extra_features);
        _currentSize += size;
        //this must be a fresh insert
        negative_candidate_queue.insert({(req._t + WLC::memory_window)%WLC::memory_window, req._id});
        if (_currentSize <= _cacheSize)
            return;
    } else if (size + _currentSize <= _cacheSize){
        //bring list 1 to list 0
        //first move meta data, then modify hash table
        uint32_t tail0_pos = meta_holder[0].size();
        meta_holder[0].emplace_back(meta_holder[1][it->second.list_pos]);
        uint32_t tail1_pos = meta_holder[1].size()-1;
        if (it->second.list_pos !=  tail1_pos) {
            //swap tail
            meta_holder[1][it->second.list_pos] = meta_holder[1][tail1_pos];
            key_map.find(meta_holder[1][tail1_pos]._key)->second.list_pos = it->second.list_pos;
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
        _currentSize = _currentSize - meta_holder[0][pos0]._size + req._size;
        swap(meta_holder[0][pos0], meta_holder[1][it->second.list_pos]);
        swap(it->second, key_map.find(key0)->second);
    }
    // check more eviction needed?
    while (_currentSize > _cacheSize) {
        evict(req._t);
    }
}


pair<uint64_t, uint32_t> WLCCache::rank(const uint32_t t) {
    uint32_t rand_idx = _distribution(_generator) % meta_holder[0].size();
    //if not trained yet, use random
    if (booster == nullptr) {
        return {meta_holder[0][rand_idx]._key, rand_idx};
    }

    uint n_sample = min(sample_rate, (uint32_t) meta_holder[0].size());

    int32_t indptr[n_sample+1];
    indptr[0] = 0;
    int32_t indices[n_sample*WLC::n_feature];
    double data[n_sample * WLC::n_feature];
    int32_t past_timestamps[n_sample];
    uint32_t sizes[n_sample];
    //next_past_timestamp, next_size = next_indptr - 1

    unsigned int idx_feature = 0;
    unsigned int idx_row = 0;
    for (int i = 0; i < n_sample; i++) {
        uint32_t pos = (i+rand_idx)%meta_holder[0].size();
        auto & meta = meta_holder[0][pos];
        //fill in past_interval
        indices[idx_feature] = 0;
        data[idx_feature++] = t - meta._past_timestamp;
        past_timestamps[idx_row] = meta._past_timestamp;

        uint8_t j = 0;
        uint32_t this_past_distance = 0;
        uint8_t n_within = 0;
        if (meta._extra) {
            for (j = 0; j < meta._extra->_past_distance_idx && j < WLC::max_n_past_distances; ++j) {
                uint8_t past_distance_idx = (meta._extra->_past_distance_idx - 1 - j) % WLC::max_n_past_distances;
                uint32_t & past_distance = meta._extra->_past_distances[past_distance_idx];
                this_past_distance += past_distance;
                indices[idx_feature] = j+1;
                data[idx_feature++] = past_distance;
                if (this_past_distance < WLC::memory_window) {
                    ++n_within;
                }
//                } else
//                    break;
            }
        }

        indices[idx_feature] = WLC::max_n_past_timestamps;
        data[idx_feature++] = meta._size;
        sizes[idx_row] = meta._size;

        for (uint k = 0; k < WLC::n_extra_fields; ++k) {
            indices[idx_feature] = WLC::max_n_past_timestamps + k + 1;
            data[idx_feature++] = meta._extra_features[k];
        }

        indices[idx_feature] = WLC::max_n_past_timestamps+WLC::n_extra_fields+1;
        data[idx_feature++] = n_within;

        for (uint8_t k = 0; k < WLC::n_edc_feature; ++k) {
            indices[idx_feature] = WLC::max_n_past_timestamps + WLC::n_extra_fields + 2 + k;
            uint32_t _distance_idx = min(uint32_t(t - meta._past_timestamp) / WLC::edc_windows[k],
                                         WLC::max_hash_edc_idx);
            if (meta._extra)
                data[idx_feature++] = meta._extra->_edc[k] * WLC::hash_edc[_distance_idx];
            else
                data[idx_feature++] = WLC::hash_edc[_distance_idx];
        }
        //remove future t
        indptr[++idx_row] = idx_feature;
    }
    int64_t len;
    vector<double> result(n_sample);
    system_clock::time_point timeBegin;
    //sample to measure inference time
    if (!(t % 10000))
        timeBegin = chrono::system_clock::now();
    LGBM_BoosterPredictForCSR(booster,
                              static_cast<void *>(indptr),
                              C_API_DTYPE_INT32,
                              indices,
                              static_cast<void *>(data),
                              C_API_DTYPE_FLOAT64,
                              idx_row+1,
                              idx_feature,
                              WLC::n_feature,  //remove future t
                              C_API_PREDICT_NORMAL,
                              0,
                              inference_params,
                              &len,
                              result.data());

    if (!(t % 10000))
        inference_time = 0.95 * inference_time +
                         0.05 *
                         chrono::duration_cast<chrono::microseconds>(chrono::system_clock::now() - timeBegin).count();
//    for (int i = 0; i < n_sample; ++i)
//        result[i] -= (t - past_timestamps[i]);
    if (objective == object_miss_ratio)
        for (uint32_t i = 0; i < n_sample; ++i)
            result[i] *= sizes[i];

    double worst_score;
    uint32_t worst_pos;
    uint64_t min_past_timestamp;

    for (int i = 0; i < n_sample; ++i)
        if (!i || result[i] > worst_score || (result[i] == worst_score && (past_timestamps[i] < min_past_timestamp))) {
            worst_score = result[i];
            worst_pos = i;
            min_past_timestamp = past_timestamps[i];
        }
    worst_pos = (worst_pos+rand_idx)%meta_holder[0].size();
    auto & meta = meta_holder[0][worst_pos];
    auto & worst_key = meta._key;

    return {worst_key, worst_pos};
}

void WLCCache::evict(const uint32_t t) {
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
        key_map.find(meta_holder[0][activate_tail_idx]._key)->second.list_pos = old_pos;
    }
    meta_holder[0].pop_back();

    key_map.find(key)->second = {1, new_pos};
    _currentSize -= meta_holder[1][new_pos]._size;
}


