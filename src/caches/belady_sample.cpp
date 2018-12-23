//
// Created by zhenyus on 12/17/18.
//

#include "belady_sample.h"
#include "utils.h"

using namespace std;


void BeladySampleCache::sample(uint64_t &t) {
#ifdef LOG_SAMPLE_RATE
    bool log_flag = ((double) rand() / (RAND_MAX)) < LOG_SAMPLE_RATE;
#endif

    //sample list 0
    if (!meta_holder[0].empty()) {
        uint32_t rand_idx = _distribution(_generator) % meta_holder[0].size();
        uint n_sample = min(sample_rate, (uint) meta_holder[0].size());

        for (uint32_t i = 0; i < n_sample; i++) {
            uint32_t pos = (i + rand_idx) % meta_holder[0].size();
            auto &meta = meta_holder[0][pos];

//            uint8_t oldest_idx = (meta._past_timestamp_idx - (uint8_t) 1)%n_past_intervals;
//            uint64_t & past_timestamp = meta._past_timestamps[oldest_idx];
//            if (past_timestamp + threshold < t) {
//                ++n_out_window;
//            }

            //fill in past_interval
            uint8_t j = 0;
            double past_intervals[max_n_past_intervals];
            for (j = 0; j < meta._past_timestamp_idx && j < n_past_intervals; ++j) {
                uint8_t past_timestamp_idx = (meta._past_timestamp_idx - 1 - j) % n_past_intervals;
                uint64_t past_interval = t - meta._past_timestamps[past_timestamp_idx];
                if (past_interval >= threshold)
                    past_intervals[j] = log1p_threshold;
                else
                    past_intervals[j] = log1p(past_interval);
            }
            for (; j < n_past_intervals; j++)
                past_intervals[j] = log1p_threshold;

#ifdef LOG_SAMPLE_RATE
            //print distribution
            if (log_flag) {
                cout << 0 <<" ";
                for (uint k = 0; k < n_past_intervals; ++k)
                    cout << past_intervals[k] << " ";
                cout << log1p(meta._future_timestamp - t) << endl;
            }
#endif
        }
    }

//    cout<<n_out_window<<endl;

}

