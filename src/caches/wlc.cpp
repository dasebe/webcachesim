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


//    int res;
//    auto importances = vector<double >(WLC::n_feature);
//
//    res = LGBM_BoosterFeatureImportance(booster,
//                          0,
//                          1,
//                          importances.data());
//    if (res == -1)
//        abort();
//    for (int i = 0; i < WLC::n_feature; ++i) {
//        if (i<32)
//            cout<<"delta"<<i<<" "<<importances[i]<<endl;
//        else if (i<33)
//            cout<<"size "<<importances[i]<<endl;
//        else if (i<37)
//            cout<<"extra"<<i-33<<" "<<importances[i]<<endl;
//        else if (i<38)
//            cout<<"n_delta "<<importances[i]<<endl;
//        else
//            cout<<"edc"<<i-38<<" "<<importances[i]<<endl;
//    }
//    cout<<endl;

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
    if (in_cache_metas.empty() || out_cache_metas.empty())
        return;
#ifdef LOG_SAMPLE_RATE
    bool log_flag = ((double) rand() / (RAND_MAX)) < LOG_SAMPLE_RATE;
#endif
    auto n_l0 = static_cast<uint32_t>(in_cache_metas.size());
    auto n_l1 = static_cast<uint32_t>(out_cache_metas.size());
    auto rand_idx = _distribution(_generator);
    // at least sample 1 from the list, at most size of the list
    auto n_sample_l0 = min(max(uint32_t (training_sample_interval*n_l0/(n_l0+n_l1)), (uint32_t) 1), n_l0);
    auto n_sample_l1 = min(max(uint32_t (training_sample_interval - n_sample_l0), (uint32_t) 1), n_l1);

    //sample list 0
    for (uint32_t i = 0; i < n_sample_l0; i++) {
        uint32_t pos = (uint32_t) (i + rand_idx) % n_l0;
        auto &meta = in_cache_metas[pos];
        meta.emplace_sample(t);
    }

    //sample list 1
    for (uint32_t i = 0; i < n_sample_l1; i++) {
        uint32_t pos = (uint32_t) (i + rand_idx) % n_l1;
        auto &meta = out_cache_metas[pos];
        meta.emplace_sample(t);
    }
}


void WLCCache::print_stats() {
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
    cerr
            << "cache size: " << _currentSize << "/" << _cacheSize << " (" << ((double) _currentSize) / _cacheSize
            << ")" << endl
            << "in/out metadata " << in_cache_metas.size() << " / " << out_cache_metas.size() << endl
//    cerr << "feature overhead: "<<feature_overhead<<endl;
            << "feature overhead per entry: " << feature_overhead / key_map.size() << endl
//    cerr << "sample overhead: "<<sample_overhead<<endl;
            << "sample overhead per entry: " << sample_overhead / key_map.size() << endl
            << "n_training: " << training_data->labels.size() << endl
            //            << "training loss: " << training_loss << endl
            //TODO: can remove this
            //            << "n_force_eviction: " << n_force_eviction << endl
            << "training_time: " << training_time << " ms" << endl
            << "inference_time: " << inference_time << " us" << endl;
    assert(in_cache_metas.size() + out_cache_metas.size() == key_map.size());
}


