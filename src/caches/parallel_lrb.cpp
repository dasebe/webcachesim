//
// Created by zhenyus on 1/16/19.
//

#include "parallel_lrb.h"

void ParallelLRBCache::train() {
//        auto timeBegin = std::chrono::system_clock::now();
    // create training dataset
    DatasetHandle trainData;

    BoosterHandle background_booster = nullptr;

    LGBM_DatasetCreateFromCSR(
            static_cast<void *>(background_training_data->indptr.data()),
            C_API_DTYPE_INT32,
            background_training_data->indices.data(),
            static_cast<void *>(background_training_data->data.data()),
            C_API_DTYPE_FLOAT64,
            background_training_data->indptr.size(),
            background_training_data->data.size(),
            ParallelLRB::n_feature,  //remove future t
            LRB_train_params,
            nullptr,
            &trainData);

    LGBM_DatasetSetField(trainData,
                         "label",
                         static_cast<void *>(background_training_data->labels.data()),
                         background_training_data->labels.size(),
                         C_API_DTYPE_FLOAT32);

    // init booster
    LGBM_BoosterCreate(trainData, LRB_train_params, &background_booster);

    // train
    for (int i = 0; i < stoi(LRB_train_params["num_iterations"]); i++) {
        int isFinished;
        LGBM_BoosterUpdateOneIter(background_booster, &isFinished);
        if (isFinished) {
            break;
        }
    }

//        auto time1 = std::chrono::system_clock::now();

    //don't testing training in order to reduce model available latency
    booster_mutex.lock();
    std::swap(booster, background_booster);
    booster_mutex.unlock();
    if_trained = true;

//        int64_t len;
//        std::vector<double > result(background_training_data->indptr.size()-1);
//        LGBM_BoosterPredictForCSR(background_booster,
//                                  static_cast<void *>(background_training_data->indptr.data()),
//                                  C_API_DTYPE_INT32,
//                                  background_training_data->indices.data(),
//                                  static_cast<void *>(background_training_data->data.data()),
//                                  C_API_DTYPE_FLOAT64,
//                                  background_training_data->indptr.size(),
//                                  background_training_data->data.size(),
//                                  LRB::n_feature,  //remove future t
//                                  C_API_PREDICT_NORMAL,
//                                  0,
//                                  LRB_train_params,
//                                  &len,
//                                  result.data());
//        auto time2 = std::chrono::system_clock::now();

//        double se = 0;
//        for (int i = 0; i < result.size(); ++i) {
//            auto diff = result[i] - background_training_data->labels[i];
//            se += diff * diff;
//        }
//        training_loss = training_loss * 0.99 + se/LRB::batch_size*0.01;

    if (background_booster)
        LGBM_BoosterFree(background_booster);

    LGBM_DatasetFree(trainData);
//        training_time = 0.8*training_time + 0.2*std::chrono::duration_cast<std::chrono::microseconds>(time1 - timeBegin).count();
}

void ParallelLRBCache::sample() {
    // warmup not finish
    if (in_cache_metas.empty() || out_cache_metas.empty())
        return;
    auto rand_idx = _distribution(_generator);
    auto n_l0 = static_cast<uint32_t>(in_cache_metas.size());
    auto n_l1 = static_cast<uint32_t>(out_cache_metas.size());
    //TODO: we should use n_l0 / (n_l0 + n_l1), but this makes byte miss ratio worse
    auto r = rand();
    if (r / (RAND_MAX + 1.) < static_cast<float>(n_l1) / (n_l0 + n_l1)) {
        uint32_t pos = rand_idx % n_l0;
        auto &meta = in_cache_metas[pos];
        meta.emplace_sample(t_counter);
    } else {
        uint32_t pos = rand_idx % n_l1;
        auto &meta = out_cache_metas[pos];
        meta.emplace_sample(t_counter);
    }
}

