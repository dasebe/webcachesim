//
// Created by zhenyus on 11/8/18.
//

#include "random_variants.h"

using namespace std;


// from boost hash combine: hashing of pairs for unordered_maps

namespace std {
    template<typename S, typename T>
    struct hash<pair<S, T>> {
        inline size_t operator()(const pair<S, T> &v) const {
            size_t seed = 0;
            hash_combine(seed, v.first);
            hash_combine(seed, v.second);
            return seed;
        }
    };
}


bool RandomCache::lookup(SimpleRequest &req) {
    return key_space.exist({req.getId(), req.getSize()});
}

void RandomCache::admit(SimpleRequest &req) {
    const uint64_t size = req.getSize();
    // object feasible to store?
    if (size > _cacheSize) {
        LOG("L", _cacheSize, req.getId(), size);
        return;
    }
    // admit new object
    key_space.insert({req.getId(), req.getSize()});
    _currentSize += size;

    LOG("a", _currentSize, obj.id, obj.size);
    // check eviction needed
    while (_currentSize > _cacheSize) {
        evict();
    }

}

void RandomCache::evict() {
    auto key = key_space.pickRandom();
    key_space.erase(key);
    _currentSize -= key.second;
}


