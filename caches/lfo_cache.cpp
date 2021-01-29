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

double LFOCache::run_rvm(std::vector<double> feature) {
    sample_type sample;

    for(auto j=0; j<feature.size(); j++){
        sample(j) = feature.at(j);
    }

    return rvm_learned_function(sample);
}

/**
 * Reference: http://dlib.net/rvm_ex.cpp.html
 * @param features
 * @param labels
 */
void LFOCache::train_rvm(std::vector<std::vector<double>> features, std::vector<double> labels) {
    std::vector<sample_type> samples;

    for(auto i = 0; i < features.size(); i++){
        sample_type samp;
        samp.set_size(features.size());

        auto feature = features.at(i);

        for(auto j=0; j<feature.size(); j++){
            samp(j) = feature.at(j);
        }

        samples.push_back(samp);
    }

    dlib::vector_normalizer<sample_type> normalizer;
    // let the normalizer learn the mean and standard deviation of the samples
    normalizer.train(samples);
    // now normalize each sample
    for (unsigned long i = 0; i < samples.size(); ++i)
        samples[i] = normalizer(samples[i]);

    dlib::krr_trainer<kernel_type> trainer;

    if(rvm_cross_validate){
        cout << "doing cross validation" << endl;
        double max = 0;
        for (double gamma = 0.000001; gamma <= 1; gamma *= 5)
        {
            // tell the trainer the parameters we want to use
            trainer.set_kernel(kernel_type(gamma));

            // Print out the cross validation accuracy for 3-fold cross validation using the current gamma.
            // cross_validate_trainer() returns a row vector.  The first element of the vector is the fraction
            // of +1 training examples correctly classified and the second number is the fraction of -1 training
            // examples correctly classified.

            auto cross_validation_results = cross_validate_trainer(trainer, samples, labels, 3);

            double class1 = *(cross_validation_results.begin());
            double class2 = *(cross_validation_results.begin() + 1);

            if(class1 + class2 > max){
                max = class1 + class2;
                rvm_gamma = gamma;
            }
        }

        cout << "Best RVM gamma: " << rvm_gamma << endl;
        rvm_cross_validate = false;
    }

    trainer.set_kernel(kernel_type(rvm_gamma));

    // Here we are making an instance of the normalized_function object.  This object provides a convenient
    // way to store the vector normalization information along with the decision function we are
    // going to learn.
    rvm_learned_function.normalizer = normalizer;  // save normalization information
    rvm_learned_function.function = trainer.train(samples, labels); // perform the actual RVM training and save the results
}

double LFOCache::run_svm(std::vector<double> feature) {
    sample_type sample;

    for(auto j=0; j<feature.size(); j++){
        sample(j) = feature.at(j);
    }

    return svm_learned_function(sample);
}

/**
 * Reference: http://dlib.net/svm_c_ex.cpp.html
 * @param features
 * @param labels
 */
void LFOCache::train_svm(std::vector<std::vector<double>> features, std::vector<double> labels) {
    std::vector<sample_type> samples;

    for(auto i = 0; i < features.size(); i++){
        sample_type samp;
        samp.set_size(features.size());

        auto feature = features.at(i);

        for(auto j=0; j<feature.size(); j++){
            samp(j) = feature.at(j);
        }

        samples.push_back(samp);
    }

    dlib::vector_normalizer<sample_type> normalizer;
    // let the normalizer learn the mean and standard deviation of the samples
    normalizer.train(samples);
    // now normalize each sample
    for (unsigned long i = 0; i < samples.size(); ++i)
        samples[i] = normalizer(samples[i]);

    randomize_samples(samples, labels);

    // here we make an instance of the svm_c_trainer object that uses our kernel
    // type.
    dlib::svm_c_trainer<kernel_type> trainer;

    if(svm_cross_validate){
        double max = 0;
        for (double gamma = 0.00001; gamma <= 1; gamma *= 5)
        {
            for (double C = 1; C < 100000; C *= 5)
            {
                // tell the trainer the parameters we want to use
                trainer.set_kernel(kernel_type(gamma));
                trainer.set_c(C);

                cout << "gamma: " << gamma << "    C: " << C;
                // Print out the cross validation accuracy for 3-fold cross validation using
                // the current gamma and C.  cross_validate_trainer() returns a row vector.
                // The first element of the vector is the fraction of +1 training examples
                // correctly classified and the second number is the fraction of -1 training
                // examples correctly classified.
                auto cross_validation_results = cross_validate_trainer(trainer, samples, labels, 3);

                double class1 = *(cross_validation_results.begin());
                double class2 = *(cross_validation_results.begin() + 1);

                if(class1 + class2 > max){
                    max = class1 + class2;
                    svm_gamma = gamma;
                    svm_c = C;
                }
            }
        }

        cout << "Best SVM gamma: " << svm_gamma << " C: " <<  svm_c << endl;
        svm_cross_validate = false;
    }

    trainer.set_kernel(kernel_type(svm_gamma));
    trainer.set_c(svm_c);
    typedef dlib::decision_function<kernel_type> dec_funct_type;
    typedef dlib::normalized_function<dec_funct_type> funct_type;

    // Here we are making an instance of the normalized_function object.  This
    // object provides a convenient way to store the vector normalization
    // information along with the decision function we are going to learn.
    svm_learned_function.normalizer = normalizer;  // save normalization information
    svm_learned_function.function = trainer.train(samples, labels); // perform the actual SVM training and save the results

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
