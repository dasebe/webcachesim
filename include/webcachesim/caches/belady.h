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

using bsoncxx::builder::basic::kvp;
using bsoncxx::builder::basic::sub_array;
#endif

using namespace std;
using namespace webcachesim;

/*
  Belady: Optimal for unit size
*/
class BeladyCache : public Cache {
protected:
    // next_request -> id
    multimap<uint64_t, uint64_t, greater<uint64_t >> _next_req_map;
    // only store in-cache object, value is size
    unordered_map<uint64_t, uint64_t> _size_map;
#ifdef EVICTION_LOGGING
    unordered_map<uint64_t, uint64_t> last_req_timestamps;
    // how far an evicted object will access again
    vector<uint8_t> eviction_distances;
    vector<uint8_t> hit_distances;
    uint64_t byte_million_req;
    unsigned int current_t;
    string task_id;
    string dburl;
    vector<double> beyond_byte_ratio;
    vector<double> beyond_obj_ratio;
    uint64_t boundary;
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
            } else if (it.first == "boundary") {
                boundary = stoull(it.second);
            } else {
                cerr << "unrecognized parameter: " << it.first << endl;
            }
        }
    }
#endif

    bool lookup(SimpleRequest &req) override;

    void admit(SimpleRequest &req) override;

    void evict();

    bool has(const uint64_t &id) override { return _size_map.find(id) != _size_map.end(); }

#ifdef EVICTION_LOGGING


    void update_stat_periodic() override {
        size_t within_byte = 0, beyond_byte = 0;
        size_t within_obj = 0, beyond_obj = 0;
        for (auto &it_n: _next_req_map) {
            auto it_s = _size_map.find(it_n.second);
            if (_size_map.end() != it_s) {
                //in-cache objs
                if (it_n.first - current_t >= boundary) {
                    beyond_byte += it_s->second;
                    ++beyond_obj;
                } else {
                    within_byte += it_s->second;
                    ++within_obj;
                }
            }
        }
        beyond_byte_ratio.emplace_back(static_cast<double>(beyond_byte) / (beyond_byte + within_byte));
        beyond_obj_ratio.emplace_back(static_cast<double>(beyond_obj) / (beyond_obj + within_obj));
    }

    void update_stat(bsoncxx::v_noabi::builder::basic::document &doc) override {
        doc.append(kvp("beyond_byte_ratio", [this](sub_array child) {
            for (const auto &element : beyond_byte_ratio)
                child.append(element);
        }));
        doc.append(kvp("beyond_obj_ratio", [this](sub_array child) {
            for (const auto &element : beyond_obj_ratio)
                child.append(element);
        }));
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
