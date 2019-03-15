//
// Created by zhenyus on 11/8/18.
//

#include "random_variants.h"
#include <algorithm>
#include "utils.h"

using namespace std;


bool RandomCache::lookup(SimpleRequest &req) {
    return key_space.exist(req._id);
}

void RandomCache::admit(SimpleRequest &req) {
    const uint64_t & size = req._size;
    // object feasible to store?
    if (size > _cacheSize) {
        LOG("L", _cacheSize, req.get_id(), size);
        return;
    }
    // admit new object
    key_space.insert(req._id);
    object_size.insert({req._id, req._size});
    _currentSize += size;

    // check eviction needed
    while (_currentSize > _cacheSize) {
        evict();
    }
}

void RandomCache::evict() {
    auto key = key_space.pickRandom();
    key_space.erase(key);
    auto & size = object_size.find(key)->second;
    _currentSize -= size;
    object_size.erase(key);
}


