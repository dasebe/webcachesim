//
// Created by zhenyus on 3/15/19.
//

#include "lr.h"

//init with a wrong value


void LRCache::train() {
    if (weights.empty())
        weights = vector<double >(LR::max_n_past_timestamps);
    auto gradient_weights = vector<double >(LR::max_n_past_timestamps);
    double gradient_bias = 0;
    for (auto & training: training_data) {
        double score = 0;
        int i = 0;
        for (; i < training.past_distances.size(); ++i)
            score += weights[i] * training.past_distances[i];
        for (; i < LR::max_n_past_timestamps; ++i)
            score += weights[i] * LR::log1p_forget_window;
        double diff = score + bias - training.future_distance;
        i = 0;
        for (; i < training.past_distances.size(); ++i)
            gradient_weights[i] += diff * training.past_distances[i];
        for (; i < LR::max_n_past_timestamps; ++i)
            gradient_weights[i] += diff * LR::log1p_forget_window;
        gradient_bias += diff;
    }
//    for (int i = 0; i < LR::max_n_past_timestamps; ++i)
//        weights[i] -= learning_rate / batch_size * gradient_weights[i];
//    bias -= learning_rate / batch_size * gradient_bias;
}

//void LRCache::try_train(uint64_t &t) {
//    static uint64_t next_idx = 0;
//    if (t < gradient_window)
//        return;
//    //look at previous window
//    auto gradient_window_idx = t / gradient_window - 1;
//    //already update
//    if (gradient_window_idx != next_idx)
//        return;
//    ++next_idx;
//    //perhaps no gradient at all
//    if (gradient_window_idx >= pending_gradients.size())
//        return;
//
//    if (bias_point == none) {
//        uint64_t n_update = 0;
//        for (uint j = 0; j < n_window_bins && !pending_gradients[gradient_window_idx].empty(); ++j) {
//            auto &gradients = pending_gradients[gradient_window_idx][j];
//            if (gradients.n_update) {
//                n_update += gradients.n_update;
//            }
//        }
//        if (!n_update)
//            return;
//        for (uint j = 0; j < n_window_bins && !pending_gradients[gradient_window_idx].empty(); ++j) {
//            auto &gradients = pending_gradients[gradient_window_idx][j];
//            if (gradients.n_update) {
//                bias -= learning_rate / n_update * gradients.bias;
//                for (uint i = 0; i < LR::max_n_past_timestamps; ++i)
//                    weights[i] -= learning_rate / n_update * gradients.weights[i];
//            }
//        }
//        return;
//    }
//
//    if ( edge <= bias_point && bias_point <= mid ) {
//        //bias weight
//        double f_evicted_idx = ((double) (* f_evicted)) / size_bin;
//        auto sample_bias = vector<double >(n_window_bins);
//        double sum_sample_bias = 0;
//        for (uint j = 0; j < n_window_bins && ! pending_gradients[gradient_window_idx].empty(); ++j) {
//            auto &gradients = pending_gradients[gradient_window_idx][j];
//            double class_id;  //smaller is better
//            double base;
//            if (gradients.n_update) {
//                if (bias_point == edge) {
//                    if (j <= f_evicted_idx) {
//                        class_id = j;
//                    } else {
//                        class_id = n_window_bins - 1 - j;
//                    }
//                } else if (bias_point == center) {
//                    class_id = abs(f_evicted_idx - j);
//                } else if (bias_point == mid) {
//                    if (j <= f_evicted_idx) {
//                        class_id = abs(f_evicted_idx / 2. - j);
//                    } else {
//                        class_id = abs((f_evicted_idx + n_window_bins) / 2 - j);
//                    }
//                }
//                base = 1 - 0.1 * class_id;
//                double weight = pow(base, alpha);
//                sample_bias[j] = weight * gradients.n_update;
//                sum_sample_bias += weight * gradients.n_update;
//            }
//        }
//
//        if (sum_sample_bias == 0)
//            return;
//
//        for (uint j = 0; j < n_window_bins && ! pending_gradients[gradient_window_idx].empty(); ++j) {
//            auto &gradients = pending_gradients[gradient_window_idx][j];
//            if (gradients.n_update) {
//                if (gradients.n_update) {
//                    bias -= learning_rate / gradients.n_update / sum_sample_bias * sample_bias[j] * gradients.bias ;
//                    for (uint i = 0; i < LR::max_n_past_timestamps; ++i)
//                        weights[i] -= learning_rate / gradients.n_update / sum_sample_bias * sample_bias[j] * gradients.weights[i];
//                }
//            }
//        }
//        return;
//    }
//
//    if (bias_point == rebalance) {
//        uint64_t n_update = 0;
//        for (uint j = 0; j < n_window_bins && !pending_gradients[gradient_window_idx].empty(); ++j) {
//            auto &gradients = pending_gradients[gradient_window_idx][j];
//            if (gradients.n_update) {
//                n_update += 1;
//            }
//        }
//        if (!n_update)
//            return;
//        for (uint j = 0; j < n_window_bins && !pending_gradients[gradient_window_idx].empty(); ++j) {
//            auto &gradients = pending_gradients[gradient_window_idx][j];
//            if (gradients.n_update) {
//                bias -= learning_rate / gradients.n_update / n_update * gradients.bias;
//                for (uint i = 0; i < LR::max_n_past_timestamps; ++i)
//                    weights[i] -= learning_rate / gradients.n_update / n_update * gradients.weights[i];
//            }
//        }
//        return;
//    }
//
//    if (bias_point == bin0) {
//        if(!pending_gradients[gradient_window_idx].empty()) {
//            auto &gradients = pending_gradients[gradient_window_idx][0];
//            if (gradients.n_update) {
//                bias -= learning_rate / gradients.n_update * gradients.bias;
//                for (uint i = 0; i < LR::max_n_past_timestamps; ++i)
//                    weights[i] -= learning_rate / gradients.n_update * gradients.weights[i];
//            }
//        }
//        return;
//    }
//
//    if (bias_point == sides) {
//        uint64_t n_update = 0;
//        double f_evicted_idx = ((double) (* f_evicted)) / size_bin;
//        for (uint j = 0; j < n_window_bins && !pending_gradients[gradient_window_idx].empty(); ++j) {
//            if ((f_evicted_idx - j < 2 && f_evicted_idx - j >= 1 ) || (j > f_evicted_idx && j - f_evicted_idx <=1)) {
//                auto &gradients = pending_gradients[gradient_window_idx][j];
//                if (gradients.n_update) {
//                    n_update += gradients.n_update;
//                }
//            }
//        }
//        if (!n_update)
//            return;
//        for (uint j = 0; j < n_window_bins && !pending_gradients[gradient_window_idx].empty(); ++j) {
//            if ((f_evicted_idx - j < 2 && f_evicted_idx - j >= 1 ) || (j > f_evicted_idx && j - f_evicted_idx <=1)) {
//                auto &gradients = pending_gradients[gradient_window_idx][j];
//                if (gradients.n_update) {
//                    bias -= learning_rate / n_update * gradients.bias;
//                    for (uint i = 0; i < LR::max_n_past_timestamps; ++i)
//                        weights[i] -= learning_rate / n_update * gradients.weights[i];
//                }
//            }
//        }
//        return;
//    }
//
//}

