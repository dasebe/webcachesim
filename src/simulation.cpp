//
// Created by Zhenyu Song on 10/30/18.
//

#include "simulation.h"
//#include <string>
//#include "request.h"
#include "annotate.h"
//#include "simulation_tinylfu.h"
//#include "cache.h"
////#include "simulation_lr_belady.h"
////#include "simulation_belady_static.h"
////#include "simulation_bins.h"
////#include "simulation_truncate.h"
#include <sstream>
#include "utils.h"
////#include "simulation_lfo.h"
////remove the code related with decouple because not using it for a long time
////#include "miss_decouple.h"
////#include "cache_size_decouple.h"
//#include "nlohmann/json.hpp"
#include "rss.h"
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include "bsoncxx/builder/basic/document.hpp"
#include "bsoncxx/json.hpp"

using namespace std;
using namespace chrono;
//using json = nlohmann::json;
using bsoncxx::builder::basic::kvp;
using bsoncxx::builder::basic::sub_array;


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
        } else if (it->first == "n_early_stop") {
            n_early_stop = stoll((it->second));
            ++it;
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
    auto metadata_overhead = get_rss();
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
    auto metadata_overhead = get_rss();
    rt_seg_rss.emplace_back(metadata_overhead);
    seg_rss.emplace_back(metadata_overhead);
    if (is_metadata_in_cache_size)
        webcache->setSize(_cache_size - metadata_overhead);
    cerr << "rss: " << metadata_overhead << endl;
}


void FrameWork::simulate() {
    cerr << "simulating" << endl;
    unordered_map<uint64_t, uint32_t> future_timestamps;
    vector<uint8_t> eviction_qualities;
    vector<uint16_t> eviction_logic_timestamps;

    SimpleRequest *req;
    unordered_set<string> offline_algorithms = {"Belady", "BeladySample", "LRUKSample", "LFUSample", "WLC", "LRU",
                                                "LRUK", "LFUDA", "LeCaR", "FIFO", "BloomFilter", "LFU", "S4LRU",
                                                "AdaptSize", "GDSF"};
    if (offline_algorithms.count(_cache_type))
        req = new AnnotatedRequest(0, 0, 0, 0);
    else
        req = new SimpleRequest(0, 0, 0);
    t_now = system_clock::now();

    while (infile >> next_seq >> t >> id >> size) {
        if (seq == n_early_stop)
            break;
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

        if (offline_algorithms.count(_cache_type))
            dynamic_cast<AnnotatedRequest *>(req)->reinit(id, size, seq, next_seq, &extra_features);
        else
            req->reinit(id, size, seq, &extra_features);

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


bsoncxx::builder::basic::document FrameWork::simulation_results() {
    bsoncxx::builder::basic::document value_builder{};
    value_builder.append(kvp("segment_byte_miss", [this](sub_array child) {
        for (const auto &element : seg_byte_miss)
            child.append(element);
    }));
    value_builder.append(kvp("segment_byte_req", [this](sub_array child) {
        for (const auto &element : seg_byte_req)
            child.append(element);
    }));
    value_builder.append(kvp("segment_object_miss", [this](sub_array child) {
        for (const auto &element : seg_object_miss)
            child.append(element);
    }));
    value_builder.append(kvp("segment_object_req", [this](sub_array child) {
        for (const auto &element : seg_object_req)
            child.append(element);
    }));
    value_builder.append(kvp("segment_rss", [this](sub_array child) {
        for (const auto &element : seg_rss)
            child.append(element);
    }));
    value_builder.append(kvp("real_time_segment_byte_miss", [this](sub_array child) {
        for (const auto &element : rt_seg_byte_miss)
            child.append(element);
    }));
    value_builder.append(kvp("real_time_segment_byte_req", [this](sub_array child) {
        for (const auto &element : rt_seg_byte_req)
            child.append(element);
    }));
    value_builder.append(kvp("real_time_segment_object_miss", [this](sub_array child) {
        for (const auto &element : rt_seg_object_miss)
            child.append(element);
    }));
    value_builder.append(kvp("real_time_segment_object_req", [this](sub_array child) {
        for (const auto &element : rt_seg_object_req)
            child.append(element);
    }));
    value_builder.append(kvp("real_time_segment_rss", [this](sub_array child) {
        for (const auto &element : rt_seg_rss)
            child.append(element);
    }));

    webcache->update_stat(value_builder);
    return value_builder;
}

bsoncxx::builder::basic::document _simulation(string trace_file, string cache_type, uint64_t cache_size,
                                              map<string, string> params) {
    FrameWork frame_work(trace_file, cache_type, cache_size, params);
    frame_work.simulate();
    return frame_work.simulation_results();
}

bsoncxx::builder::basic::document simulation(string trace_file, string cache_type,
                                             uint64_t cache_size, map<string, string> params) {
//    if (cache_type == "Adaptive-TinyLFU")
//        return _simulation_tinylfu(trace_file, cache_type, cache_size, params);
//    else if (cache_type == "LFO")
//        return LFO::_simulation_lfo(trace_file, cache_type, cache_size, params);
//    else if (cache_type == "BeladyTruncate")
//       return _simulation_truncate(trace_file, cache_type, cache_size, params);
//    else
        return _simulation(trace_file, cache_type, cache_size, params);
}
