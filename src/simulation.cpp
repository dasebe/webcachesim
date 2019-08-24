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
//remove the code related with decouple because not using it for a long time
//#include "miss_decouple.h"
//#include "cache_size_decouple.h"
#include "nlohmann/json.hpp"
#include <proc/readproc.h>
#include <cstdint>

using namespace std;
using namespace chrono;
using json = nlohmann::json;


FrameWork::FrameWork(const string &trace_file, const string &cache_type, const uint64_t &cache_size,
                     map<string, string> &params) {

    if (params.find("n_extra_fields") == params.end()) {
        cerr << "error: field n_extra_fields is required" << endl;
        abort();
    }
    annotate(trace_file, stoul(params["n_extra_fields"]));

    _trace_file = trace_file;
    _cache_type = cache_type;
    _cache_size = cache_size;

    // create cache
    webcache = move(Cache::create_unique(cache_type));
    if (webcache == nullptr) {
        cerr << "cache type not implemented" << endl;
        abort();
    }

    // configure cache size
    webcache->setSize(cache_size);

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
    infile.open(trace_file + ".ant");
    if (!infile) {
        cerr << "Exception opening/reading file" << endl;
        exit(-1);
    }
    check_trace_format();
    adjust_real_time_offset();
    extra_features = vector<uint16_t>(n_extra_fields);
}

void FrameWork::check_trace_format() {
    //get whether file is in a correct format
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

void FrameWork::adjust_real_time_offset() {
    // Zhenyu: not assume t start from any constant, so need to compute the first window
    infile >> next_seq >> t;
    time_window_end =
            real_time_segment_window * (t / real_time_segment_window + (t % real_time_segment_window != 0));
    infile.clear();
    infile.seekg(0, ios::beg);
}


void FrameWork::update_real_time_stats() {
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

void FrameWork::update_stats() {
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
        webcache->setSize(_cache_size - metadata_overhead);
    cerr << "rss: " << metadata_overhead << endl;
}


void FrameWork::simulate() {
    cerr << "simulating" << endl;
    SimpleRequest *req;
    if (_cache_type == "Belady")
        req = new AnnotatedRequest(0, 0, 0, 0);
    else
        req = new SimpleRequest(0, 0, 0);
    t_now = system_clock::now();

    while (infile >> next_seq >> t >> id >> size) {
        for (int i = 0; i < n_extra_fields; ++i)
            infile >> extra_features[i];
        if (uni_size)
            size = 1;

        DPRINTF("seq: %lu\n", seq);

        while (t >= time_window_end)
            update_real_time_stats();
        if (seq && !(seq % segment_window))
            update_stats();

        update_metric_req(byte_req, obj_req, size);
        update_metric_req(rt_byte_req, rt_obj_req, size)

        if (_cache_type == "Belady") {
            dynamic_cast<AnnotatedRequest *>(req)->reinit(id, size, seq, next_seq, &extra_features);
        } else {
            req->reinit(id, size, seq, &extra_features);
        }
        bool is_hit = webcache->lookup(*req);
        if (!is_hit) {
            update_metric_req(byte_miss, obj_miss, size);
            update_metric_req(rt_byte_miss, rt_obj_miss, size)
            webcache->admit(*req);
        }
        ++seq;
    }
    //for the residue segment of trace
    update_real_time_stats();
    update_stats();
    infile.close();
}


std::map<std::string, std::string> FrameWork::simulation_results() {
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
    };

    webcache->update_stat(res);
    return res;
}

map<string, string> _simulation(string trace_file, string cache_type, uint64_t cache_size,
                                map<string, string> params){
    FrameWork frame_work(trace_file, cache_type, cache_size, params);
    frame_work.simulate();
    return frame_work.simulation_results();
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
