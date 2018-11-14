//
// Created by zhenyus on 11/8/18.
//

#include "request.h"
#include "belady.h"

bool BeladyCache::lookup(SimpleRequest& _req) {
    AnnotatedRequest & req = static_cast<AnnotatedRequest&>(_req);
    auto it = _cacheMap.left.find(std::make_pair(req.get_id(), req.get_size()));
    if (it != _cacheMap.left.end()) {
        // log hit
        _cacheMap.left.replace_data(it, req._next_t);
        return true;
    }
    return false;
}

void BeladyCache::admit(SimpleRequest& _req) {
    AnnotatedRequest & req = static_cast<AnnotatedRequest&>(_req);

    const uint64_t size = req.get_size();
    // object feasible to store?
    if (size > _cacheSize) {
        LOG("L", _cacheSize, req.get_id(), size);
        return;
    }

    // admit new object
    _cacheMap.insert({{req.get_id(), req.get_size()}, req._next_t});
    _currentSize += size;

    // check eviction needed
    while (_currentSize > _cacheSize) {
        evict();
    }
}

void BeladyCache::evict() {
    auto right_iter = _cacheMap.right.begin();
    _currentSize -= right_iter->second.second;
    _cacheMap.right.erase(right_iter);
}