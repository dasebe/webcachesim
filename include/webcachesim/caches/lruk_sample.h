//
// Created by zhenyus on 12/17/18.
//

#ifndef WEBCACHESIM_LRUK_SAMPLE_H
#define WEBCACHESIM_LRUK_SAMPLE_H


#include <cache.h>
#include <unordered_map>
#include "lr.h"

using namespace std;

class LRUKSampleCache : public Cache {
public:
    unordered_map<uint64_t, pair<bool, uint32_t>> key_map;
    vector<LRMeta> meta_holder[2];

    uint sample_rate = 32;
    // n_past_interval
    uint8_t n_past_intervals = 4;
    bool forget_on_evict = true;

    default_random_engine _generator = default_random_engine();
    uniform_int_distribution<std::size_t> _distribution = uniform_int_distribution<std::size_t>();

    LRUKSampleCache()
            : Cache() {
    }

    void init_with_params(const map<string, string> &params) override {
        //set params
        for (auto &it: params) {
            if (it.first == "sample_rate") {
                sample_rate = stoul(it.second);
            } else if (it.first == "n_past_intervals") {
                n_past_intervals = (uint8_t) stoi(it.second);
            } else if (it.first == "forget_on_evict") {
                forget_on_evict = (bool) stoi(it.second);
            } else {
                cerr << "unrecognized parameter: " << it.first << endl;
            }
        }

        //init
        LRMeta::_n_past_intervals = n_past_intervals;
    }

    virtual bool lookup(SimpleRequest &req);

    virtual void admit(SimpleRequest &req);

    void evict(const uint64_t &t);

    //sample, rank the 1st and return
    pair<uint64_t, uint32_t> rank(const uint64_t &t);
};

static Factory<LRUKSampleCache> factoryLRUKSample("LRUKSample");

#endif //WEBCACHESIM_LRUK_SAMPLE_H
