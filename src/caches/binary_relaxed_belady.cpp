//
// Created by zhenyus on 12/17/18.
//

#include "binary_relaxed_belady.h"
#include "utils.h"

using namespace std;


bool BinaryRelaxedBeladyCache::lookup(SimpleRequest &_req) {
    auto &req = dynamic_cast<AnnotatedRequest &>(_req);
    current_t = req._t;
    auto it = key_map.find(req._id);
    if (it != key_map.end()) {
        //update past timestamps
        auto list_idx = it->second.first;
        auto pos = it->second.second;
        if (within_boundary == list_idx) {
            auto &meta = within_boundary_meta[pos];
            meta.update(req);
            if (meta._future_timestamp - current_t >= belady_boundary) {
                meta_remove_and_append(within_boundary_meta, pos, beyond_boundary_meta);
            }
        } else {
            auto &meta = beyond_boundary_meta[pos];
            meta.update(req);
            if (meta._future_timestamp - current_t < belady_boundary) {
                meta_remove_and_append(beyond_boundary_meta, pos, within_boundary_meta);
            }
        }
        return true;
    }
    return false;
}

void BinaryRelaxedBeladyCache::admit(SimpleRequest &_req) {
    AnnotatedRequest &req = static_cast<AnnotatedRequest &>(_req);
    const uint64_t &size = req._size;
    // object feasible to store?
    if (size > _cacheSize) {
        LOG("L", _cacheSize, req.get_id(), size);
        return;
    }

    auto it = key_map.find(req._id);
    if (it == key_map.end()) {
        if (req._next_seq - req._t >= belady_boundary) {
            key_map.insert({req._id, {beyond_boundary, beyond_boundary_meta.size()}});
            beyond_boundary_meta.emplace_back(req);
        } else {
            key_map.insert({req._id, {within_boundary, within_boundary_meta.size()}});
            within_boundary_meta.emplace_back(req);
        }
        _currentSize += size;
    }

    // check more eviction needed?
    while (_currentSize > _cacheSize) {
        evict();
    }
}


pair<BinaryRelaxedBeladyCache::MetaT, uint32_t> BinaryRelaxedBeladyCache::rank() {
    auto rand_idx = _distribution(_generator);
    while (!beyond_boundary_meta.empty()) {
        auto pos = rand_idx % beyond_boundary_meta.size();
        auto &meta = beyond_boundary_meta[pos];
        if (meta._future_timestamp - current_t < belady_boundary) {
            meta_remove_and_append(beyond_boundary_meta, pos, within_boundary_meta);
        } else {
            return {beyond_boundary, pos};
        }
    }
    auto pos = rand_idx % within_boundary_meta.size();
    return {within_boundary, pos};
}

void BinaryRelaxedBeladyCache::evict() {
    auto epair = rank();
    auto &meta_type = epair.first;
    auto &old_pos = epair.second;
//
//    //record meta's future interval
//
//#ifdef EVICTION_LOGGING
//    {
//        auto &meta = meta_holder[0][old_pos];
//        //record eviction decision quality
//        unsigned int decision_qulity =
//                static_cast<double>(meta._future_timestamp - current_t) / (_cacheSize * 1e6 / byte_million_req);
//        decision_qulity = min((unsigned int) 255, decision_qulity);
//        eviction_distances.emplace_back(decision_qulity);
//    }
//#endif

    if (within_boundary == meta_type) {
        meta_remove(within_boundary_meta, old_pos);
    } else {
        meta_remove(beyond_boundary_meta, old_pos);
    }

}