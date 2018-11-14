//
// Created by zhenyus on 11/8/18.
//

#include "request.h"
#include "lfo.h"

bool LFOCache::lookup(SimpleRequest& _req) {
    auto & req = static_cast<ClassifiedRequest&>(_req);
    auto it = _cacheMap.left.find(std::make_pair(req.get_id(), req.get_size()));
    if (it != _cacheMap.left.end()) {
        // log hit
        _cacheMap.left.replace_data(it, (req.rehit_probability));
        return true;
    }
    return false;
}

void LFOCache::admit(SimpleRequest& _req) {
    auto & req = static_cast<ClassifiedRequest&>(_req);

    const uint64_t size = req.get_size();
    // object feasible to store?
    if (size > _cacheSize) {
        LOG("L", _cacheSize, req.get_id(), size);
        return;
    }

    // admit new object
    _cacheMap.insert({{req.get_id(), req.get_size()}, req.rehit_probability});
    _currentSize += size;

    // check eviction needed
    while (_currentSize > _cacheSize) {
        evict();
    }
}

void LFOCache::evict() {
    auto right_iter = _cacheMap.right.begin();
    _currentSize -= right_iter->second.second;
    _cacheMap.right.erase(right_iter);
}