void BeladySampleCacheFilter::sample(uint64_t &t, vector<Gradient> & ext_pending_gradients,
        double * ext_weights, double & ext_bias, uint64_t & ext_gradient_window) {
    if (meta_holder[0].empty() || meta_holder[1].empty())
        return;
#ifdef LOG_SAMPLE_RATE
    bool log_flag = ((double) rand() / (RAND_MAX)) < LOG_SAMPLE_RATE;
#endif

    //sample list 0
    {
        uint32_t rand_idx = _distribution(_generator) % meta_holder[0].size();
        uint n_sample = min( (uint) ceil((double) sample_rate*meta_holder[0].size()/(meta_holder[0].size()+meta_holder[1].size())),
                             (uint) meta_holder[0].size());

        for (uint32_t i = 0; i < n_sample; i++) {
            uint32_t pos = (i + rand_idx) % meta_holder[0].size();
            auto &meta = meta_holder[0][pos];

            //fill in past_interval
            uint8_t j = 0;
            double past_intervals[max_n_past_intervals];
            for (j = 0; j < meta._past_timestamp_idx && j < n_past_intervals; ++j) {
                uint8_t past_timestamp_idx = (meta._past_timestamp_idx - 1 - j) % n_past_intervals;
                uint64_t past_interval = t - meta._past_timestamps[past_timestamp_idx];
                if (past_interval >= threshold)
                    past_intervals[j] = log1p_threshold;
                else
                    past_intervals[j] = log1p(past_interval);
            }
            for (; j < n_past_intervals; j++)
                past_intervals[j] = log1p_threshold;


            uint64_t known_future_interval;
            double log1p_known_future_interval;
            if (meta._future_timestamp - t < threshold) {
                known_future_interval = meta._future_timestamp - t;
                log1p_known_future_interval = log1p(known_future_interval);
            }
            else {
                known_future_interval = threshold;
                log1p_known_future_interval = log1p_threshold;
            }

#ifdef LOG_SAMPLE_RATE
            //print distribution
            if (log_flag) {
                cout << 0 <<" ";
                for (uint k = 0; k < n_past_intervals; ++k)
                    cout << past_intervals[k] << " ";
                cout << log1p_known_future_interval << endl;
            }
#endif

            double score = 0;
            for (j = 0; j < n_past_intervals; j++)
                score += ext_weights[j] * past_intervals[j];

            //statistics
            double diff = score + ext_bias - log1p_known_future_interval;
            //update gradient
            auto gradient_window_idx = (t+known_future_interval) / ext_gradient_window;
            if (gradient_window_idx >= ext_pending_gradients.size())
                ext_pending_gradients.resize(gradient_window_idx + 1);
            auto &gradient = ext_pending_gradients[gradient_window_idx];
            for (uint k = 0; k < n_past_intervals; ++k)
                gradient.weights[k] += diff * past_intervals[k];
            gradient.bias += diff;
            ++gradient.n_update;
        }
    }

    //sample list 1, but don't update
    {
        uint32_t rand_idx = _distribution(_generator) % meta_holder[1].size();
        //sample less from list 1 as there are gc
        uint n_sample = min( (uint) floor( (double) sample_rate*meta_holder[1].size()/(meta_holder[0].size()+meta_holder[1].size())),
                             (uint) meta_holder[1].size());
//        cout<<n_sample<<endl;

        for (uint32_t i = 0; i < n_sample; i++) {
            //garbage collection
            while (meta_holder[1].size()) {
                uint32_t pos = (i + rand_idx) % meta_holder[1].size();
                auto &meta = meta_holder[1][pos];
                uint8_t oldest_idx = (meta._past_timestamp_idx - (uint8_t) 1)%n_past_intervals;
                uint64_t & past_timestamp = meta._past_timestamps[oldest_idx];
                if (past_timestamp + threshold < t) {
                    uint64_t & ekey = meta._key;
                    key_map.erase(ekey);
                    //evict
                    uint32_t tail1_pos = meta_holder[1].size()-1;
                    if (pos !=  tail1_pos) {
                        //swap tail
                        meta_holder[1][pos] = meta_holder[1][tail1_pos];
                        key_map.find(meta_holder[1][tail1_pos]._key)->second.second = pos;
                    }
                    meta_holder[1].pop_back();
                } else
                    break;
            }
            if (!meta_holder[1].size())
                break;
            uint32_t pos = (i + rand_idx) % meta_holder[1].size();
            auto &meta = meta_holder[1][pos];
            //fill in past_interval
            uint8_t j = 0;
            double past_intervals[max_n_past_intervals];
            for (j = 0; j < meta._past_timestamp_idx && j < n_past_intervals; ++j) {
                uint8_t past_timestamp_idx = (meta._past_timestamp_idx - 1 - j) % n_past_intervals;
                uint64_t past_interval = t - meta._past_timestamps[past_timestamp_idx];
                if (past_interval >= threshold)
                    past_intervals[j] = log1p_threshold;
                else
                    past_intervals[j] = log1p(past_interval);
            }
            for (; j < n_past_intervals; j++)
                past_intervals[j] = log1p_threshold;

            uint64_t known_future_interval;
            double log1p_known_future_interval;
            if (meta._future_timestamp - t < threshold) {
                known_future_interval = meta._future_timestamp - t;
                log1p_known_future_interval = log1p(known_future_interval);
            }
            else {
                known_future_interval = threshold;
                log1p_known_future_interval = log1p_threshold;
            }

#ifdef LOG_SAMPLE_RATE
            //print distribution
            if (log_flag) {
                cout << 1 <<" ";
                for (uint k = 0; k < n_past_intervals; ++k)
                    cout << past_intervals[k] << " ";
                cout << log1p_known_future_interval << endl;
            }
#endif
            if (out_sample) {
                double score = 0;
                for (j = 0; j < n_past_intervals; j++)
                    score += ext_weights[j] * past_intervals[j];

                //statistics
                double diff = score + ext_bias - log1p_known_future_interval;
                //update gradient
                auto gradient_window_idx = (t + known_future_interval) / ext_gradient_window;
                if (gradient_window_idx >= ext_pending_gradients.size())
                    ext_pending_gradients.resize(gradient_window_idx + 1);
                auto &gradient = ext_pending_gradients[gradient_window_idx];
                for (uint k = 0; k < n_past_intervals; ++k)
                    gradient.weights[k] += diff * past_intervals[k];
                gradient.bias += diff;
                ++gradient.n_update;
            }
        }
    }

//    cout<<n_out_window<<endl;

}


