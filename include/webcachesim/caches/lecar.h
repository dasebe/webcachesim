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

#ifdef EVICTION_LOGGING
#include "mongocxx/client.hpp"
#endif

using namespace std;
using namespace boost;
using namespace webcachesim;

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

#ifdef EVICTION_LOGGING
    uint32_t current_t;
    unordered_map<uint64_t, uint32_t> future_timestamps;
    vector<uint8_t> eviction_qualities;
    vector<uint16_t> eviction_logic_timestamps;
    uint64_t byte_million_req;
    string task_id;
    string dburl;
#endif

    void init_with_params(map<string, string> params) override {
        //set params
        for (auto& it: params) {
            if (it.first == "learning_rate") {
                learning_rate = stod(it.second);
#ifdef EVICTION_LOGGING
            } else if (it.first == "byte_million_req") {
                byte_million_req = stoull(it.second);
            } else if (it.first == "task_id") {
                task_id = it.second;
            } else if (it.first == "dburl") {
                dburl = it.second;
#endif
            } else {
                cerr << "unrecognized parameter: " << it.first << endl;
            }
        }
        discount_rate = pow(0.005, 1./_cacheSize);
        w[0] = w[1] = 0.5;
    }


#ifdef EVICTION_LOGGING
    void update_stat(bsoncxx::v_noabi::builder::basic::document &doc) override {
        //Log to GridFs because the value is too big to store in mongodb
        try {
            mongocxx::client client = mongocxx::client{mongocxx::uri(dburl)};
            mongocxx::database db = client["webcachesim"];
            auto bucket = db.gridfs_bucket();

            auto uploader = bucket.open_upload_stream(task_id + ".evictions");
            for (auto &b: eviction_qualities)
                uploader.write((uint8_t *) (&b), sizeof(uint8_t));
            uploader.close();
            uploader = bucket.open_upload_stream(task_id + ".eviction_timestamps");
            for (auto &b: eviction_logic_timestamps)
                uploader.write((uint8_t *) (&b), sizeof(uint16_t));
            uploader.close();
        } catch (const std::exception &xcp) {
            cerr << "error: db connection failed: " << xcp.what() << std::endl;
            abort();
        }
    }
#endif

    bool lookup(SimpleRequest &req) override;

    void admit(SimpleRequest &req) override;
    void evict(uint64_t & t, uint64_t & counter);
    bool has(const uint64_t& id) {return size_map.find(id) != size_map.end();}
};

static Factory<LeCaRCache> factoryLeCaR("LeCaR");

#endif //WEBCACHESIM_LECAR_H
