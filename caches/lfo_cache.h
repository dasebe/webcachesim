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
#include "lfo_features.h"
//#include "adaptsize_const.h"

typedef std::list<CacheObject>::iterator ListIteratorType;
typedef std::unordered_map<CacheObject, ListIteratorType> lruCacheMapType;



class LFOCache : public Cache {

private:
    std::unordered_map<IdType, LFOFeature> id2feature;

protected:
    std::list<CacheObject> _cacheList;
    lruCacheMapType _cacheMap;

    virtual void hit(lruCacheMapType::const_iterator it, uint64_t size);
    void update_timegaps(LFOFeature & feature, uint64_t new_time);


public:
    LFOCache(): Cache() {}
    virtual ~LFOCache() {};

    virtual bool lookup(SimpleRequest* req);
    virtual void admit(SimpleRequest* req);
    virtual void evict(SimpleRequest* req);
    virtual void evict();
    virtual SimpleRequest* evict_return();
    LFOFeature get_lfo_feature(SimpleRequest* req);
};

static Factory<LFOCache> factoryLFO("LFO");


#endif //WEBCDN_LFO_CACHE_H
