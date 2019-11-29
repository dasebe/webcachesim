//
// Created by Arnav Garg on 2019-11-28.
//

#include "lfo_cache.h"

using namespace std;

#define LOG_MSG "[LOG][LFO_CACHE]"


void LFOCache::update_timegaps(LFOFeature & feature, uint64_t new_time) {

    uint64_t time_diff = new_time - feature.timestamp;

    for (auto it = feature.timegaps.begin(); it != feature.timegaps.end(); it++) {
        *it = *it + time_diff;
    }

    feature.timegaps.push_back(new_time);

    if (feature.timegaps.size() > 50) {
        auto start = feature.timegaps.begin();
        feature.timegaps.erase(start);
    }
}

LFOFeature LFOCache::get_lfo_feature(SimpleRequest* req) {
    if (id2feature.find(req->getId()) != id2feature.end()) {
        LFOFeature& feature = id2feature[req->getId()];
        update_timegaps(feature, req->getTimeStamp());
        feature.timestamp = req->getTimeStamp();
    } else {
        LFOFeature feature(req->getId(), req->getSize(), req->getTimeStamp());
        feature.available_cache_size = getFreeBytes();
        id2feature[req->getId()] = feature;
    }

    return id2feature[req->getId()];
}

void LFOCache::hit(lruCacheMapType::const_iterator it, uint64_t size) {

}

bool LFOCache::lookup(SimpleRequest* req) {
//    cout << LOG_MSG << "Lookup." << endl;
    get_lfo_feature(req);
    return true;
};

void LFOCache::admit(SimpleRequest* req) {
    cout << LOG_MSG << "Admit." << endl;
};

void LFOCache::evict(SimpleRequest* req) {

};

void LFOCache::evict() {

}

SimpleRequest* LFOCache::evict_return() {
    return nullptr;
}
