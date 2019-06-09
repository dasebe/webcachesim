//
// Created by Zhenyu Song on 10/30/18.
//

#include "simulation.h"
#include <fstream>
#include <string>
#include <regex>
#include "request.h"
#include "simulation_future.h"
#include "simulation_tinylfu.h"
#include "cache.h"
//#include "simulation_lr_belady.h"
//#include "simulation_belady_static.h"
//#include "simulation_bins.h"
#include "simulation_truncate.h"
#include <chrono>
#include "utils.h"
#include <unordered_map>
#include "simulation_lfo.h"

#include "miss_decouple.h"
#include "cache_size_decouple.h"
#include "nlohmann/json.hpp"


#include <proc/readproc.h>
#include <cstdint>

using namespace std;
using namespace chrono;
using json = nlohmann::json;

map<string, string> _simulation(string trace_file, string cache_type, uint64_t cache_size,
                                map<string, string> params){
    // create cache
    unique_ptr<Cache> webcache = move(Cache::create_unique(cache_type));
    if(webcache == nullptr) {
        cerr<<"cache type not implemented"<<endl;
        exit(-2);
    }

    // configure cache size
    webcache->setSize(cache_size);

    bool uni_size = false;
    uint64_t segment_window = 10000000;
    uint n_extra_fields = 0;
    bool is_metadata_in_cache_size = true;

    for (auto it = params.cbegin(); it != params.cend();) {
        if (it->first == "uni_size") {
            uni_size = static_cast<bool>(stoi(it->second));
            it = params.erase(it);
        } else if (it->first == "is_metadata_in_cache_size") {
            is_metadata_in_cache_size = static_cast<bool>(stoi(it->second));
            it = params.erase(it);
        } else if (it->first == "segment_window") {
            segment_window = stoull((it->second));
            ++it;
        } else if (it->first == "n_extra_fields") {
            n_extra_fields = stoull(it->second);
            ++it;
        } else {
            ++it;
        }
    }

    webcache->init_with_params(params);

    ifstream infile;
    infile.open(trace_file);
    if (!infile) {
        cerr << "Exception opening/reading file"<<endl;
        exit(-1);
    }
    //get whether file is in a correct format
    {
        std::string line;
        getline(infile, line);
        istringstream iss(line);
        int64_t tmp;
        int counter = 0;
        while (iss>>tmp) {++counter;}
        //format: t id size [extra]
        if (counter != 3+n_extra_fields) {
            cerr<<"error: input file column should be 3+n_extra_fields"<<endl;
            abort();
        }
        infile.clear();
        infile.seekg(0, ios::beg);
    }
    uint64_t tmp, id, size;
    //measure every segment
    uint64_t byte_req = 0, byte_miss = 0, obj_req = 0, obj_miss = 0;
    //global statistics
    vector<uint64_t> seg_byte_req, seg_byte_miss, seg_object_req, seg_object_miss;
    vector<uint64_t> seg_rss;

    vector<uint16_t > extra_features(n_extra_fields, 0);

    SimpleRequest req(0, 0, 0);
    //don't use real timestamp, use relative seq starting from 0
    uint64_t seq = 0;
    auto t_now = system_clock::now();

#ifdef MISS_DECOUPLE
    unordered_map<uint64_t , uint32_t > total_request_map;
    MissStatistics miss_stat;
#endif

#ifdef CACHE_SIZE_DECOUPLE
    unordered_map<uint64_t, uint64_t> size_map;
    CacheSizeStatistics size_stat;
#endif

    while (infile >> tmp >> id >> size) {
        for (int i = 0; i < n_extra_fields; ++i)
            infile>>extra_features[i];
        if (uni_size)
            size = 1;

        DPRINTF("seq: %lu\n", seq);

        update_metric_req(byte_req, obj_req, size)

#ifdef MISS_DECOUPLE
        //count total request
        auto it = total_request_map.find(id);
        if (it == total_request_map.end())
            total_request_map.insert({id, 1});
        else
            ++it->second;
#endif
#ifdef CACHE_SIZE_DECOUPLE
        auto it_size = size_map.find(id);
        if (it_size == size_map.end())
            size_map.insert({id, size});
#endif
        req.reinit(id, size, seq, &extra_features);
        bool is_hit = webcache->lookup(req);
        if (!is_hit) {
            update_metric_req(byte_miss, obj_miss, size)
            webcache->admit(req);
        }

#ifdef MISS_DECOUPLE
        if (seq >= n_warmup) {
            auto &n_total_request = total_request_map[id];
            miss_stat.update(n_total_request, size, is_hit);
        }
#endif

        ++seq;

        if (!(seq%segment_window)) {
#ifdef CACHE_SIZE_DECOUPLE
            if (seq >= n_warmup) {
                uint32_t snapshot_id = seq/segment_window;
                cerr<<"snapshoting cache size decoupling at id: "<<snapshot_id<<endl;
                for (auto &kv: total_request_map) {
                    if (webcache->has(kv.first)) {
                        size_stat.update(snapshot_id, kv.second, size_map[kv.first]);
                    }
                }
            }
#endif
            auto _t_now = chrono::system_clock::now();
            cerr<<"\nsegment id: " << seq/segment_window-1 << endl;
            cerr<<"delta t: "<<chrono::duration_cast<std::chrono::milliseconds>(_t_now - t_now).count()/1000.<<endl;
            t_now = _t_now;
            cerr<<"segment bmr: " << double(byte_miss) / byte_req << endl;
            seg_byte_miss.emplace_back(byte_miss); seg_byte_req.emplace_back(byte_req);
            seg_object_miss.emplace_back(obj_miss); seg_object_req.emplace_back(obj_req);
            byte_miss=obj_miss=byte_req=obj_req=0;
            //reduce cache size by metadata
            struct proc_t usage;
            look_up_our_self(&usage);
            uint64_t metadata_overhead = usage.rss*getpagesize();
            seg_rss.emplace_back(metadata_overhead);
            if (is_metadata_in_cache_size)
                webcache->setSize(cache_size-metadata_overhead);
            cerr<<"rss: "<<metadata_overhead<<endl;
        }
    }

    infile.close();

    map<string, string> res = {
            {"segment_byte_miss", json(seg_byte_miss).dump()},
            {"segment_byte_req", json(seg_byte_req).dump()},
            {"segment_object_miss", json(seg_object_miss).dump()},
            {"segment_object_req", json(seg_object_req).dump()},
            {"segment_rss", json(seg_rss).dump()},
#ifdef MISS_DECOUPLE
            {"miss_decouple", miss_stat.dump()},
#endif
#ifdef CACHE_SIZE_DECOUPLE
            {"cache_size_decouple", size_stat.dump()},
#endif
    };

    webcache->update_stat(res);
    return res;
}

map<string, string> simulation(string trace_file, string cache_type,
        uint64_t cache_size, map<string, string> params){
    if (cache_type == "Belady" || cache_type == "BeladySample" || cache_type == "LRUKSample" ||
        cache_type == "LFUSample")
        return _simulation_future(trace_file, cache_type, cache_size, params);
    else if (cache_type == "Adaptive-TinyLFU")
        return _simulation_tinylfu(trace_file, cache_type, cache_size, params);
    else if (cache_type == "LFO")
        return LFO::_simulation_lfo(trace_file, cache_type, cache_size, params);
    else if (cache_type == "BeladyTruncate")
       return _simulation_truncate(trace_file, cache_type, cache_size, params);
    else
        return _simulation(trace_file, cache_type, cache_size, params);
}
