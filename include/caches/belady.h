//
// Created by zhenyus on 11/8/18.
//

#ifndef WEBCACHESIM_BELADY_H
#define WEBCACHESIM_BELADY_H

#include "cache.h"
#include <utils.h>
#include <unordered_map>
#include <map>
#include <fstream>
#include <algorithm>

using namespace std;

/*
  Belady: Optimal for unit size
*/
class BeladyCache : public Cache
{
protected:
    // list for recency order
    multimap<uint64_t , uint64_t, greater<uint64_t >> _valueMap;
    // only store in-cache object, value is size
    unordered_map<uint64_t, uint64_t> _cacheMap;
    // how far an evicted object will access again
    vector<uint8_t> eviction_qualities;
    vector<uint16_t> eviction_logic_timestamps;
    uint64_t byte_million_req;
    unsigned int current_t;
    string task_id;


public:
    BeladyCache()
            : Cache()
    {
    }

    void init_with_params(map<string, string> params) override {
        for (auto &it: params) {
            if (it.first == "byte_million_req") {
                byte_million_req = stoull(it.second);
            } else if (it.first == "task_id") {
                task_id = it.second;
            } else {
                cerr << "unrecognized parameter: " << it.first << endl;
            }
        }
    }

    virtual bool lookup(SimpleRequest& req);
    virtual void admit(SimpleRequest& req);
    virtual void evict(SimpleRequest& req) {
        //no need to use it
    };
    virtual void evict();
    bool has(const uint64_t& id) {return _cacheMap.find(id) != _cacheMap.end();}

    void update_stat(std::map<std::string, std::string> &res) override {
        //log eviction qualities. The value is too big to store in mongodb
        string webcachesim_trace_dir = getenv("WEBCACHESIM_TRACE_DIR");
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
};

static Factory<BeladyCache> factoryBelady("Belady");


#endif //WEBCACHESIM_BELADY_H
