//
// Created by zhenyus on 1/15/19.
//

#include "belady_truncate.h"


uint64_t BeladyTruncateCache::lookup_truncate(AnnotatedRequest &req){
    _valueMap.emplace(req._next_t, req._id);
    auto it = _cacheMap.find(req._id);
    if (it == _cacheMap.end()) {
        //bring in
        admit(req);
        return 0;
    } else {
        uint64_t size_hit = it->second;
        uint64_t size_fetch = req._size - it->second;
        if (size_fetch) {
            it->second = req._size;
            _currentSize += size_fetch;
            while (_currentSize > _cacheSize) {
                evict();
            }
        }
        return size_hit;
    }
}

void BeladyTruncateCache::admit(SimpleRequest& _req) {
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

void BeladyTruncateCache::evict() {
    auto it = _valueMap.begin();
    auto iit = _cacheMap.find(it->second);
    if (iit != _cacheMap.end()) {
        if (_currentSize - iit->second >= _cacheSize) {
            //completely evict
            _currentSize -= iit->second;
            _cacheMap.erase(iit);
            _valueMap.erase(it);
        } else {
            iit->second -= (_currentSize - _cacheSize);
            _currentSize = _cacheSize;
        }
        return;
    }
    _valueMap.erase(it);
}