bool BeladySampleCache::lookup(SimpleRequest &_req) {
    auto & req = dynamic_cast<AnnotatedRequest &>(_req);

    //todo: deal with size consistently
    sample(req._t);

    auto it = key_map.find(req._id);
    if (it != key_map.end()) {
        //update past timestamps
        bool & list_idx = it->second.first;
        uint32_t & pos_idx = it->second.second;
        meta_holder[list_idx][pos_idx].update(req._t, req._next_t);

        return !list_idx;
    }
    return false;
}

bool BeladySampleCacheFilter::lookup(SimpleRequest &_req, vector<Gradient> & ext_pending_gradients,
        double * ext_weights, double & ext_bias, uint64_t & ext_gradient_window) {
    auto & req = dynamic_cast<AnnotatedRequest &>(_req);

    //todo: deal with size consistently
    sample(req._t, ext_pending_gradients, ext_weights, ext_bias, ext_gradient_window);

    auto it = key_map.find(req._id);
    if (it != key_map.end()) {
        //update past timestamps
        bool & list_idx = it->second.first;
        uint32_t & pos_idx = it->second.second;
        meta_holder[list_idx][pos_idx].update(req._t, req._next_t);

        return !list_idx;
    }
    return false;
}


void BeladySampleCache::admit(SimpleRequest &_req) {
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


pair<uint64_t, uint32_t> BeladySampleCache::rank(const uint64_t & t) {
    uint64_t max_future_timestamp;
    uint64_t max_key;
    uint32_t max_pos;

    uint32_t rand_idx = _distribution(_generator) % meta_holder[0].size();
    uint n_sample;
    if (sample_rate < meta_holder[0].size())
        n_sample = sample_rate;
    else
        n_sample = meta_holder[0].size();

    for (uint32_t i = 0; i < n_sample; i++) {
        uint32_t pos = (i+rand_idx)%meta_holder[0].size();
        auto & meta = meta_holder[0][pos];
        //fill in past_interval
        uint8_t j = 0;
        double past_intervals[max_n_past_intervals];
        for (j = 0; j < meta._past_timestamp_idx && j < n_past_intervals; ++j) {
            uint8_t past_timestamp_idx = (meta._past_timestamp_idx - 1 - j) % n_past_intervals;
            uint64_t past_interval = t - meta._past_timestamps[past_timestamp_idx];
            if (past_interval >= threshold)
                past_intervals[j] = log1p_threshold;
            else
                past_intervals[j] = log1p(past_interval);
        }
        for (; j < n_past_intervals; j++)
            past_intervals[j] = log1p_threshold;

        auto & future_timestamp = meta._future_timestamp;

        if (!i || future_timestamp > max_future_timestamp) {
            max_future_timestamp = future_timestamp;
            max_key = meta._key;
            max_pos = pos;
        }

    }

    return {max_key, max_pos};
}

void BeladySampleCache::evict(const uint64_t & t) {
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