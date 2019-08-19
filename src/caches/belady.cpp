//
// Created by zhenyus on 11/8/18.
//

#include "request.h"
#include "belady.h"

bool BeladyCache::lookup(SimpleRequest& _req) {
    auto & req = dynamic_cast<AnnotatedRequest&>(_req);
    _valueMap.emplace(req._next_seq, req._id);
    auto if_hit = _cacheMap.find(req._id) !=_cacheMap.end();
    //time to delete the past next_seq
    _valueMap.erase(_req._t);

    {
        current_t = req._t;
    }

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
        {   //record eviction decision quality
            unsigned int decision_qulity =
                    static_cast<double>(it->first - current_t) / (_cacheSize * 1e6 / byte_million_req);
            decision_qulity = min((unsigned int) 255, decision_qulity);
            eviction_qualities.emplace_back(decision_qulity);
            eviction_logic_timestamps.emplace_back(current_t / 10000);
        }
        _currentSize -= iit->second;
        _cacheMap.erase(iit);
    }
    _valueMap.erase(it);
}