void LRCache::sample(uint64_t &t) {
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
    auto n_sample_l0 = min(max(uint32_t (sample_rate*n_l0/(n_l0+n_l1)), (uint32_t) 1), n_l0);
    auto n_sample_l1 = min(max(sample_rate - n_sample_l0, (uint32_t) 1), n_l1);

    //sample list 0
    {
        for (uint32_t i = 0; i < n_sample_l0; i++) {
            uint32_t pos = (uint32_t) (i + rand_idx) % n_l0;
            auto &meta = meta_holder[0][pos];
            uint8_t last_timestamp_idx = (meta._past_timestamp_idx - static_cast<uint8_t>(1))% LR::max_n_past_timestamps;
            uint64_t last_timestamp = meta._past_timestamps[last_timestamp_idx];
            uint64_t forget_timestamp = last_timestamp + LR::forget_window;
            pending_training_data.insert({forget_timestamp, LRPendingTrainingData(meta, t)});
        }
    }

    //sample list 1
    {
        for (uint32_t i = 0; i < n_sample_l1; i++) {
            uint32_t pos = (uint32_t) (i + rand_idx) % n_l1;
            auto &meta = meta_holder[1][pos];
            uint8_t last_timestamp_idx = (meta._past_timestamp_idx - static_cast<uint8_t>(1))% LR::max_n_past_timestamps;
            uint64_t last_timestamp = meta._past_timestamps[last_timestamp_idx];
            uint64_t forget_timestamp = last_timestamp + LR::forget_window;
            pending_training_data.insert({forget_timestamp, LRPendingTrainingData(meta, t)});
        }
    }
}