bool WLCCache::lookup(SimpleRequest &req) {
    bool ret;
    //piggy back
    if (req._t && !((req._t)%segment_window))
        print_stats();

    {
        AnnotatedRequest *_req = (AnnotatedRequest *) &req;
        auto it = WLC::future_timestamps.find(_req->_id);
        if (it == WLC::future_timestamps.end()) {
            WLC::future_timestamps.insert({_req->_id, _req->_next_seq});
        } else {
            it->second = _req->_next_seq;
        }
        //TODO: if we have global variable WLC::current_t, don't need t as a function argument
        WLC::current_t = req._t;
    }
    forget(req._t);

    //first update the metadata: insert/update, which can trigger pending data.mature
    auto it = key_map.find(req._id);
    if (it != key_map.end()) {
        auto list_idx = it->second.list_idx;
        auto list_pos = it->second.list_pos;
        WLCMeta &meta = list_idx ? out_cache_metas[list_pos] : in_cache_metas[list_pos];
        //update past timestamps
        assert(meta._key == req._id);
        uint64_t last_timestamp = meta._past_timestamp;
        uint64_t forget_timestamp = last_timestamp + WLC::memory_window;
        //if the key in out_metadata, it must also in forget table
        assert((!list_idx) ||
               (negative_candidate_queue.find(forget_timestamp % WLC::memory_window) !=
                negative_candidate_queue.end()));
        //re-request
        if (!meta._sample_times.empty()) {
            //mature
            for (auto & sample_time: meta._sample_times) {
                //don't use label within the first forget window because the data is not static
                uint32_t future_distance = req._t - sample_time;
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

        //make this update after update training, otherwise the last timestamp will change
        meta.update(req._t);
        if (list_idx) {
            negative_candidate_queue.erase(forget_timestamp % WLC::memory_window);
            negative_candidate_queue.insert({(req._t + WLC::memory_window) % WLC::memory_window, req._id});
            assert(negative_candidate_queue.find((req._t + WLC::memory_window) % WLC::memory_window) !=
                   negative_candidate_queue.end());
        } else {
            InCacheMeta *p = static_cast<InCacheMeta *>(&meta);
            p->p_last_request = in_cache_lru_queue.re_request(p->p_last_request);
        }
        //update negative_candidate_queue
        ret = !list_idx;
    } else {
        ret = false;
    }

    //sampling happens late to prevent immediate re-request
    if (!(req._t % training_sample_interval))
        sample(req._t);
    return ret;
}


void WLCCache::forget(uint32_t t) {
    /*
     * forget happens exactly after the beginning of each time, without doing any other operations. For example, an
     * object is request at time 0 with memory window = 5, and will be forgotten exactly at the start of time 5.
     * */
    //remove item from forget table, which is not going to be affect from update
    auto it = negative_candidate_queue.find(t%WLC::memory_window);
    if (it != negative_candidate_queue.end()) {
        auto forget_key = it->second;
        auto pos = key_map.find(forget_key)->second.list_pos;
        // Forget only happens at list 1
        assert(key_map.find(forget_key)->second.list_idx);
//        auto pos = meta_it->second.list_pos;
//        bool meta_id = meta_it->second.list_idx;
        auto &meta = out_cache_metas[pos];

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
//        if (!meta_id) {
//            ++n_force_eviction;
//            _currentSize -= meta._size;
//            {
//                auto it = WLC::future_timestamps.find(meta._key);
//                unsigned int decision_qulity =
//                        static_cast<double>(it->second - WLC::current_t) / (_cacheSize * 1e6 / byte_million_req);
//                decision_qulity = min((unsigned int) 255, decision_qulity);
//                eviction_qualities.emplace_back(decision_qulity);
//                eviction_logic_timestamps.emplace_back(WLC::current_t / 10000);
//            }
//        }
        //free the actual content
        meta.free();
        //TODO: can add a function to delete from a queue with (key, pos)
        //evict
        uint32_t tail_pos = out_cache_metas.size() - 1;
        if (pos != tail_pos) {
            //swap tail
            out_cache_metas[pos] = out_cache_metas[tail_pos];
            key_map.find(out_cache_metas[tail_pos]._key)->second.list_pos = pos;
        }
        out_cache_metas.pop_back();
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
        key_map.insert({req._id, {0, (uint32_t) in_cache_metas.size()}});
        auto lru_it = in_cache_lru_queue.request(req._id);
        in_cache_metas.emplace_back(req._id, req._size, req._t, req._extra_features, lru_it);
        _currentSize += size;
        //this must be a fresh insert
//        negative_candidate_queue.insert({(req._t + WLC::memory_window)%WLC::memory_window, req._id});
        if (_currentSize <= _cacheSize)
            return;
    } else if (size + _currentSize <= _cacheSize){
        //bring list 1 to list 0
        //first move meta data, then modify hash table
        uint32_t tail0_pos = in_cache_metas.size();
        auto &meta = out_cache_metas[it->second.list_pos];
        auto forget_timestamp = meta._past_timestamp + WLC::memory_window;
        negative_candidate_queue.erase(forget_timestamp % WLC::memory_window);
        auto it_lru = in_cache_lru_queue.request(req._id);
        in_cache_metas.emplace_back(out_cache_metas[it->second.list_pos], it_lru);
        uint32_t tail1_pos = out_cache_metas.size() - 1;
        if (it->second.list_pos !=  tail1_pos) {
            //swap tail
            out_cache_metas[it->second.list_pos] = out_cache_metas[tail1_pos];
            key_map.find(out_cache_metas[tail1_pos]._key)->second.list_pos = it->second.list_pos;
        }
        out_cache_metas.pop_back();
        it->second = {0, tail0_pos};
        _currentSize += size;
        return;
    }
    // check more eviction needed?
    while (_currentSize > _cacheSize) {
        evict(req._t);
    }
}


pair<uint64_t, uint32_t> WLCCache::rank(const uint32_t t) {
    {
        //if not trained yet, or in_cache_lru past memory window, use LRU
        uint64_t &candidate_key = in_cache_lru_queue.dq.back();
        auto it = key_map.find(candidate_key);
        auto pos = it->second.list_pos;
        auto &meta = in_cache_metas[pos];
        if ((!booster) || (meta._past_timestamp + WLC::memory_window <= t))
            return {meta._key, pos};
    }

    uint32_t rand_idx = _distribution(_generator) % in_cache_metas.size();

    uint n_sample = min(sample_rate, (uint32_t) in_cache_metas.size());

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
        uint32_t pos = (i + rand_idx) % in_cache_metas.size();
        auto &meta = in_cache_metas[pos];
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
    worst_pos = (worst_pos + rand_idx) % in_cache_metas.size();
    auto &meta = in_cache_metas[worst_pos];
    auto & worst_key = meta._key;

    return {worst_key, worst_pos};
}

void WLCCache::evict(const uint32_t t) {
    auto epair = rank(t);
    uint64_t & key = epair.first;
    uint32_t & old_pos = epair.second;


    {
        auto it = WLC::future_timestamps.find(key);
        unsigned int decision_qulity =
                static_cast<double>(it->second - WLC::current_t) / (_cacheSize * 1e6 / byte_million_req);
        decision_qulity = min((unsigned int) 255, decision_qulity);
        eviction_qualities.emplace_back(decision_qulity);
        eviction_logic_timestamps.emplace_back(WLC::current_t / 10000);
    }

    auto &meta = in_cache_metas[old_pos];
    if (meta._past_timestamp + WLC::memory_window <= t) {
        //must be the tail of lru
        if (!meta._sample_times.empty()) {
            //mature
            uint32_t future_distance = t - meta._past_timestamp + WLC::memory_window;
            for (auto &sample_time: meta._sample_times) {
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

        _currentSize -= meta._size;
        key_map.erase(key);
        in_cache_lru_queue.dq.pop_back();

        uint32_t activate_tail_idx = in_cache_metas.size() - 1;
        if (old_pos != activate_tail_idx) {
            //move tail
            in_cache_metas[old_pos] = in_cache_metas[activate_tail_idx];
            key_map.find(in_cache_metas[activate_tail_idx]._key)->second.list_pos = old_pos;
        }
        in_cache_metas.pop_back();

    } else {
        //bring list 0 to list 1

        auto &meta = in_cache_metas[old_pos];
        in_cache_lru_queue.dq.erase(meta.p_last_request);
        _currentSize -= meta._size;
        negative_candidate_queue.insert({(meta._past_timestamp + WLC::memory_window) % WLC::memory_window, meta._key});

        uint32_t new_pos = out_cache_metas.size();
        out_cache_metas.emplace_back(in_cache_metas[old_pos]);
        uint32_t activate_tail_idx = in_cache_metas.size() - 1;
        if (old_pos != activate_tail_idx) {
            //move tail
            in_cache_metas[old_pos] = in_cache_metas[activate_tail_idx];
            key_map.find(in_cache_metas[activate_tail_idx]._key)->second.list_pos = old_pos;
        }
        in_cache_metas.pop_back();
        key_map.find(key)->second = {1, new_pos};
    }
}


