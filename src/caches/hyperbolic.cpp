//
// Created by zhenyus on 12/17/18.
//

#include "hyperbolic.h"

bool HyperbolicCache::lookup(SimpleRequest &req) {
    auto it = key_map.find(req._id);
    if (it != key_map.end()) {
        //update past timestamps
        uint32_t & pos_idx = it->second;
        meta_holder[pos_idx].update();
        return true;
    }
    return false;
}

void HyperbolicCache::admit(SimpleRequest &req) {
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

    const uint64_t & size = req._size;
    // object feasible to store?
    if (size > _cacheSize) {
        LOG("L", _cacheSize, req.get_id(), size);
        return;
    }

    LOG("a", 0, _req._id, _req._size);
    //fresh insert
    key_map.insert({req._id, (uint32_t) meta_holder.size()});
    meta_holder.emplace_back(req._id, req._size, req._t);
    _currentSize += size;
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


pair<uint64_t, uint32_t> HyperbolicCache::rank(const uint64_t & t) {
    uint64_t worst_key;
    uint32_t worst_pos;
    double worst_score;

    uint32_t rand_idx = _distribution(_generator) % meta_holder.size();
    uint n_sample = min(meta_holder.size(), sample_rate);

    for (uint32_t i = 0; i < n_sample; i++) {
        uint32_t pos = (i+rand_idx)%meta_holder.size();
        auto & meta = meta_holder[pos];

        double score = meta._n_requests / (t - meta._insertion_time+1);  //add 1 to dividend to prevent 0

        if (!i || (score < worst_score)) {
            worst_key = meta._key;
            worst_pos = pos;
            worst_score = score;
        }
    }
    LOG("e", 0, meta_holder[0][worst_pos]._key, meta_holder[0][worst_pos]._size);

    return {worst_key, worst_pos};
}

void HyperbolicCache::evict(const uint64_t & t) {
    auto epair = rank(t);
    uint64_t & key = epair.first;
    uint32_t & old_pos = epair.second;

    //remove timestamp. Forget
    auto & meta = meta_holder[old_pos];

    //update state before deletion
    _currentSize -= meta._size;
    key_map.erase(key);

    uint32_t activate_tail_idx = meta_holder.size()-1;
    if (old_pos !=  activate_tail_idx) {
        //move tail
        meta_holder[old_pos] = meta_holder[activate_tail_idx];
        key_map.find(meta_holder[activate_tail_idx]._key)->second= old_pos;
    }
    meta_holder.pop_back();
}