//void LRCache::sample_without_update(uint64_t &t) {
//    if (meta_holder[0].empty() || meta_holder[1].empty())
//        return;
//    /*
//     * sample the cache to get in-cache distribution
//     */
//#ifdef LOG_SAMPLE_RATE
//    bool log_flag = ((double) rand() / (RAND_MAX)) < LOG_SAMPLE_RATE;
//#endif
//
//    //sample list 0
//    {
//        uint32_t rand_idx = _distribution(_generator) % meta_holder[0].size();
//        uint n_sample = min(sample_rate, (uint) meta_holder[0].size());
//
//        for (uint32_t i = 0; i < n_sample; i++) {
//            uint32_t pos = (i + rand_idx) % meta_holder[0].size();
//            auto &meta = meta_holder[0][pos];
//
//            //fill in past_interval
//            uint8_t j = 0;
//            auto past_intervals = vector<double >(LR::max_n_past_timestamps);
//            for (j = 0; j < meta._past_timestamp_idx && j < LR::max_n_past_timestamps; ++j) {
//                uint8_t past_timestamp_idx = (meta._past_timestamp_idx - 1 - j) % LR::max_n_past_timestamps;
//                uint64_t past_interval = t - meta._past_timestamps[past_timestamp_idx];
//                if (past_interval >= threshold)
//                    past_intervals[j] = log1p_threshold;
//                else
//                    past_intervals[j] = log1p(past_interval);
//            }
//            for (; j < LR::max_n_past_timestamps; j++)
//                past_intervals[j] = log1p_threshold;
//
//            uint64_t known_future_interval;
//            double log1p_known_future_interval;
//            if (meta._future_timestamp - t < threshold) {
//                known_future_interval = meta._future_timestamp - t;
//                log1p_known_future_interval = log1p(known_future_interval);
//            }
//            else {
////                known_future_interval = threshold - 1;
//                log1p_known_future_interval = log1p_threshold;
//            }
//
//#ifdef LOG_SAMPLE_RATE
//            //print distribution
//            if (log_flag) {
//                cout << 2 <<" ";
//                for (uint k = 0; k < LR::max_n_past_timestamps; ++k)
//                    cout << past_intervals[k] << " ";
//                cout << log1p_known_future_interval << endl;
//            }
//#endif
//        }
//    }
//}

