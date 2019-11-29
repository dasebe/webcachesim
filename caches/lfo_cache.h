//
// Created by Arnav Garg on 2019-11-28.
//

#ifndef WEBCDN_LFO_CACHE_H
#define WEBCDN_LFO_CACHE_H

#include <list>
#include <vector>
#include <unordered_map>
#include "cache.h"
#include "cache_object.h"
#include "adaptsize_const.h"

typedef std::list<CacheObject>::iterator ListIteratorType;
typedef std::unordered_map<CacheObject, ListIteratorType> lruCacheMapType;


enum OptimizationGoal {
    OBJECT_HIT_RATIO,
    BYTE_HIT_RATIO
};


struct LFOFeature {
    IdType id;
    uint64_t size;
    OptimizationGoal optimizationGoal;
    uint64_t timestamp;
    std::vector<uint64_t> timegaps;
    uint64_t available_cache_size;

    LFOFeature(IdType _id, uint64_t _size, uint64_t _time)
        : id(_id),
          size(_size),
          timestamp(_time)
    {}

    std::vector<uint8_t> get_vector() {
        std::vector<uint8_t> features;
        features.push_back(size);
        features.push_back((optimizationGoal == BYTE_HIT_RATIO)? 1 : size);
        features.push_back(available_cache_size);

        for (int i = timegaps.size();  i < 50; i++) {
            features.push_back(0);
        }

        for (auto it = timegaps.begin(); it != timegaps.end(); it++) {
            features.push_back(*it);
        }

        return features;
    }
};


class LFOCache : public Cache {

private:
    std::unordered_map<IdType, LFOFeature> id2feature;
    void update_timegaps(LFOFeature & feature, uint64_t new_time);

protected:
    std::list<CacheObject> _cacheList;
    lruCacheMapType _cacheMap;

    virtual void hit(lruCacheMapType::const_iterator it, uint64_t size);
    LFOFeature get_lfo_feature(SimpleRequest* req);


public:
    LFOCache(): Cache() {}
    virtual ~LFOCache() {}

    virtual bool lookup(SimpleRequest* req);
    virtual void admit(SimpleRequest* req);
    virtual void evict(SimpleRequest* req);
    virtual void evict();
    virtual SimpleRequest* evict_return();
};

static Factory<LFOCache> factoryLFO("LFO");


#endif //WEBCDN_LFO_CACHE_H
