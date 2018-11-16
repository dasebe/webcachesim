//
// Created by zhenyus on 11/8/18.
//

#ifndef WEBCACHESIM_RANDOM_VARIANTS_H
#define WEBCACHESIM_RANDOM_VARIANTS_H

#include "cache.h"
#include "cache_object.h"
#include "pickset.h"
#include <unordered_map>
#include <list>
#include <cmath>
#include <boost/bimap.hpp>
#include <boost/bimap/list_of.hpp>



class RandomCache : public Cache
{
public:
    PickSet<std::pair<uint64_t , uint64_t> > key_space;

    RandomCache()
        : Cache()
    {
    }
    virtual ~RandomCache()
    {
    }

    virtual bool lookup(SimpleRequest& req);
    virtual void admit(SimpleRequest& req);
    virtual void evict();
    virtual void evict(SimpleRequest& req) {
        //no need to use it
    };
};

static Factory<RandomCache> factoryRandom("Random");


class LRCache : public RandomCache
{
public:
    // from id to intervals
    std::unordered_map<std::pair<uint64_t, uint64_t >, std::list<uint64_t> > past_timestamps;
    boost::bimap<boost::bimaps::set_of<KeyT>, boost::bimaps::list_of<uint64_t>> future_timestamp;
    // sample_size
    uint64_t sample_rate=32;
    // threshold
    uint64_t threshold=1000000;
    double log1p_threshold=log1p(threshold);
    // batch_size
    uint64_t batch_size=100;
    // learning_rate 
    double learning_rate=0.001;
    // n_past_interval
    uint64_t n_past_intervals = 4;

    double * weights;
    double bias;
    double mean_diff=0;
    std::map<uint64_t, std::list<double>> pending_gradients;


    LRCache()
        : RandomCache()
    {
    }
    virtual ~LRCache()
    {
        delete []weights;
    }

    virtual void setPar(std::string parName, std::string parValue) {
        if (parName == "sample_rate")
            sample_rate = stoull(parValue);
        else if(parName == "threshold") {
            threshold = stoull(parValue);
            log1p_threshold = std::log1p(threshold);
        }
        else if(parName == "batch_size")
            batch_size = stoull(parValue);
        else if(parName == "learning_rate")
            learning_rate = stod(parValue);
        else if(parName == "n_past_intervals") {
            n_past_intervals = stoull(parValue);
            weights = new double[n_past_intervals];
            for (int i = 0; i < n_past_intervals; i++) {
                weights[i] = 0;
            }
        }
        else {
       std::cerr << "unrecognized parameter: " << parName << std::endl;
   }
}
    virtual bool lookup(SimpleRequest& req);
    virtual void admit(SimpleRequest& req);
    virtual void evict(uint64_t t);
    void try_train(uint64_t t);
    void try_gc(uint64_t t);
};

static Factory<LRCache> factoryLR("LR");


class BeladySampleCache : public RandomCache
{
public:
    std::unordered_map<std::pair<uint64_t, uint64_t >, uint64_t > future_timestamp;
    // sample_size
    uint64_t sample_rate=32;
    // threshold
    uint64_t threshold=1000000;
    double log1p_threshold=log1p(threshold);

    BeladySampleCache()
        : RandomCache()
    {
    }

    virtual void setPar(std::string parName, std::string parValue) {
        if (parName == "sample_rate")
            sample_rate = stoull(parValue);
        else if(parName == "threshold") {
            threshold = stoull(parValue);
            log1p_threshold = std::log1p(threshold);
        }
        else {
       std::cerr << "unrecognized parameter: " << parName << std::endl;
   }
}
    virtual bool lookup(SimpleRequest& req);
    virtual void admit(SimpleRequest& req);
    virtual void evict(uint64_t t);
};

static Factory<BeladySampleCache> factoryBeladySample("BeladySample");

#endif //WEBCACHESIM_RANDOM_VARIANTS_H
