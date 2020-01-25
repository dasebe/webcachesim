//
// Created by zhenyus on 11/16/19.
//

#ifndef WEBCACHESIM_RELAXED_BELADY_H
#define WEBCACHESIM_RELAXED_BELADY_H

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

class RelaxedBeladyMeta {
public:
    KeyT _key;
    SizeT _size;
    uint64_t _past_timestamp;
    uint64_t _future_timestamp;

    RelaxedBeladyMeta(const uint64_t &key, const uint64_t &size, const uint64_t &past_timestamp,
                      const uint64_t &future_timestamp) {
        _key = key;
        _size = size;
        _past_timestamp = past_timestamp;
        _future_timestamp = future_timestamp;
    }

    explicit RelaxedBeladyMeta(const AnnotatedRequest &req) {
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


class RelaxedBeladyCache : public Cache {
public:
    //key -> (0/1 list, idx)
    enum MetaT : uint8_t {
        within_boundary = 0, beyond_boundary = 1
    };
    //key -> <metaT, pos>
    unordered_map<KeyT, pair<MetaT, uint32_t >> key_map;
    // list for recency order
    multimap<uint64_t, pair<KeyT, SizeT>, greater<uint64_t >> within_boundary_meta;
    vector<RelaxedBeladyMeta> beyond_boundary_meta;

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
        size_t within_byte = 0, beyond_byte = 0;
        size_t within_obj = 0, beyond_obj = 0;
        for (auto &it_n: within_boundary_meta) {
            auto &key = it_n.second.first;
            auto &size = it_n.second.second;
            auto it_k = key_map.find(key);
            if (key_map.end() != it_k && within_boundary == it_k->second.first) {
                //in-cache objs
                if (it_n.first - current_t >= belady_boundary) {
                    beyond_byte += size;
                    ++beyond_obj;
                } else {
                    within_byte += size;
                    ++within_obj;
                }
            }
        }

        for (auto &meta: beyond_boundary_meta) {
            if (meta._future_timestamp - current_t >= belady_boundary) {
                beyond_byte += meta._size;
                ++beyond_obj;
            } else {
                within_byte += meta._size;
                ++within_obj;
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
    }

#endif

    void beyond_meta_remove_and_append(const uint32_t &pos) {
        auto &meta = beyond_boundary_meta[pos];
        auto it = key_map.find(meta._key);
        it->second.first = static_cast<MetaT>(!(it->second.first));
        within_boundary_meta.emplace(meta._future_timestamp, pair(meta._key, meta._size));

        auto old_tail_idx = beyond_boundary_meta.size() - 1;
        if (pos != old_tail_idx) {
            //move tail
            assert(key_map.find(beyond_boundary_meta.back()._key) != key_map.end());
            meta = beyond_boundary_meta.back();
            key_map.find(beyond_boundary_meta.back()._key)->second.second = pos;
        }
        beyond_boundary_meta.pop_back();
    }

    void beyond_meta_remove(const uint32_t &pos) {
        auto &meta = beyond_boundary_meta[pos];
        _currentSize -= meta._size;
        key_map.erase(meta._key);
        auto old_tail_idx = beyond_boundary_meta.size() - 1;
        if (pos != old_tail_idx) {
            //move tail
            assert(key_map.find(beyond_boundary_meta.back()._key) != key_map.end());
            meta = beyond_boundary_meta.back();
            key_map.find(beyond_boundary_meta.back()._key)->second.second = pos;
        }
        beyond_boundary_meta.pop_back();
    }

    pair<MetaT, uint32_t> rank();

    bool lookup(SimpleRequest &req) override;

    void admit(SimpleRequest &req) override;

    void evict();
};

static Factory<RelaxedBeladyCache> factoryRelaxedBelady("RelaxedBelady");


#endif //WEBCACHESIM_RELAXED_BELADY_H
