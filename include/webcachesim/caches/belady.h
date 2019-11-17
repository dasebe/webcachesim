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
using namespace webcachesim;

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
    unordered_map<uint64_t, uint64_t> last_req_timestamps;
    // how far an evicted object will access again
    vector<uint8_t> eviction_distances;
    vector<uint8_t> hit_distances;
    uint64_t byte_million_req;
    unsigned int current_t;
    string task_id;
    string dburl;
#endif

public:

#ifdef EVICTION_LOGGING

    void init_with_params(const map<string, string> &params) override {
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

    bool lookup(SimpleRequest &req) override;

    void admit(SimpleRequest &req) override;

    void evict();

    bool has(const uint64_t &id) override { return _cacheMap.find(id) != _cacheMap.end(); }

#ifdef EVICTION_LOGGING
    void update_stat(bsoncxx::v_noabi::builder::basic::document &doc) override {
        //Log to GridFs because the value is too big to store in mongodb
        try {
            mongocxx::client client = mongocxx::client{mongocxx::uri(dburl)};
            mongocxx::database db = client["webcachesim"];
            auto bucket = db.gridfs_bucket();

            auto uploader = bucket.open_upload_stream(task_id + ".evictions");
            for (auto &b: eviction_distances)
                uploader.write((uint8_t *) (&b), sizeof(uint8_t));
            uploader.close();
            uploader = bucket.open_upload_stream(task_id + ".hits");
            for (auto &b: hit_distances)
                uploader.write((uint8_t *) (&b), sizeof(uint8_t));
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
