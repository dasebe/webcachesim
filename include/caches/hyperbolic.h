//
// Created by zhenyus on 12/17/18.
//

#ifndef WEBCACHESIM_HYPERBOLIC_H
#define WEBCACHESIM_HYPERBOLIC_H

#include <cache.h>
#include <unordered_map>
#include <random>

using namespace std;

class HyperbolicMeta {
public:
    uint64_t _key;
    uint64_t _size;
    uint64_t _insertion_time;
    uint64_t _n_requests;

    HyperbolicMeta(const uint64_t & key, const uint64_t & size, const uint64_t & t) {
        _key = key;
        _size = size;
        _insertion_time = t;
        _n_requests = 1;
    }

    inline void update() {
        ++_n_requests;
    }
};


class HyperbolicCache : public Cache
{
public:
    unordered_map<uint64_t, uint32_t> key_map;
    vector<HyperbolicMeta> meta_holder;

    uint64_t sample_rate = 64;

    //todo: seed and generator
    default_random_engine _generator = default_random_engine();
    uniform_int_distribution<std::size_t> _distribution = uniform_int_distribution<std::size_t>();

    void init_with_params(map<string, string> params) override {
        //set params
        for (auto& it: params) {
            if (it.first == "sample_rate") {
                sample_rate = stoul(it.second);
            } else {
                cerr << "unrecognized parameter: " << it.first << endl;
            }
        }
    }

    virtual bool lookup(SimpleRequest& req);
    virtual void admit(SimpleRequest& req);
    virtual void evict(const uint64_t & t);
    void evict(SimpleRequest & req) {};
    void evict() {};
    //sample, rank the 1st and return
    pair<uint64_t, uint32_t > rank(const uint64_t & t);
};

static Factory<HyperbolicCache> factoryHyperbolic("Hyperbolic");

#endif //WEBCACHESIM_HYPERBOLIC_H
