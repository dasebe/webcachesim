//
// Created by zhenyus on 12/17/18.
//

#ifndef WEBCACHESIM_BINARY_RELAXED_BELADY_H
#define WEBCACHESIM_BINARY_RELAXED_BELADY_H

#include <cache.h>
#include <unordered_map>
#include <cmath>
#include <random>
#include "mongocxx/client.hpp"
#include "mongocxx/uri.hpp"
#include <bsoncxx/builder/basic/document.hpp>
#include <assert.h>
#include "bsoncxx/json.hpp"

using namespace std;
using bsoncxx::builder::basic::kvp;
using bsoncxx::builder::basic::sub_array;
using namespace webcachesim;
typedef uint64_t KeyT;
typedef uint64_t SizeT;

class BinaryRelaxedBeladyMeta {
public:
    KeyT _key;
    SizeT _size;
    uint64_t _past_timestamp;
    uint64_t _future_timestamp;

    BinaryRelaxedBeladyMeta(const uint64_t &key, const uint64_t &size, const uint64_t &past_timestamp,
                            const uint64_t &future_timestamp) {
        _key = key;
        _size = size;
        _past_timestamp = past_timestamp;
        _future_timestamp = future_timestamp;
    }

    explicit BinaryRelaxedBeladyMeta(const AnnotatedRequest &req) {
        _key = req._id;
        _size = req._size;
        _past_timestamp = req._t;
        _future_timestamp = req._next_seq;
    }

    inline void update(const uint64_t &past_timestamp, const uint64_t &future_timestamp) {
        _past_timestamp = past_timestamp;
        _future_timestamp = future_timestamp;
    }

    inline void update(const AnnotatedRequest &req) {
        _past_timestamp = req._t;
        _future_timestamp = req._next_seq;
    }
};


class BinaryRelaxedBeladyCache : public Cache {
public:
    //key -> (0/1 list, idx)
    enum MetaT : uint8_t {
        within_boundary = 0, beyond_boundary = 1
    };
    unordered_map<KeyT, pair<MetaT, uint32_t >> key_map;
    vector<BinaryRelaxedBeladyMeta> within_boundary_meta;
    vector<BinaryRelaxedBeladyMeta> beyond_boundary_meta;

    uint64_t belady_boundary = 10000000;

    default_random_engine _generator = default_random_engine();
    uniform_int_distribution<std::size_t> _distribution = uniform_int_distribution<std::size_t>();
    uint64_t current_t;

#ifdef EVICTION_LOGGING
    // how far an evicted object will access again
    vector<uint8_t> eviction_distances;
    uint64_t byte_million_req;
    string task_id;
    string dburl;
    vector<double> beyond_byte_ratio;
    vector<double> beyond_obj_ratio;
#endif

    void init_with_params(const map<string, string> &params) override {
        //set params
        for (auto &it: params) {
            if (it.first == "belady_boundary") {
                belady_boundary = stoull(it.second);
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
    }

#ifdef EVICTION_LOGGING

    void update_stat_periodic() override {
        int64_t within_byte = 0, beyond_byte = 0;
        int64_t within_obj = 0, beyond_obj = 0;
        for (auto &i: meta_holder[0]) {
            if (i._future_timestamp - current_t > threshold) {
                beyond_byte += i._size;
                ++beyond_obj;
            } else {
                within_byte += i._size;
                ++within_obj;
            }
        }
        beyond_byte_ratio.emplace_back(static_cast<double>(beyond_byte) / (beyond_byte + within_byte));
        beyond_obj_ratio.emplace_back(static_cast<double>(beyond_obj) / (beyond_obj + within_obj));
    }

    void update_stat(bsoncxx::builder::basic::document &doc) override {
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
        } catch (const std::exception &xcp) {
            cerr << "error: db connection failed: " << xcp.what() << std::endl;
            abort();
        }
    }

#endif

    void meta_remove_and_append(
            vector<BinaryRelaxedBeladyMeta> &rm_list,
            const uint32_t &pos,
            vector<BinaryRelaxedBeladyMeta> &app_list
    ) {
        auto &meta = rm_list[pos];
        auto it = key_map.find(meta._key);
        it->second.first = static_cast<MetaT>(!(it->second.first));
        it->second.second = app_list.size();
        app_list.emplace_back(meta);

        auto old_tail_idx = rm_list.size() - 1;
        if (pos != old_tail_idx) {
            //move tail
            assert(key_map.find(rm_list.back()._key) != key_map.end());
            meta = rm_list.back();
            key_map.find(rm_list.back()._key)->second.second = pos;
        }
        rm_list.pop_back();
    }

    void meta_remove(vector<BinaryRelaxedBeladyMeta> &list, const uint32_t &pos) {
        auto &meta = list[pos];
        _currentSize -= meta._size;
        key_map.erase(meta._key);
        auto old_tail_idx = list.size() - 1;
        if (pos != old_tail_idx) {
            //move tail
            assert(key_map.find(list.back()._key) != key_map.end());
            meta = list.back();
            key_map.find(list.back()._key)->second.second = pos;
        }
        list.pop_back();
    }

    pair<MetaT, uint32_t> rank();

    bool lookup(SimpleRequest &req) override;

    void admit(SimpleRequest &req) override;

    void evict();
};

static Factory<BinaryRelaxedBeladyCache> factoryBinaryRelaxedBelady("BinaryRelaxedBelady");


#endif //WEBCACHESIM_BINARY_RELAXED_BELADY_H
