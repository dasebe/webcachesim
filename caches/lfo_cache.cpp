//
// Created by Arnav Garg on 2019-11-28.
//

#include "lfo_cache.h"

using namespace std;

double LFOCache::run_lightgbm(std::vector<double> feature) {
    double* featureVector = new double[feature.size()];
    for (int i = 0; i < feature.size(); i++) {
        featureVector[i] = feature[i];
    }
    int64_t predictionsLength = 1;
    double predictions = 1;

    LGBM_BoosterPredictForMatSingleRow(boosterHandle,
                                       featureVector,
                                       C_API_DTYPE_FLOAT64,
                                       feature.size(),
                                       1,
                                       C_API_PREDICT_RAW_SCORE,
                                       -1,
                                       "",
                                       &predictionsLength,
                                       &predictions);

    if (predictionsLength != 1) {
        std::cout << "predictionsLength returned more than 1 value for input";
    }

    return predictions;
}

void LFOCache::train_lightgbm(std::vector<std::vector<double>> features, std::vector<double> opt_decisions) {

    if (features.empty()) {
        std::cout << "No features sent to train!" << std::endl;
    }

    int numSamples = features.size();
    int featureLength = features[0].size();

    double featureVector[numSamples][featureLength];
    double labels[opt_decisions.size()];

    for (int i = 0; i < numSamples; i++) {
        for (int j = 0; j < featureLength; j++) {
            featureVector[i][j] = features[i][j];
        }
    }

    for (int i = 0; i < features.size(); i++) {
        labels[i] = opt_decisions[i];
    }


    int isdataSetLoaded = 0;

    if (dataHandle == nullptr){
        isdataSetLoaded = LGBM_DatasetCreateFromMat(featureVector, C_API_DTYPE_FLOAT64, numSamples,
                                                    featureLength, 1, "", nullptr, &dataHandle);
    } else {
        isdataSetLoaded = LGBM_BoosterResetTrainingData(dataHandle, featureVector);
    }
    if (isdataSetLoaded != 0) {
        std::cout << "Loading dataset failed\n";
    }

    LGBM_DatasetSetField(dataHandle, "label", labels, numSamples, C_API_DTYPE_FLOAT32);
    int isLearnerCreated = LGBM_BoosterCreate(dataHandle, "", &boosterHandle);

    if (isLearnerCreated != 0) {
        std::cout << "Creating learner failed\n";
    }

    for (int i=0 ; i < numIterations; i++) {
        int isFinished;
        int isUpdated = LGBM_BoosterUpdateOneIter(boosterHandle, &isFinished);
        if (isUpdated != 0) {
            std::cout << "Failed to update at iteration number " << i << "\n";
        }
        if (isFinished == 0) {
            std::cout << "No further gain, cannot split anymore" << std::endl;
            break;
        }
    }
    dataHandle = nullptr;
}


//void LFOCache::update_timegaps(LFOFeature & feature, uint64_t new_time) {
//
//    uint64_t time_diff = new_time - feature.timestamp;
//
//    for (auto it = feature.timegaps.begin(); it != feature.timegaps.end(); it++) {
//        *it = *it + time_diff;
//    }
//
//    feature.timegaps.push_back(new_time);
//
//    if (feature.timegaps.size() > 50) {
//        auto start = feature.timegaps.begin();
//        feature.timegaps.erase(start);
//    }
//}

//LFOFeature LFOCache::get_lfo_feature(SimpleRequest* req) {
//    if (id2feature.find(req->getId()) != id2feature.end()) {
//        LFOFeature& feature = id2feature[req->getId()];
//        update_timegaps(feature, req->getTimeStamp());
//        feature.timestamp = req->getTimeStamp();
//    } else {
//        LFOFeature feature(req->getId(), req->getSize(), req->getTimeStamp());
//        feature.available_cache_size = getFreeBytes();
//        id2feature[req->getId()] = feature;
//    }
//
//    return id2feature[req->getId()];
//}

bool LFOCache::lookup(SimpleRequest* req) {
    auto it = _cacheMap.find(req->getId());
    if(it != _cacheMap.end()) return true;
    return false;
};

void LFOCache::admit(SimpleRequest* req) {
    const uint64_t size = req->getSize();
    auto lfoFeature = get_lfo_feature(req);

    if (run_lightgbm(lfoFeature.get_vector()) >= threshold) {
        while (_currentSize + size > _cacheSize) {
            evict();
        }
    }
    CacheObject obj(req);
    _cacheMap.insert({lfoFeature.id, obj});
    _cacheObjectMinpq.push(obj);
    _currentSize += size;

};

void LFOCache::evict(SimpleRequest* req) {
    throw "Random eviction not supported for LFOCache";
};

void LFOCache::evict() {
    evict_return();
}

SimpleRequest* LFOCache::evict_return() {
    if(_cacheObjectMinpq.size() > 0){
        CacheObject obj = _cacheObjectMinpq.top();
        _currentSize -= obj.size;
        _cacheMap.erase(obj.id);
        _cacheObjectMinpq.pop();
        SimpleRequest *req = new SimpleRequest(obj.id,obj.size);
        return req;
    }
    return NULL;
}
