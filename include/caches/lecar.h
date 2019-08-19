//
// Created by zhenyus on 2/17/19.
//

#ifndef WEBCACHESIM_LECAR_H
#define WEBCACHESIM_LECAR_H

#include "cache.h"
#include <utils.h>
#include <unordered_map>
#include <map>
#include <boost/bimap.hpp>
#include <fstream>

using namespace std;
using namespace boost;

class LeCaRCache : public Cache
{
public:
    // recency insert_time: key
    bimap<uint64_t, uint64_t > recency;
    // frequency <frequency, t>: key
    bimap<pair<uint64_t, uint64_t> , uint64_t > frequency;
    // only store in-cache object, value is size
    unordered_map<uint64_t, uint64_t> size_map;

    // eviction_time: key
    bimap<pair<uint64_t, uint64_t>, uint64_t > h_lru;
    bimap<pair<uint64_t, uint64_t>, uint64_t > h_lfu;
    map<uint64_t, uint64_t > h_size_map;
    uint64_t h_lru_current_size = 0;
    uint64_t h_lfu_current_size = 0;

    double learning_rate = 0.45;
    double discount_rate;
    //w0: lru, w1: lfu
    double w[2];

    uint32_t current_t;
    unordered_map<uint64_t, uint32_t> future_timestamps;
    vector<uint8_t> eviction_qualities;
    vector<uint16_t> eviction_logic_timestamps;
    uint64_t byte_million_req;
    string task_id;

    void init_with_params(map<string, string> params) override {
        //set params
        for (auto& it: params) {
            if (it.first == "learning_rate") {
                learning_rate = stod(it.second);
            } else if (it.first == "byte_million_req") {
                byte_million_req = stoull(it.second);
            } else if (it.first == "task_id") {
                task_id = it.second;
            } else {
                cerr << "unrecognized parameter: " << it.first << endl;
            }
        }
        discount_rate = pow(0.005, 1./_cacheSize);
        w[0] = w[1] = 0.5;
    }


    void update_stat(std::map<std::string, std::string> &res) override {
        //log eviction qualities. The value is too big to store in mongodb
        string webcachesim_trace_dir = getenv("WEBCACHESIM_TRACE_DIR");
        ofstream outfile(webcachesim_trace_dir + "/" + task_id + ".evictions");
        {
            ofstream outfile(webcachesim_trace_dir + "/" + task_id + ".evictions");
            if (!outfile) {
                cerr << "Exception opening file" << endl;
                abort();
            }
            for (auto &b: eviction_qualities)
                outfile << b;
            outfile.close();
        }
        {
            ofstream outfile(webcachesim_trace_dir + "/" + task_id + ".eviction_timestamps");
            if (!outfile) {
                cerr << "Exception opening file" << endl;
                abort();
            }
            for (auto &b: eviction_logic_timestamps)
                outfile.write((char *) &b, sizeof(uint16_t));
            outfile.close();

        }
    }

    bool lookup(SimpleRequest& req);
    virtual void admit(SimpleRequest& req);
    virtual void evict(SimpleRequest& req){};
    virtual void evict(){};
    void evict(uint64_t & t, uint64_t & counter);
    bool has(const uint64_t& id) {return size_map.find(id) != size_map.end();}
};

static Factory<LeCaRCache> factoryLeCaR("LeCaR");

#endif //WEBCACHESIM_LECAR_H
