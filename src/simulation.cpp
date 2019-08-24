//
// Created by Zhenyu Song on 10/30/18.
//

#include "simulation.h"
#include <fstream>
#include <string>
#include <regex>
#include "request.h"
#include "annotate.h"
#include "simulation_tinylfu.h"
#include "cache.h"
//#include "simulation_lr_belady.h"
//#include "simulation_belady_static.h"
//#include "simulation_bins.h"
#include "simulation_truncate.h"
#include <chrono>
#include <sstream>
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
    if (params.find("n_extra_fields") == params.end()) {
        cerr << "error: field n_extra_fields is required" << endl;
        exit(-1);
    }
    annotate(trace_file, stoul(params["n_extra_fields"]));

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
    uint64_t real_time_segment_window = 10000000;
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
        } else if (it->first == "real_time_segment_window") {
            real_time_segment_window = stoull((it->second));
            it = params.erase(it);
        } else if (it->first == "n_extra_fields") {
            n_extra_fields = stoull(it->second);
            ++it;
        } else {
            ++it;
        }
    }

    webcache->init_with_params(params);

    ifstream infile(trace_file + ".ant");
    if (!infile) {
        cerr << "Exception opening/reading file"<<endl;
        exit(-1);
    }
    //get whether file is in a correct format
    {
        std::string line;
        getline(infile, line);
        istringstream iss(line);
        uint64_t tmp;
        int counter = 0;
        while (iss >> tmp) {
            ++counter;
        }
        //todo: check the type of each argument
        //format: n_seq t id size [extra]
        if (counter != 4 + n_extra_fields) {
            cerr << "error: input file column should be 4 + " << n_extra_fields << endl
                 << "first line: " << line << endl;
            abort();
        }
        infile.clear();
        infile.seekg(0, ios::beg);
    }
    uint64_t t, id, size, next_seq;
    //measure every segment
    uint64_t byte_req = 0, byte_miss = 0, obj_req = 0, obj_miss = 0;
    //rt: real_time
    uint64_t rt_byte_req = 0, rt_byte_miss = 0, rt_obj_req = 0, rt_obj_miss = 0;
    //global statistics
    vector<uint64_t> seg_byte_req, seg_byte_miss, seg_object_req, seg_object_miss;
    vector<uint64_t> seg_rss;
    //rt: real_time
    vector<uint64_t> rt_seg_byte_req, rt_seg_byte_miss, rt_seg_object_req, rt_seg_object_miss;
    vector<uint64_t> rt_seg_rss;

    uint64_t time_window_end;
    {
        // Zhenyu: not assume t start from any constant, so need to compute the first window
        infile >> next_seq >> t;
        time_window_end =
                real_time_segment_window * (t / real_time_segment_window + (t % real_time_segment_window != 0));
        infile.clear();
        infile.seekg(0, ios::beg);
    }

    vector<uint16_t > extra_features(n_extra_fields, 0);

    cerr << "simulating" << endl;
    SimpleRequest *req;
    if (cache_type == "Belady") {
        req = new AnnotatedRequest(0, 0, 0, 0);
    } else {
        req = new SimpleRequest(0, 0, 0);
    }
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

    while (infile >> next_seq >> t >> id >> size) {
        for (int i = 0; i < n_extra_fields; ++i)
            infile>>extra_features[i];
        if (uni_size)
            size = 1;

        DPRINTF("seq: %lu\n", seq);

        while (t >= time_window_end) { // in seconds
            rt_seg_byte_miss.emplace_back(rt_byte_miss);
            rt_seg_byte_req.emplace_back(rt_byte_req);
            rt_seg_object_miss.emplace_back(rt_obj_miss);
            rt_seg_object_req.emplace_back(rt_obj_req);
            rt_byte_miss = rt_obj_miss = rt_byte_req = rt_obj_req = 0;
            //real time only read rss info
            struct proc_t usage;
            look_up_our_self(&usage);
            uint64_t metadata_overhead = usage.rss * getpagesize();
            rt_seg_rss.emplace_back(metadata_overhead);
            time_window_end += real_time_segment_window;
        }

        if (seq && !(seq % segment_window)) {
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
            cerr << "\nsegment id: " << seq / segment_window - 1 << endl;
            cerr << "delta t: " << chrono::duration_cast<std::chrono::milliseconds>(_t_now - t_now).count() / 1000.
                 << endl;
            t_now = _t_now;
            cerr << "segment bmr: " << double(byte_miss) / byte_req << endl;
            seg_byte_miss.emplace_back(byte_miss);
            seg_byte_req.emplace_back(byte_req);
            seg_object_miss.emplace_back(obj_miss);
            seg_object_req.emplace_back(obj_req);
            byte_miss = obj_miss = byte_req = obj_req = 0;
            //reduce cache size by metadata
            struct proc_t usage;
            look_up_our_self(&usage);
            uint64_t metadata_overhead = usage.rss * getpagesize();
            seg_rss.emplace_back(metadata_overhead);
            if (is_metadata_in_cache_size)
                webcache->setSize(cache_size - metadata_overhead);
            cerr << "rss: " << metadata_overhead << endl;
        }


        update_metric_req(byte_req, obj_req, size)
        update_metric_req(rt_byte_req, rt_obj_req, size)

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
        if (cache_type == "Belady") {
            dynamic_cast<AnnotatedRequest *>(req)->reinit(id, size, seq, next_seq, &extra_features);
        } else {
            req->reinit(id, size, seq, &extra_features);
        }
        bool is_hit = webcache->lookup(*req);
        if (!is_hit) {
            update_metric_req(byte_miss, obj_miss, size)
            update_metric_req(rt_byte_miss, rt_obj_miss, size)
            webcache->admit(*req);
        }

#ifdef MISS_DECOUPLE
        if (seq >= n_warmup) {
            auto &n_total_request = total_request_map[id];
            miss_stat.update(n_total_request, size, is_hit);
        }
#endif
        ++seq;
    }

    {//partial segment
        auto _t_now = chrono::system_clock::now();
        rt_seg_byte_miss.emplace_back(rt_byte_miss);
        rt_seg_byte_req.emplace_back(rt_byte_req);
        rt_seg_object_miss.emplace_back(rt_obj_miss);
        rt_seg_object_req.emplace_back(rt_obj_req);
        //real time only read rss info
        struct proc_t usage;
        look_up_our_self(&usage);
        uint64_t metadata_overhead = usage.rss * getpagesize();
        rt_seg_rss.emplace_back(metadata_overhead);
    }

    {
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
        seg_byte_miss.emplace_back(byte_miss);
        seg_byte_req.emplace_back(byte_req);
        seg_object_miss.emplace_back(obj_miss);
        seg_object_req.emplace_back(obj_req);
        //reduce cache size by metadata
        struct proc_t usage;
        look_up_our_self(&usage);
        uint64_t metadata_overhead = usage.rss * getpagesize();
        seg_rss.emplace_back(metadata_overhead);
    }

    infile.close();

    map<string, string> res = {
            {"segment_byte_miss", json(seg_byte_miss).dump()},
            {"segment_byte_req", json(seg_byte_req).dump()},
            {"segment_object_miss", json(seg_object_miss).dump()},
            {"segment_object_req", json(seg_object_req).dump()},
            {"segment_rss", json(seg_rss).dump()},
            {"real_time_segment_byte_miss", json(rt_seg_byte_miss).dump()},
            {"real_time_segment_byte_req", json(rt_seg_byte_req).dump()},
            {"real_time_segment_object_miss", json(rt_seg_object_miss).dump()},
            {"real_time_segment_object_req", json(rt_seg_object_req).dump()},
            {"real_time_segment_rss", json(rt_seg_rss).dump()},
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
    if (cache_type == "Adaptive-TinyLFU")
        return _simulation_tinylfu(trace_file, cache_type, cache_size, params);
    else if (cache_type == "LFO")
        return LFO::_simulation_lfo(trace_file, cache_type, cache_size, params);
    else if (cache_type == "BeladyTruncate")
       return _simulation_truncate(trace_file, cache_type, cache_size, params);
    else
        return _simulation(trace_file, cache_type, cache_size, params);
}
