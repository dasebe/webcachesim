//
// Created by zhenyus on 11/8/18.
//

#include "request.h"
#include "belady.h"

bool BeladyCache::lookup(SimpleRequest& _req) {
    auto & req = dynamic_cast<AnnotatedRequest&>(_req);
    _valueMap.emplace(req._next_t, req._id);
    auto if_hit = _cacheMap.find(req._id) !=_cacheMap.end();
    return if_hit;
}

void BeladyCache::admit(SimpleRequest& _req) {
    auto & req = dynamic_cast<AnnotatedRequest&>(_req);
    const uint64_t & size = req._size;

    // object feasible to store?
    if (size > _cacheSize) {
        LOG("L", _cacheSize, req._id, size);
        return;
    }

    // admit new object
    _cacheMap.insert({req._id, req._size});
    _currentSize += size;

    // check eviction needed
    while (_currentSize > _cacheSize) {
        evict();
    }
}

void BeladyCache::evict() {
    auto it = _valueMap.begin();
    auto iit = _cacheMap.find(it->second);
    if (iit != _cacheMap.end()) {
        _currentSize -= iit->second;
        _cacheMap.erase(iit);
    }
    _valueMap.erase(it);
}