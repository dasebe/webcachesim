//
// Created by zhenyus on 11/18/18.
//

#include "ucb.h"
#include "request.h"
#include "cmath"

double upper_bound(int step, int num_plays) {
    return sqrt(2*log((step+1))/num_plays);
}

bool UCBCache::lookup(SimpleRequest &req) {
    KeyT key = req._id;
    ++t;
    //update plays
    auto it = mlcache_plays.find(key);
    if (it != mlcache_plays.end())
        it->second += 1;
    else
        mlcache_plays.insert({key, 1});

    auto it_score = mlcache_score.left.find(key);
    if (it_score != mlcache_score.left.end()) {
        auto plays = mlcache_plays.find(key);
        double score = it_score->second
                + upper_bound(t-1, plays->second)
                - upper_bound(t, plays->second) * plays->second;
        mlcache_score.left.replace_data(it_score, score);
        return true;
    }
    else
        return false;
}

void UCBCache::admit(SimpleRequest& req) {

    const uint64_t size = req.get_size();
    // object feasible to store?
    if (size > _cacheSize) {
        LOG("L", _cacheSize, req.get_id(), size);
        return;
    }

    // admit new object
    KeyT key = req._id;
    _currentSize += size;
    size_map[key] = size;
    double score = - upper_bound(t, mlcache_plays.find(key)->second);
    mlcache_score.insert({key, score});

    // check eviction needed
    while (_currentSize > _cacheSize) {
        evict();
    }
}

void UCBCache::evict() {
    auto right_iter = mlcache_score.right.begin();
    auto key = right_iter->second;
    auto size = size_map[key];
    _currentSize -= size;
    size_map.erase(key);
    mlcache_score.right.erase(right_iter);

}
