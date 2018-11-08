//
// Created by zhenyus on 11/8/18.
//

#include "request.h"
#include "belady.h"

bool BeladyCache::lookup(SimpleRequest& _req) {
    AnnotatedRequest & req = static_cast<AnnotatedRequest&>(_req);
    auto it = _cacheMap.left.find(std::make_pair(req.getId(), req.getSize()));
    if (it != _cacheMap.left.end()) {
        // log hit
        LOG("h", 0, obj.id, obj.size);
        _cacheMap.left.replace_data(it, (req.get_next_t()));
        return true;
    }
    return false;
}

void BeladyCache::admit(SimpleRequest& _req) {
    AnnotatedRequest & req = static_cast<AnnotatedRequest&>(_req);

    const uint64_t size = req.getSize();
    // object feasible to store?
    if (size > _cacheSize) {
        LOG("L", _cacheSize, req.getId(), size);
        return;
    }

    // admit new object
    _cacheMap.insert({{req.getId(), req.getSize()}, req.get_next_t()});
    _currentSize += size;

    LOG("a", _currentSize, obj.id, obj.size);
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