bool LRCache::lookup(SimpleRequest &req) {
    bool ret;
    if (!(req._t%1000000)) {
        cerr << "cache size: "<<_currentSize<<"/"<<_cacheSize<<endl;
        cerr << "n_metadata: "<<key_map.size()<<endl;
        assert(key_map.size() == forget_table.size());
        cerr << "n_pending: "<< pending_training_data.size() <<endl;
        cerr << "n_training: "<<training_data.size()<<endl;
        cerr << "mean diff: " << mean_diff << endl;
        for (int j = 0; j < weights.size() ; ++j)
            cerr << "weight " << j << ": " << weights[j] << endl;
        cerr << "bias: " << bias << endl;
    }

    //first update the metadata: insert/update, which can trigger pending data.mature
    auto it = key_map.find(req._id);
    if (it != key_map.end()) {
        //update past timestamps
        bool & list_idx = it->second.first;
        uint32_t & pos_idx = it->second.second;
        LRMeta & meta = meta_holder[list_idx][pos_idx];
        assert(meta._key == req._id);
        uint8_t last_timestamp_idx = (meta._past_timestamp_idx - static_cast<uint8_t>(1))% LR::max_n_past_timestamps;
        uint64_t last_timestamp = meta._past_timestamps[last_timestamp_idx];
        uint64_t forget_timestamp = last_timestamp + LR::forget_window;
        //if the key in key_map, it must also in forget table
        auto forget_it = forget_table.find(forget_timestamp);
        assert(forget_it != forget_table.end());
        //re-request
        auto pending_range = pending_training_data.equal_range(forget_timestamp);
        for (auto pending_it = pending_range.first; pending_it != pending_range.second;) {
            //mature
            auto future_distance = log1p(req._t - last_timestamp);
            //don't use label within the first forget window because the data is not static
            if (req._t > LR::forget_window)
                training_data.emplace_back(pending_it->second, future_distance);
            //training
            if (training_data.size() == batch_size) {
                train();
                training_data.clear();
            }
            pending_it = pending_training_data.erase(pending_it);
        }
        //remove this entry
        forget_table.erase(forget_it);
        forget_table.insert({req._t+LR::forget_window, req._id});
        assert(key_map.size() == forget_table.size());

        //make this update after update training, otherwise the last timestamp will change
        meta.update(req._t);
        //update forget_table
        ret = !list_idx;
    } else {
        ret = false;
    }
    forget(req._t);
    //sampling
    if (!(req._t % training_sample_interval))
        sample(req._t);
    return ret;
}

void LRCache::forget(uint64_t &t) {
    //remove item from forget table, which is not going to be affect from update
    auto forget_it = forget_table.find(t);
    if (forget_it != forget_table.end()) {
        auto &key = forget_it->second;
        auto meta_it = key_map.find(key);
        if (!meta_it->second.first) {
//            cerr << "warning: force evicting object passing forget window" << endl;
            auto &pos = meta_it->second.second;
            auto &meta = meta_holder[0][pos];
            //timeout mature
            auto pending_range = pending_training_data.equal_range(t);
            for (auto pending_it = pending_range.first; pending_it != pending_range.second;) {
                //mature
                auto future_distance = LR::log1p_forget_window;
                training_data.emplace_back(pending_it->second, future_distance);
                //training
                if (training_data.size() == batch_size) {
                    train();
                    training_data.clear();
                }
                pending_it = pending_training_data.erase(pending_it);
            }

            assert(meta._key == key);
            key_map.erase(key);
            _currentSize -= meta._size;
            //evict
            uint32_t tail0_pos = meta_holder[0].size()-1;
            if (pos !=  tail0_pos) {
                //swap tail
                meta_holder[0][pos] = meta_holder[0][tail0_pos];
                key_map.find(meta_holder[0][tail0_pos]._key)->second.second = pos;
            }
            meta_holder[0].pop_back();
            //forget
            forget_table.erase(forget_it);
            assert(key_map.size() == forget_table.size());
        } else {
            auto &pos = meta_it->second.second;
            auto &meta = meta_holder[1][pos];
            //timeout mature
            auto pending_range = pending_training_data.equal_range(t);
            for (auto pending_it = pending_range.first; pending_it != pending_range.second;) {
                //mature
                auto future_distance = LR::log1p_forget_window;
                training_data.emplace_back(pending_it->second, future_distance);
                //training
                if (training_data.size() == batch_size) {
                    train();
                    training_data.clear();
                }
                pending_it = pending_training_data.erase(pending_it);
            }

            assert(meta._key == key);
            key_map.erase(key);
            //evict
            uint32_t tail1_pos = meta_holder[1].size() - 1;
            if (pos != tail1_pos) {
                //swap tail
                meta_holder[1][pos] = meta_holder[1][tail1_pos];
                key_map.find(meta_holder[1][tail1_pos]._key)->second.second = pos;
            }
            meta_holder[1].pop_back();
            //forget
            forget_table.erase(forget_it);
            assert(key_map.size() == forget_table.size());
        }
    }
}

