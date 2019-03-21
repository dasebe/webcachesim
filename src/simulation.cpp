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
#include "miss_decouple.h"

using namespace std;
using namespace chrono;


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

    uint64_t n_warmup = 0;
    bool uni_size = false;
    uint64_t segment_window = 1000000;
    uint n_extra_fields = 0;

    for (auto it = params.cbegin(); it != params.cend();) {
        if (it->first == "n_warmup") {
            n_warmup = stoull(it->second);
            it = params.erase(it);
        } else if (it->first == "uni_size") {
            uni_size = static_cast<bool>(stoi(it->second));
            it = params.erase(it);
        } else if (it->first == "segment_window") {
            segment_window = stoull((it->second));
            it = params.erase(it);
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

    uint64_t byte_req = 0, byte_hit = 0, obj_req = 0, obj_hit = 0;
    //don't use real timestamp, use relative seq starting from 1
    uint64_t tmp, id, size;
    uint64_t seg_byte_req = 0, seg_byte_hit = 0, seg_obj_req = 0, seg_obj_hit = 0;
    string seg_bhr;
    string seg_ohr;

    vector<uint64_t > extra_features(n_extra_fields, 0);

    SimpleRequest req(0, 0, 0);
    uint64_t seq = 0;
    auto t_now = system_clock::now();

#ifdef MISS_DECOUPLE
    unordered_map<uint64_t , uint32_t > total_request_map;
    MissStatistics miss_stat;
#endif

    while (infile >> tmp >> id >> size) {
        for (int i = 0; i < n_extra_fields; ++i)
            infile>>extra_features[i];
        //todo: currently real timestamp t is not used. Only relative seq is used
        if (uni_size)
            size = 1;

        DPRINTF("seq: %lu\n", seq);

        if (seq >= n_warmup)
            update_metric_req(byte_req, obj_req, size);
        update_metric_req(seg_byte_req, seg_obj_req, size);

#ifdef MISS_DECOUPLE
        //count total request
        auto it = total_request_map.find(id);
        if (it == total_request_map.end())
            total_request_map.insert({id, 1});
        else
            ++it->second;
#endif

        req.reinit(id, size, seq+1, &extra_features);
        bool if_hit = webcache->lookup(req);
        if (if_hit) {
            if (seq >= n_warmup)
                update_metric_req(byte_hit, obj_hit, size);
            update_metric_req(seg_byte_hit, seg_obj_hit, size);
        } else {
            webcache->admit(req);
        }

#ifdef MISS_DECOUPLE
        if (seq >= n_warmup) {
            auto &n_total_request = total_request_map[id];
            miss_stat.update(n_total_request, if_hit);
        }
#endif

        ++seq;

        if (!(seq%segment_window)) {
            auto _t_now = chrono::system_clock::now();
            cerr<<"delta t: "<<chrono::duration_cast<std::chrono::milliseconds>(_t_now - t_now).count()/1000.<<endl;
            cerr<<"seq: " << seq << endl;
            double _seg_bhr = double(seg_byte_hit) / seg_byte_req;
            double _seg_ohr = double(seg_obj_hit) / seg_obj_req;
            cerr<<"accu bhr: " << double(byte_hit) / byte_req << endl;
            cerr<<"seg bhr: " << _seg_bhr << endl;
            seg_bhr+=to_string(_seg_bhr)+"\t";
            seg_ohr+=to_string(_seg_ohr)+"\t";
            seg_byte_hit=seg_obj_hit=seg_byte_req=seg_obj_req=0;
            t_now = _t_now;
        }
    }

    infile.close();

    map<string, string> res = {
            {"byte_hit_rate", to_string(double(byte_hit) / byte_req)},
            {"object_hit_rate", to_string(double(obj_hit) / obj_req)},
            {"segment_byte_hit_rate", seg_bhr},
            {"segment_object_hit_rate", seg_ohr},
#ifdef MISS_DECOUPLE
            {"miss_decouple", miss_stat.yaml_dump()},
#endif
    };
    return res;
}

map<string, string> simulation(string trace_file, string cache_type,
        uint64_t cache_size, map<string, string> params){
    if (cache_type == "Belady" || cache_type == "BeladySample" || cache_type == "LRUKSample" ||
        cache_type == "LFUSample")
        return _simulation_future(trace_file, cache_type, cache_size, params);
    else if (cache_type == "Adaptive-TinyLFU")
        return _simulation_tinylfu(trace_file, cache_type, cache_size, params);
//    else if (cache_type == "LFO")
//        return LFO::_simulation_lfo(trace_file, cache_type, cache_size, params);
//    else if (cache_type == "LFO2")
//        return _simulation_lfo2(trace_file, cache_type, cache_size, params);
//    else if (cache_type == "LRBelady")
//        return _simulation_lr_belady(trace_file, cache_type, cache_size, params);
//    else if (cache_type == "BeladyStatic")
//        return _simulation_belady_static(trace_file, cache_type, cache_size, params);
//    else if (cache_type == "Bins")
//        return _simulation_bins(trace_file, cache_type, cache_size, params);
   else if (cache_type == "BeladyTruncate")
       return _simulation_truncate(trace_file, cache_type, cache_size, params);
    else
        return _simulation(trace_file, cache_type, cache_size, params);
}
