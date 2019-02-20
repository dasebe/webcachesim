//
// Created by zhenyus on 11/8/18.
//

#include <chrono>
#include "simulation_belady_static.h"
#include "cache.h"
#include <fstream>
#include "utils.h"
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <assert.h>
#include <unordered_set>


using namespace std;
using namespace chrono;



map<string, string> _simulation_belady_static(string trace_file, string cache_type, uint64_t cache_size,
                                    map<string, string> params){
    /*
     *  todo: only uni size
     *  assumption: n_warmup % static_window == 0. Static_window < n_warmup
     *
     */
    uint64_t n_warmup = 0;
    bool uni_size = false;
    uint64_t segment_window = 1000000;
    uint64_t static_window = 100000;
    bool zero_cost = true;
    for (auto& kv: params) {
        if (kv.first == "n_warmup")
            n_warmup = stoull(kv.second);
        if (kv.first == "uni_size")
            uni_size = static_cast<bool>(stoi(kv.second));
        if (kv.first == "segment_window")
            segment_window = stoull(kv.second);
        if (kv.first == "static_window")
            static_window = stoull(kv.second);
        if (kv.first == "zero_cost")
            zero_cost = static_cast<bool>(stoul(kv.second));
    }

    ifstream infile;
    infile.open(trace_file);
    if (!infile) {
        cerr << "Exception opening/reading file"<<endl;
        return {};
    }

    uint64_t byte_req = 0, byte_hit = 0, obj_req = 0, obj_hit = 0;
    uint64_t t, id, size;

    unordered_set<uint64_t> static_set;
    unordered_map<uint64_t, uint32_t > request_map;

    uint64_t seq = 0;
    auto t_now = system_clock::now();
    while (infile >> t >> id >> size) {
        if (uni_size)
            size = 1;

        DPRINTF("seq: %lu\n", seq);

        if (seq >= n_warmup)
            update_metric_req(byte_req, obj_req, size);

        auto it = request_map.find(id);
        if (it == request_map.end()) {
            request_map.insert({id, 1});
        } else {
            it -> second += 1;
        }

        if (seq+1 == static_window) {
            if (request_map.size() < cache_size) {
                cerr<<"the static window size is too small"<<endl;
                return {};
            }
            //first window
            vector<pair<uint32_t, uint64_t>> count;
            for (auto & it: request_map) {
                count.emplace_back(it.second, it.first);
            }
            sort(count.begin(), count.end(), greater<pair<uint32_t, uint64_t>>());
            for (auto it = count.cbegin(); it != count.cbegin() + cache_size; ++it)
                static_set.insert(it->second);
            request_map.clear();
        }
        else if (!((seq+1) % static_window)) {
            if (request_map.size() < cache_size) {
                cerr<<"the static window size is too small"<<endl;
                return {};
            }
            vector<pair<uint32_t, uint64_t>> count;
            for (auto & it: request_map) {
                count.emplace_back(it.second, it.first);
            }
            sort(count.begin(), count.end(), greater<pair<uint32_t, uint64_t>>());
            unordered_set<uint64_t > new_static_set;
            for (auto it = count.cbegin(); it != count.cbegin() + cache_size; ++it) {
                if (seq > n_warmup) {
                    if (static_set.find(it->second) == static_set.end()) {
                        if (zero_cost) {
                            obj_hit += it->first;
                            byte_hit += it->first * size;
                        } else {
                            obj_hit += it->first - 1;
                            byte_hit += (it->first - 1) * size;
                        }
                    } else {
                        obj_hit += it->first;
                        byte_hit += it->first * size;
                    }
                }
                new_static_set.insert(it->second);
            }
            static_set = new_static_set;
            request_map.clear();
        }

        ++seq;

        if (!(seq%segment_window)) {
            auto _t_now = chrono::system_clock::now();
            cerr<<"delta t: "<<chrono::duration_cast<std::chrono::milliseconds>(_t_now - t_now).count()/1000.<<endl;
            cerr<<"seq: " << seq << endl;
            cerr<<"accu bhr: " << double(byte_hit) / byte_req << endl;
            t_now = _t_now;
        }
    }

    infile.close();

    map<string, string> res = {
            {"byte_hit_rate", to_string(double(byte_hit) / byte_req)},
            {"object_hit_rate", to_string(double(obj_hit) / obj_req)},
    };
    return res;
}