void ParallelLRBCache::async_lookup(const uint64_t &key) {
    //first update the metadata: insert/update, which can trigger pending data.mature
    auto it = key_map.find(key);
    if (it != key_map.end()) {
        auto list_idx = it->second.list_idx;
        auto list_pos = it->second.list_pos;
        ParallelLRBMeta &meta = list_idx ? out_cache_metas[list_pos] : in_cache_metas[list_pos];
        //update past timestamps
        assert(meta._key == key);
        uint32_t last_timestamp = meta._past_timestamp;
        uint32_t forget_timestamp = last_timestamp + ParallelLRB::memory_window;
        //if the key in out_metadata, it must also in forget table
        assert((!list_idx) ||
               (negative_candidate_queue.find(forget_timestamp % ParallelLRB::memory_window) !=
                negative_candidate_queue.end()));
        //re-request
        if (!meta._sample_times.empty()) {
            //mature
            training_data_mutex.lock();
            for (auto &sample_time: meta._sample_times) {
                //don't use label within the first forget window because the data is not static
                uint32_t future_distance = t_counter - sample_time;
                training_data->emplace_back(meta, sample_time, future_distance);
            }
            training_data_mutex.unlock();
            meta._sample_times.clear();
            meta._sample_times.shrink_to_fit();
        }
        //make this update after update training, otherwise the last timestamp will change
        meta.update(t_counter);
        if (list_idx) {
            negative_candidate_queue.erase(forget_timestamp % ParallelLRB::memory_window);
            negative_candidate_queue.insert(
                    {(t_counter + ParallelLRB::memory_window) % ParallelLRB::memory_window, key});
            assert(negative_candidate_queue.find(
                    (t_counter + ParallelLRB::memory_window) % ParallelLRB::memory_window) !=
                   negative_candidate_queue.end());
        } else {
            auto *p = dynamic_cast<ParallelInCacheMeta *>(&meta);
            p->p_last_request = in_cache_lru_queue.re_request(p->p_last_request);
        }
        //sampling
        sample();
        ++t_counter;
        forget();
    } else {
        //logical time won't progress as no state change in our system
    }
}


void ParallelLRBCache::forget() {
    /*
     * forget happens exactly after the beginning of each time, without doing any other operations. For example, an
     * object is request at time 0 with memory window = 5, and will be forgotten exactly at the start of time 5.
     * */
    //remove item from forget table, which is not going to be affect from update
    auto it = negative_candidate_queue.find(t_counter % ParallelLRB::memory_window);
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
            uint32_t future_distance = ParallelLRB::memory_window * 2;
            training_data_mutex.lock();
            for (auto &sample_time: meta._sample_times) {
                //don't use label within the first forget window because the data is not static
                training_data->emplace_back(meta, sample_time, future_distance);
            }
            training_data_mutex.unlock();
            meta._sample_times.clear();
            meta._sample_times.shrink_to_fit();
        }

        assert(meta._key == forget_key);
        //free the actual content
        meta.free();
        //evict
        uint32_t tail_pos = out_cache_metas.size() - 1;
        if (pos != tail_pos) {
            //swap tail
            out_cache_metas[pos] = out_cache_metas[tail_pos];
            key_map.find(out_cache_metas[tail_pos]._key)->second.list_pos = pos;
        }
        out_cache_metas.pop_back();
        key_map.erase(forget_key);
        auto shard_id = forget_key%n_shard;
        size_map_mutex[shard_id].lock();
        size_map[shard_id].erase(forget_key);
        size_map_mutex[shard_id].unlock();
        negative_candidate_queue.erase(t_counter % ParallelLRB::memory_window);

    }
}

