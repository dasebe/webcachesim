//
// Created by zhenyus on 1/1/19.
//

#include "lfu_sample.h"


bool LFUSampleCache::lookup(SimpleRequest &_req) {
    auto & req = static_cast<AnnotatedRequest &>(_req);

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

void LFUSampleCache::admit(SimpleRequest &_req) {
    //cache state
#ifdef CDEBUG
    {
        DPRINTF("cache state: \n");
        vector<uint64_t> cache_state;
        for (auto &it: meta_holder[0])
            cache_state.push_back(it._key);
        sort(cache_state.begin(), cache_state.end());
        for (auto &it: cache_state)
            DPRINTF("%lu\n", it);
    }
#endif

    AnnotatedRequest & req = static_cast<AnnotatedRequest &>(_req);
    const uint64_t & size = req._size;
    // object feasible to store?
    if (size > _cacheSize) {
        LOG("L", _cacheSize, req.get_id(), size);
        return;
    }

    LOG("a", 0, _req._id, _req._size);
    auto it = key_map.find(req._id);
    if (it == key_map.end()) {
        //fresh insert
        key_map.insert({req._id, {0, (uint32_t) meta_holder[0].size()}});
        meta_holder[0].emplace_back(req._id, req._size, req._t, req._next_t);
        _currentSize += size;
        if (_currentSize <= _cacheSize) {
#ifdef CDEBUG
            {
                DPRINTF("cache state: \n");
                vector<uint64_t> cache_state;
                for (auto &it: meta_holder[0])
                    cache_state.push_back(it._key);
                sort(cache_state.begin(), cache_state.end());
                for (auto &it: cache_state)
                    DPRINTF("%lu\n", it);
            }
#endif
            return;
        }
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
#ifdef CDEBUG
        {
            DPRINTF("cache state: \n");
            vector<uint64_t> cache_state;
            for (auto &it: meta_holder[0])
                cache_state.push_back(it._key);
            sort(cache_state.begin(), cache_state.end());
            for (auto &it: cache_state)
                DPRINTF("%lu\n", it);
        }
#endif
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
#ifdef CDEBUG
    {
        DPRINTF("cache state: \n");
        vector<uint64_t> cache_state;
        for (auto &it: meta_holder[0])
            cache_state.push_back(it._key);
        sort(cache_state.begin(), cache_state.end());
        for (auto &it: cache_state)
            DPRINTF("%lu\n", it);
    }
#endif

}


pair<uint64_t, uint32_t> LFUSampleCache::rank(const uint64_t & t) {
    uint64_t max_key;
    uint32_t max_pos;
    uint64_t min_past_timestamp;
    uint min_n_request;

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
        uint n_request = 0;
        for (j = 0; j < meta._past_timestamp_idx && j < n_past_intervals; ++j) {
            uint8_t past_timestamp_idx = (meta._past_timestamp_idx - 1 - j) % n_past_intervals;
            uint64_t past_interval = t - meta._past_timestamps[past_timestamp_idx];
            if (past_interval <= threshold)
                ++n_request;
            else
                //all other requests are older
                break;
        }

        uint8_t oldest_idx = (meta._past_timestamp_idx - (uint8_t) 1) % n_past_intervals;
        uint64_t past_timestamp = meta._past_timestamps[oldest_idx];

        if (!i || (min_n_request > n_request) || ((min_n_request == n_request) &&
            (past_timestamp < min_past_timestamp))) {
            max_key = meta._key;
            max_pos = pos;
            min_past_timestamp = past_timestamp;
            min_n_request = n_request;
        }
    }
    LOG("e", 0, meta_holder[0][max_pos]._key, meta_holder[0][max_pos]._size);

    return {max_key, max_pos};
}

void LFUSampleCache::evict(const uint64_t & t) {
    auto epair = rank(t);
    uint64_t & key = epair.first;
    uint32_t & old_pos = epair.second;

    //remove timestamp. Forget
    auto & meta = meta_holder[0][old_pos];
    if (forget_on_evict)
        meta._past_timestamp_idx = 0;

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
