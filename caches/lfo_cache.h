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
#include <LightGBM/c_api.h>
#include <queue>

typedef std::unordered_map<IdType, CacheObject> lfoCacheMapType;

struct GreaterCacheObject {
    bool operator()(CacheObject const& p1, CacheObject const& p2)
    {
        // return "true" if "p1" is ordered
        // before "p2", for example:
        return p1.dvar > p2.dvar;
    }
};

class LFOCache : public Cache {

private:
    BoosterHandle boosterHandle = nullptr;
    int numIterations;
    DatasetHandle dataHandle = nullptr;
    double threshold = 0.5;

protected:
    std::list<CacheObject> _cacheList;
    lfoCacheMapType _cacheMap;
    std::priority_queue<CacheObject, std::vector<CacheObject>, GreaterCacheObject> _cacheObjectMinpq;

//    void update_timegaps(LFOFeature & feature, uint64_t new_time);
    void train_lightgbm(std::vector<std::vector<double>> features, std::vector<double> labels);
    double run_lightgbm(std::vector<double> feature);


public:
    LFOCache(): Cache() {}
    virtual ~LFOCache() {};

    virtual bool lookup(SimpleRequest* req);
    virtual void admit(SimpleRequest* req);
    virtual void evict(SimpleRequest* req);
    virtual void evict();
    virtual SimpleRequest* evict_return();
//    LFOFeature get_lfo_feature(SimpleRequest* req);
};

static Factory<LFOCache> factoryLFO("LFO");


#endif //WEBCDN_LFO_CACHE_H