void ParallelLRBCache::async_admit(const uint64_t &key, const int64_t &size, const uint16_t *extra_features) {
    auto it = key_map.find(key);
    if (it == key_map.end()) {
        //fresh insert
        key_map.insert({key, KeyMapEntryT{.list_idx=0, .list_pos = (uint32_t) in_cache_metas.size()}});
        auto shard_id = key%n_shard;
        size_map_mutex[shard_id].lock();
        size_map[shard_id].insert({key, size});
        size_map_mutex[shard_id].unlock();

        auto lru_it = in_cache_lru_queue.request(key);
        in_cache_metas.emplace_back(key, size, t_counter, extra_features, lru_it);
        _currentSize += size;
        if (_currentSize <= _cacheSize)
            goto Lreturn;
    } else if (!it->second.list_idx) {
        //already in the cache
        goto Lnoop;
    } else {
        //bring list 1 to list 0
        //first move meta data, then modify hash table
        uint32_t tail0_pos = in_cache_metas.size();
        auto &meta = out_cache_metas[it->second.list_pos];
        auto forget_timestamp = meta._past_timestamp + ParallelLRB::memory_window;
        negative_candidate_queue.erase(forget_timestamp % ParallelLRB::memory_window);
        auto it_lru = in_cache_lru_queue.request(key);
        in_cache_metas.emplace_back(out_cache_metas[it->second.list_pos], it_lru);
        uint32_t tail1_pos = out_cache_metas.size() - 1;
        if (it->second.list_pos != tail1_pos) {
            //swap tail
            out_cache_metas[it->second.list_pos] = out_cache_metas[tail1_pos];
            key_map.find(out_cache_metas[tail1_pos]._key)->second.list_pos = it->second.list_pos;
        }
        out_cache_metas.pop_back();
        it->second = {0, tail0_pos};
        auto shard_id = key%n_shard;
        size_map_mutex[shard_id].lock();
        size_map[shard_id].find(key)->second = size;
        size_map_mutex[shard_id].unlock();
        _currentSize += size;
    }
    // check more eviction needed?
    while (_currentSize > _cacheSize) {
        evict();
    }
    Lreturn:
    //sampling
    sample();
    ++t_counter;
    forget();
    //no logical op is performed
    Lnoop:
    return;
}


pair<uint64_t, uint32_t> ParallelLRBCache::rank() {
    //if not trained yet, or in_cache_lru past memory window, use LRU
    uint64_t &candidate_key = in_cache_lru_queue.dq.back();
    auto it = key_map.find(candidate_key);
    auto pos = it->second.list_pos;
    auto &meta = in_cache_metas[pos];
    if ((!if_trained) || (ParallelLRB::memory_window <= t_counter - meta._past_timestamp))
        return {meta._key, pos};


    int32_t indptr[sample_rate + 1];
    indptr[0] = 0;
    int32_t indices[sample_rate * ParallelLRB::n_feature];
    double data[sample_rate * ParallelLRB::n_feature];
    int32_t past_timestamps[sample_rate];

    uint64_t keys[sample_rate];
    uint32_t poses[sample_rate];
    //next_past_timestamp, next_size = next_indptr - 1

    unsigned int idx_feature = 0;
    unsigned int idx_row = 0;
    for (int i = 0; i < sample_rate; i++) {
        uint32_t pos = _distribution(_generator) % in_cache_metas.size();
        auto &meta = in_cache_metas[pos];

        keys[i] = meta._key;
        poses[i] = pos;
        //fill in past_interval
        indices[idx_feature] = 0;
        data[idx_feature++] = t_counter - meta._past_timestamp;

        uint8_t j = 0;
        uint32_t this_past_distance = 0;
        uint8_t n_within = 0;
        if (meta._extra) {
            for (j = 0; j < meta._extra->_past_distance_idx && j < ParallelLRB::max_n_past_distances; ++j) {
                uint8_t past_distance_idx =
                        (meta._extra->_past_distance_idx - 1 - j) % ParallelLRB::max_n_past_distances;
                uint32_t &past_distance = meta._extra->_past_distances[past_distance_idx];
                this_past_distance += past_distance;
                indices[idx_feature] = j + 1;
                data[idx_feature++] = past_distance;
                if (this_past_distance < ParallelLRB::memory_window) {
                    ++n_within;
                }
//                } else
//                    break;
            }
        }

        indices[idx_feature] = ParallelLRB::max_n_past_timestamps;
        data[idx_feature++] = meta._size;
//        sizes[idx_row] = meta._size;

        for (uint k = 0; k < ParallelLRB::n_extra_fields; ++k) {
            indices[idx_feature] = ParallelLRB::max_n_past_timestamps + k + 1;
            data[idx_feature++] = meta._extra_features[k];
        }

        indices[idx_feature] = ParallelLRB::max_n_past_timestamps + ParallelLRB::n_extra_fields + 1;
        data[idx_feature++] = n_within;

        for (uint8_t k = 0; k < ParallelLRB::n_edc_feature; ++k) {
            indices[idx_feature] = ParallelLRB::max_n_past_timestamps + ParallelLRB::n_extra_fields + 2 + k;
            uint32_t _distance_idx = min(uint32_t(t_counter - meta._past_timestamp) / ParallelLRB::edc_windows[k],
                                         ParallelLRB::max_hash_edc_idx);
            if (meta._extra)
                data[idx_feature++] = meta._extra->_edc[k] * ParallelLRB::hash_edc[_distance_idx];
            else
                data[idx_feature++] = ParallelLRB::hash_edc[_distance_idx];
        }
        //remove future t
        indptr[++idx_row] = idx_feature;
    }
    int64_t len;
    std::vector<double> result(sample_rate);
//    auto time_begin = std::chrono::system_clock::now();
    booster_mutex.lock();
    LGBM_BoosterPredictForCSR(booster,
                              static_cast<void *>(indptr),
                              C_API_DTYPE_INT32,
                              indices,
                              static_cast<void *>(data),
                              C_API_DTYPE_FLOAT64,
                              idx_row + 1,
                              idx_feature,
                              ParallelLRB::n_feature,  //remove future t
                              C_API_PREDICT_NORMAL,
                              0,
                              LRB_inference_params,
                              &len,
                              result.data());
    booster_mutex.unlock();

//    auto time_end = std::chrono::system_clock::now();
//    inference_time = 0.99 * inference_time + 0.01 * std::chrono::duration_cast<std::chrono::microseconds>(time_end - time_begin).count();
//    for (int i = 0; i < sample_rate; ++i)
//        result[i] -= (t - past_timestamps[i]);
//    if (objective == object_hit_rate)
//        for (uint32_t i = 0; i < n_sample; ++i)
//            result[i] *= sizes[i];

    double worst_score = result[0];
    uint32_t worst_pos = poses[0];
    uint64_t worst_key = keys[0];
    uint64_t min_past_timestamp = past_timestamps[0];

    for (int i = 1; i < sample_rate; ++i)
        if (result[i] > worst_score || (result[i] == worst_score && (past_timestamps[i] < min_past_timestamp))) {
            worst_score = result[i];
            worst_pos = poses[i];
            worst_key = keys[i];
            min_past_timestamp = past_timestamps[i];
        }

    return {worst_key, worst_pos};
}

