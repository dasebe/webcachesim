//
// Created by zhenyus on 12/17/18.
//

#include "belady_sample.h"
//#include <algorithm>

using namespace std;

void BeladySampleCache::sample(uint64_t &t) {
    //sample list 0
    if (!meta_holder[0].empty()) {
        uint32_t rand_idx = _distribution(_generator) % meta_holder[0].size();
        uint n_sample = min(sample_rate*meta_holder[0].size()/(meta_holder[0].size()+meta_holder[1].size()),
                meta_holder[0].size());

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

//            //print distribution
//            if (!i) {
//                for (uint k = 0; k < n_past_intervals; ++k)
//                    cout << past_intervals[k] << " ";
//                cout << log1p(meta._future_timestamp - t) << endl;
//            }
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
        meta_holder[list_idx][pos_idx].append_past_timestamp(req._t);

        //update future timestamp. Can look only threshold far
        meta_holder[list_idx][pos_idx]._future_timestamp = min(req._next_t, req._t + threshold);

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
        meta_holder[0].emplace_back(req._id, req._size, req._t, min(req._next_t, req._t + threshold));
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