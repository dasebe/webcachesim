//
// Created by zhenyus on 12/17/18.
//

#include "belady_sample.h"
#include "utils.h"

using namespace std;


bool BeladySampleCache::lookup(SimpleRequest &_req) {
    auto & req = dynamic_cast<AnnotatedRequest &>(_req);
    current_t = req._t;
    auto it = key_map.find(req._id);
    if (it != key_map.end()) {
        //update past timestamps
        bool & list_idx = it->second.first;
        uint32_t & pos_idx = it->second.second;
        meta_holder[list_idx][pos_idx].update(req._t, req._next_seq);

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
        meta_holder[0].emplace_back(req._id, req._size, req._t, req._next_seq);
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
        auto epair = rank();
        auto & key0 = epair.first;
        auto & pos0 = epair.second;
        auto & pos1 = it->second.second;
        _currentSize = _currentSize - meta_holder[0][pos0]._size + req._size;
        swap(meta_holder[0][pos0], meta_holder[1][pos1]);
        swap(it->second, key_map.find(key0)->second);
    }
    // check more eviction needed?
    while (_currentSize > _cacheSize) {
        evict();
    }
}


pair<uint64_t, uint32_t> BeladySampleCache::rank() {
    uint64_t max_future_interval;
    uint64_t min_past_timetamp;
    uint64_t max_key;
    uint32_t max_pos;

    uint32_t rand_idx = _distribution(_generator) % meta_holder[0].size();
    uint n_sample = min(sample_rate, (uint32_t) meta_holder[0].size());

    for (uint32_t i = 0; i < n_sample; i++) {
        uint32_t pos = (i+rand_idx)%meta_holder[0].size();
        auto & meta = meta_holder[0][pos];
        //fill in past_interval
        uint64_t &past_timestamp = meta._past_timestamp;

        uint64_t future_interval;
        if (meta._future_timestamp - current_t > threshold)
            future_interval = 2*threshold;
        else
            future_interval = meta._future_timestamp - current_t;

        if (!i || future_interval > max_future_interval ||
            ((future_interval == max_future_interval) && (past_timestamp < min_past_timetamp))) {
            max_future_interval = future_interval;
            min_past_timetamp = past_timestamp;
            max_key = meta._key;
            max_pos = pos;
        }
    }
    return {max_key, max_pos};
}

void BeladySampleCache::evict() {
//    static uint counter = 0;
    auto epair = rank();
    uint64_t & key = epair.first;
    uint32_t & old_pos = epair.second;

    //record meta's future interval

#ifdef EVICTION_LOGGING
    {
        auto &meta = meta_holder[0][old_pos];
        //record eviction decision quality
        unsigned int decision_qulity =
                static_cast<double>(meta._future_timestamp - current_t) / (_cacheSize * 1e6 / byte_million_req);
        decision_qulity = min((unsigned int) 255, decision_qulity);
        eviction_distances.emplace_back(decision_qulity);
    }
#endif

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