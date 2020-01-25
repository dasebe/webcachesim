//
// Created by zhenyus on 12/17/18.
//

#include "percent_relaxed_belady.h"
#include "utils.h"
#include <assert.h>

using namespace std;


bool PercentRelaxedBeladyCache::lookup(SimpleRequest &_req) {
    auto &req = dynamic_cast<AnnotatedRequest &>(_req);
    current_t = req._t;
    auto it = key_map.find(req._id);
    if (it != key_map.end()) {
        //update past timestamps
        uint32_t &pos_idx = it->second;
        meta_holder[pos_idx].update(req._t, req._next_seq);

        return true;
    }
    return false;
}

void PercentRelaxedBeladyCache::admit(SimpleRequest &_req) {
    AnnotatedRequest &req = static_cast<AnnotatedRequest &>(_req);
    const uint64_t &size = req._size;
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


pair<uint64_t, uint32_t> PercentRelaxedBeladyCache::rank() {
//    vector<pair<uint64_t, uint32_t >> beyond_boundary_key_pos;
//    uint64_t max_future_interval = 0;
//    uint64_t max_key;
//    uint32_t max_pos;


//    if (memorize_sample) {
//        //first pass: move near objects out of the set.
//        for (auto it = memorize_sample_keys.cbegin(); it != memorize_sample_keys.end();) {
//            auto &key = *it;
//            auto &pos = key_map.find(key)->second;
//            auto &meta = meta_holder[pos];
//            uint64_t &past_timestamp = meta._past_timestamp;
//            if (meta._future_timestamp - current_t <= threshold) {
//                it = memorize_sample_keys.erase(it);
//            } else {
//                beyond_boundary_key_pos.emplace_back(pair(key, pos));
//                ++it;
//            }
//        }
//    }

    uint n_sample = min(sample_rate, (uint32_t) meta_holder.size());

    unordered_set<uint64_t> key_set;
    vector<uint64_t> keys;
    vector<uint64_t> scores;
    vector<uint32_t> poses;

    if (memorize_sample) {
        auto n_memorize = memorize_sample_keys.size();
        key_set.reserve(n_sample + n_memorize);
        keys.reserve(n_sample + n_memorize);
        scores.reserve(n_sample + n_memorize);
        poses.reserve(n_sample + n_memorize);
    } else {
        key_set.reserve(n_sample);
        keys.reserve(n_sample);
        scores.reserve(n_sample);
        poses.reserve(n_sample);
    }

    for (uint32_t i = 0; i < n_sample; i++) {
        //true random sample
        uint32_t pos = (i + _distribution(_generator)) % meta_holder.size();
        auto &meta = meta_holder[pos];

//        if (memorize_sample && memorize_sample_keys.find(meta._key) != memorize_sample_keys.end()) {
//            //this key is already in the memorize keys, so we will enumerate it
//            continue;
//        }
        if (key_set.find(meta._key) != key_set.end()) {
            continue;
        }

        key_set.insert(meta._key);
        keys.emplace_back(meta._key);
        scores.emplace_back(meta._future_timestamp);
        poses.emplace_back(pos);

//        uint64_t future_interval;
//        if (meta._future_timestamp - current_t <= threshold) {
//            future_interval = meta._future_timestamp - current_t;
//        } else {
//            beyond_boundary_key_pos.emplace_back(pair(meta._key, pos));
//            if (memorize_sample && memorize_sample_keys.size() < sample_rate) {
//                memorize_sample_keys.insert(meta._key);
//            }
//            continue;
//        }

//        //select the first one: random one
//        if (future_interval > max_future_interval) {
//            max_future_interval = future_interval;
//            max_key = meta._key;
//            max_pos = pos;
//        }
    }

    if (memorize_sample) {
        for (auto &key: memorize_sample_keys) {
            auto it = key_map.find(key);
            assert(it != key_map.end());
            uint32_t &pos = it->second;
            auto &meta = meta_holder[pos];

//        if (memorize_sample && memorize_sample_keys.find(meta._key) != memorize_sample_keys.end()) {
//            //this key is already in the memorize keys, so we will enumerate it
//            continue;
//        }
            if (key_set.find(meta._key) != key_set.end()) {
                continue;
            }

            key_set.insert(meta._key);
            keys.emplace_back(meta._key);
            scores.emplace_back(meta._future_timestamp);
            poses.emplace_back(pos);
        }
    }

    vector<int> index(keys.size(), 0);
    for (int i = 0; i < index.size(); ++i) {
        index[i] = i;
    }

    sort(index.begin(), index.end(),
         [&](const int &a, const int &b) {
             return (scores[a] > scores[b]);
         }
    );

    if (memorize_sample) {
        memorize_sample_keys.clear();
        for (int i = 1; i < index.size() && memorize_sample_keys.size() < sample_rate; ++i) {
            auto &key_idx = index[i];
            auto &key = keys[key_idx];
            memorize_sample_keys.insert(key);
        }
    }

    auto n_candidate = int(ceil(index.size() * p));
    auto selected_perm = _distribution(_generator) % n_candidate;
    auto selected_idx = index[selected_perm];


    return {keys[selected_idx], poses[selected_idx]};


//    if (beyond_boundary_key_pos.empty()) {
//        return {max_key, max_pos};
//    } else {
//        auto rand_id = _distribution(_generator) % beyond_boundary_key_pos.size();
//        auto &item = beyond_boundary_key_pos[rand_id];
//        return {item.first, item.second};
//    }
}

void PercentRelaxedBeladyCache::evict() {
//    static uint counter = 0;
    auto epair = rank();
    uint64_t &key = epair.first;
    uint32_t &old_pos = epair.second;

    //record meta's future interval

#ifdef EVICTION_LOGGING
    {
        auto &meta = meta_holder[old_pos];
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
    if (old_pos != activate_tail_idx) {
        //move tail
        meta_holder[old_pos] = meta_holder[activate_tail_idx];
        key_map.find(meta_holder[activate_tail_idx]._key)->second = old_pos;
    }
    meta_holder.pop_back();

    key_map.erase(key);
}