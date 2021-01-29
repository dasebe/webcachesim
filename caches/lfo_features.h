//
// Created by Arnav Garg on 2019-11-28.
//

#ifndef WEBCDN_LFO_FEATURES_H
#define WEBCDN_LFO_FEATURES_H

#include <request.h>
#include <vector>

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


    LFOFeature() {}

    LFOFeature(IdType _id, uint64_t _size, uint64_t _time)
            : id(_id),
              size(_size),
              timestamp(_time)
    {}

    ~LFOFeature() {}

    std::vector<double> get_vector() {
        std::vector<double> features;
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

#endif //WEBCDN_LFO_FEATURES_H
