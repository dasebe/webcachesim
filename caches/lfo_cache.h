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
#include <lib/dlib/dlib/svm.h>

typedef std::unordered_map<IdType, CacheObject> lfoCacheMapType;

typedef dlib::matrix<double, 0, 1> sample_type;
typedef dlib::radial_basis_kernel<sample_type> kernel_type;
typedef dlib::decision_function<kernel_type> dec_funct_type;
typedef dlib::normalized_function<dec_funct_type> funct_type;

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
    funct_type rvm_learned_function;
    funct_type svm_learned_function;

    bool rvm_cross_validate;
    bool svm_cross_validate;

    double rvm_gamma;
    double svm_gamma;
    double svm_c;

protected:
    std::list<CacheObject> _cacheList;
    lfoCacheMapType _cacheMap;
    std::priority_queue<CacheObject, std::vector<CacheObject>, GreaterCacheObject> _cacheObjectMinpq;

    void train_lightgbm(std::vector<std::vector<double>> features, std::vector<double> labels);
    double run_lightgbm(std::vector<double> feature);

    void train_rvm(std::vector<std::vector<double>> features, std::vector<double> labels);
    double run_rvm(std::vector<double> feature);

    void train_svm(std::vector<std::vector<double>> features, std::vector<double> labels);
    double run_svm(std::vector<double> feature);

public:
    LFOCache(): Cache() {
        rvm_cross_validate = true;
        svm_cross_validate = true;
    }
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
