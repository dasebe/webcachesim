//
// Created by zhenyus on 11/8/18.
//

#include "request.h"
#include "lfo2.h"

bool LFOACache::lookup(SimpleRequest& _req) {
    AnnotatedRequest & req = static_cast<AnnotatedRequest&>(_req);
    auto it = _cacheMap.left.find(req._id);
    if (it != _cacheMap.left.end()) {
        // log hit
        _cacheMap.left.replace_data(it, req._next_t);
        return true;
    }
    return false;
}

void LFOACache::admit(SimpleRequest& _req) {
    AnnotatedRequest & req = static_cast<AnnotatedRequest&>(_req);

    const uint64_t & size = req._size;
    // object feasible to store?
    if (size > _cacheSize) {
        LOG("L", _cacheSize, req.get_id(), size);
        return;
    }

    // admit new object
    _cacheMap.insert({req._id, req._next_t});
    object_size.insert({req._id, req._size});
    _currentSize += size;

    // check eviction needed
    while (_currentSize > _cacheSize) {
        evict();
    }
}

void LFOACache::evict() {
    auto right_iter = _cacheMap.right.begin();
    _currentSize -= object_size.find(right_iter->second)->second;
    object_size.erase(right_iter->first);
    _cacheMap.right.erase(right_iter);
}

bool LFOBCache::lookup(SimpleRequest& _req) {
//    auto & req = static_cast<ClassifiedRequest&>(_req);
//    auto it = _cacheMap.left.find(std::make_pair(req.get_id(), req.get_size()));
//    if (it != _cacheMap.left.end()) {
//        // log hit
//        _cacheMap.left.replace_data(it, (req.rehit_probability));
//        return true;
//    }
    return false;
}

void LFOBCache::admit(SimpleRequest& _req) {
//    auto & req = static_cast<ClassifiedRequest&>(_req);
//
//    const uint64_t size = req.get_size();
//    // object feasible to store?
//    if (size > _cacheSize) {
//        LOG("L", _cacheSize, req.get_id(), size);
//        return;
//    }
//
//    // admit new object
//    _cacheMap.insert({{req.get_id(), req.get_size()}, req.rehit_probability});
//    _currentSize += size;
//
//    // check eviction needed
//    while (_currentSize > _cacheSize) {
//        evict();
//    }
}

void LFOBCache::evict() {
//    auto right_iter = _cacheMap.right.begin();
//    _currentSize -= right_iter->second.second;
//    _cacheMap.right.erase(right_iter);
}