void ParallelLRBCache::evict() {
    auto epair = rank();
    uint64_t &key = epair.first;
    uint32_t &old_pos = epair.second;

    auto &meta = in_cache_metas[old_pos];
    if (ParallelLRB::memory_window <= t_counter - meta._past_timestamp) {
        //must be the tail of lru
        if (!meta._sample_times.empty()) {
            //mature
            uint32_t future_distance = t_counter - meta._past_timestamp + ParallelLRB::memory_window;
            training_data_mutex.lock();
            for (auto &sample_time: meta._sample_times) {
                //don't use label within the first forget window because the data is not static
                training_data->emplace_back(meta, sample_time, future_distance);
            }
            training_data_mutex.unlock();
            meta._sample_times.clear();
            meta._sample_times.shrink_to_fit();
        }


        in_cache_lru_queue.dq.erase(meta.p_last_request);
        meta.p_last_request = in_cache_lru_queue.dq.end();
        //above is suppose to be below, but to make sure the action is correct
//        in_cache_lru_queue.dq.pop_back();
        meta.free();
        _currentSize -= meta._size;
        key_map.erase(key);
        //remove from metas
        auto shard_id = key%n_shard;
        size_map_mutex[shard_id].lock();
        size_map[shard_id].erase(key);
        size_map_mutex[shard_id].unlock();

        uint32_t activate_tail_idx = in_cache_metas.size() - 1;
        if (old_pos != activate_tail_idx) {
            //move tail
            in_cache_metas[old_pos] = in_cache_metas[activate_tail_idx];
            key_map.find(in_cache_metas[activate_tail_idx]._key)->second.list_pos = old_pos;
        }
        in_cache_metas.pop_back();

    } else {
        //bring list 0 to list 1
        in_cache_lru_queue.dq.erase(meta.p_last_request);
        meta.p_last_request = in_cache_lru_queue.dq.end();
        _currentSize -= meta._size;
        negative_candidate_queue.insert(
                {(meta._past_timestamp + ParallelLRB::memory_window) % ParallelLRB::memory_window, meta._key});

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
        //in list 1, can still query
        auto shard_id = key%n_shard;
        size_map_mutex[shard_id].lock();
        size_map[shard_id].find(key)->second = 0;
        size_map_mutex[shard_id].unlock();

    }
}
