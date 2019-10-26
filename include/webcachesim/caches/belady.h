//
// Created by zhenyus on 11/8/18.
//

#ifndef WEBCACHESIM_BELADY_H
#define WEBCACHESIM_BELADY_H

#include "cache.h"
#include <utils.h>
#include <unordered_map>
#include <map>

#ifdef EVICTION_LOGGING
#include "mongocxx/client.hpp"
#endif

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
#ifdef EVICTION_LOGGING
    // how far an evicted object will access again
    vector<uint8_t> eviction_qualities;
    vector<uint16_t> eviction_logic_timestamps;
    uint64_t byte_million_req;
    unsigned int current_t;
    string task_id;
    string dburl;
#endif

public:

#ifdef EVICTION_LOGGING
    void init_with_params(map<string, string> params) override {
        for (auto &it: params) {
            if (it.first == "byte_million_req") {
                byte_million_req = stoull(it.second);
            } else if (it.first == "task_id") {
                task_id = it.second;
            } else if (it.first == "dburl") {
                dburl = it.second;
            } else {
                cerr << "unrecognized parameter: " << it.first << endl;
            }
        }
    }
#endif

    virtual bool lookup(SimpleRequest& req);
    virtual void admit(SimpleRequest& req);
    virtual void evict(SimpleRequest& req) {
        //no need to use it
    };
    virtual void evict();
    bool has(const uint64_t& id) {return _cacheMap.find(id) != _cacheMap.end();}

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
};

static Factory<BeladyCache> factoryBelady("Belady");


#endif //WEBCACHESIM_BELADY_H