//
//bool LRCache::lookup_without_update(SimpleRequest &_req) {
//    auto & req = dynamic_cast<AnnotatedRequest &>(_req);
//    static uint64_t i = 0;
//    if (!(i%1000000)) {
//        cerr << "mean diff: " << mean_diff << endl;
//        for (int j = 0; j < LR::max_n_past_timestamps; ++j)
//            cerr << "weight " << j << ": " << weights[j] << endl;
//        cerr << "bias: " << bias << endl;
//    }
//    ++i;
//
//    //todo: deal with size consistently
//    try_train(req._t);
//    sample_without_update(req._t);
//
//    auto it = key_map.find(req._id);
//    if (it != key_map.end()) {
//        //update past timestamps
//        bool & list_idx = it->second.first;
//        uint32_t & pos_idx = it->second.second;
//        meta_holder[list_idx][pos_idx].update(req._t, req._next_seq);
//        return !list_idx;
//    }
//    return false;
//}

void LRCache::admit(SimpleRequest &req) {
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
        meta_holder[0].emplace_back(req._id, req._size, req._t);
        _currentSize += size;
        forget_table.insert({req._t + LR::forget_window, req._id});
        assert(key_map.size() == forget_table.size());
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


pair<uint64_t, uint32_t> LRCache::rank(const uint64_t & t) {
    uint32_t rand_idx = _distribution(_generator) % meta_holder[0].size();
    //if not trained yet, use random
    if (weights.empty()) {
        return {meta_holder[0][rand_idx]._key, rand_idx};
    }

    double worst_score;
    uint64_t worst_key;
    uint32_t worst_pos;
    uint64_t min_past_timestamp;

    uint n_sample = min(sample_rate, (uint32_t) meta_holder[0].size());

    for (uint32_t i = 0; i < n_sample; i++) {
        uint32_t pos = (i + rand_idx) % meta_holder[0].size();
        auto &meta = meta_holder[0][pos];
        //fill in past_interval
        double score = 0;
        int8_t j = 0;
        auto past_intervals = vector<double>(LR::max_n_past_timestamps);
        for (; j < meta._past_timestamp_idx && j < LR::max_n_past_timestamps; ++j) {
            uint8_t past_timestamp_idx = (meta._past_timestamp_idx - 1 - j) % LR::max_n_past_timestamps;
            uint64_t past_interval = t - meta._past_timestamps[past_timestamp_idx];
            if (past_interval < LR::forget_window)
                score += log1p(past_interval) * weights[j];
        }
        for (; j < LR::max_n_past_timestamps; ++j)
            score += LR::log1p_forget_window * weights[j];
        if (objective == object_hit_rate)
            score += log1p(meta._size);

        uint8_t oldest_idx = (meta._past_timestamp_idx - 1) % LR::max_n_past_timestamps;
        uint64_t past_timestamp = meta._past_timestamps[oldest_idx];

        if (!i || score > worst_score || (score == worst_score && (past_timestamp < min_past_timestamp))) {
            worst_score = score;
            worst_key = meta._key;
            worst_pos = pos;
            min_past_timestamp = past_timestamp;
        }
    }

    return {worst_key, worst_pos};
}

void LRCache::evict(const uint64_t & t) {
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



