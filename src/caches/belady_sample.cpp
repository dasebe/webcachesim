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
        uint32_t &pos_idx = it->second;
        meta_holder[pos_idx].update(req._t, req._next_seq);

        if (memorize_sample && memorize_sample_keys.find(req._id) != memorize_sample_keys.end() &&
            req._next_seq - current_t <= threshold)
            memorize_sample_keys.erase(req._id);

        return true;
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
        key_map.insert({req._id, (uint32_t) meta_holder.size()});
        meta_holder.emplace_back(req._id, req._size, req._t, req._next_seq);
        _currentSize += size;
    }
    // check more eviction needed?
    while (_currentSize > _cacheSize) {
        evict();
    }
}


pair<uint64_t, uint32_t> BeladySampleCache::rank() {
    uint64_t max_future_interval = 0;
    uint64_t max_key;
    uint32_t max_pos;

    if (memorize_sample) {
        for (auto it = memorize_sample_keys.cbegin(); it != memorize_sample_keys.end();) {
            auto &key = *it;
            auto &pos = key_map.find(key)->second;
            auto &meta = meta_holder[pos];
            uint64_t &past_timestamp = meta._past_timestamp;
            if (meta._future_timestamp - current_t <= threshold) {
                it = memorize_sample_keys.erase(it);
            } else {
                auto future_interval = 2 * threshold;
                //select the first one: random one
                if (future_interval > max_future_interval) {
                    max_future_interval = 2 * threshold;
                    max_key = key;
                    max_pos = pos;
                }
                ++it;
            }
        }
    }

    uint32_t rand_idx = _distribution(_generator) % meta_holder.size();
    uint n_sample = min(sample_rate, (uint32_t) meta_holder.size());

    for (uint32_t i = 0; i < n_sample; i++) {
        uint32_t pos = (i + rand_idx) % meta_holder.size();
        auto &meta = meta_holder[pos];
        //fill in past_interval
        uint64_t &past_timestamp = meta._past_timestamp;

        if (memorize_sample && memorize_sample_keys.find(meta._key) != memorize_sample_keys.end())
            continue;

        uint64_t future_interval;
        if (meta._future_timestamp - current_t > threshold) {
            future_interval = 2*threshold;
            if (memorize_sample && memorize_sample_keys.size() < sample_rate)
                memorize_sample_keys.insert(meta._key);
        }
        else
            future_interval = meta._future_timestamp - current_t;

        //select the first one: random one
        if (future_interval > max_future_interval) {
            max_future_interval = future_interval;
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

    if (memorize_sample && memorize_sample_keys.find(key) != memorize_sample_keys.end())
        memorize_sample_keys.erase(key);

    _currentSize -= meta_holder[old_pos]._size;
    uint32_t activate_tail_idx = meta_holder.size() - 1;
    if (old_pos !=  activate_tail_idx) {
        //move tail
        meta_holder[old_pos] = meta_holder[activate_tail_idx];
        key_map.find(meta_holder[activate_tail_idx]._key)->second = old_pos;
    }
    meta_holder.pop_back();

    key_map.erase(key);
}