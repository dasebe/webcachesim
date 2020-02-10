//
// Created by Zhenyu Song on 10/30/18.
//

#include "simulation.h"
#include "annotate.h"
#include "simulation_tinylfu.h"
#include <sstream>
#include "utils.h"
#include "rss.h"
#include <cstdint>
#include <unordered_map>
#include <numeric>
#include "bsoncxx/builder/basic/document.hpp"
#include "bsoncxx/json.hpp"

using namespace std;
using namespace chrono;
using bsoncxx::builder::basic::kvp;
using bsoncxx::builder::basic::sub_array;


FrameWork::FrameWork(const string &trace_file, const string &cache_type, const uint64_t &cache_size,
                     map<string, string> &params) {
    //set cache_type related
    _cache_type = cache_type;
    _cache_size = cache_size;
    is_offline = offline_algorithms.count(_cache_type);

#ifdef EVICTION_LOGGING
    //logging eviction requires next_seq information
    is_offline = true;
#endif


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
#ifdef EVICTION_LOGGING
            if (true == is_metadata_in_cache_size) {
                throw invalid_argument(
                        "error: set is_metadata_in_cache_size while EVICTION_LOGGING. Must not consider metadata overhead");
            }
#endif
            it = params.erase(it);
        } else if (it->first == "bloom_filter") {
            bloom_filter = static_cast<bool>(stoi(it->second));
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
        } else {
            ++it;
        }
    }
    webcache->init_with_params(params);

    //trace_file related init
    _trace_file = trace_file;
    check_n_extra_field();
    if (is_offline) {
        annotate(trace_file, n_extra_fields);
    }

    if (is_offline) {
        _trace_file = _trace_file + ".ant";
    }
    infile.open(trace_file);
    if (!infile) {
        cerr << "Exception opening/reading file " << trace_file << endl;
        exit(-1);
    }
    adjust_real_time_offset();
    extra_features = vector<uint16_t>(n_extra_fields);
}

void FrameWork::check_n_extra_field() {
    //TODO: take trace_file_name as an argument?
    std::ifstream in_file(_trace_file);
    if (!in_file) {
        throw std::runtime_error("Exception opening/reading file " + _trace_file);
    }

    //get whether file is in a correct format
    std::string line;
    getline(in_file, line);
    istringstream iss(line);
    uint64_t tmp;
    int counter = 0;
    while (iss >> tmp) {
        ++counter;
    }
    //format: n_seq t id size [extra]
    int n_base_field = 3;
    n_extra_fields = counter - n_base_field;
    in_file.close();
}

void FrameWork::adjust_real_time_offset() {
    // Zhenyu: not assume t start from any constant, so need to compute the first window
    if (is_offline) {
        infile >> next_seq >> t;
    } else {
        infile >> t;
    }
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
    cerr << "\nsegment id: " << seq / segment_window - 1 << endl
         << "cache size: " << webcache->_currentSize << "/" << webcache->_cacheSize
         << " (" << ((double) webcache->_currentSize) / webcache->_cacheSize << ")" << endl
         << "delta t: " << chrono::duration_cast<std::chrono::milliseconds>(_t_now - t_now).count() / 1000.
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
    seg_rss.emplace_back(metadata_overhead);
    webcache->update_stat_periodic();
    if (is_metadata_in_cache_size)
        webcache->setSize(_cache_size - metadata_overhead);
    cerr << "rss: " << metadata_overhead << endl;
}


bsoncxx::builder::basic::document FrameWork::simulate() {
    cerr << "simulating" << endl;
    unordered_map<uint64_t, uint32_t> future_timestamps;
    vector<uint8_t> eviction_qualities;
    vector<uint16_t> eviction_logic_timestamps;
    if (bloom_filter) {
        filter = new AkamaiBloomFilter;
    }

    SimpleRequest *req;
    if (is_offline)
        req = new AnnotatedRequest(0, 0, 0, 0);
    else
        req = new SimpleRequest(0, 0, 0);
    t_now = system_clock::now();

    while (true) {
        if (is_offline) {
            if (!(infile >> next_seq >> t >> id >> size))
                break;
        } else {
            if (!(infile >> t >> id >> size))
                break;
        }

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

        if (is_offline)
            dynamic_cast<AnnotatedRequest *>(req)->reinit(id, size, seq, next_seq, &extra_features);
        else
            req->reinit(id, size, seq, &extra_features);

        bool seen = (!bloom_filter) || filter->exist_or_insert(id);
        if (seen) {
            bool is_hit = webcache->lookup(*req);
            if (!is_hit) {
                update_metric_req(byte_miss, obj_miss, size);
                update_metric_req(rt_byte_miss, rt_obj_miss, size)
                webcache->admit(*req);
            }
        } else {
            update_metric_req(byte_miss, obj_miss, size);
            update_metric_req(rt_byte_miss, rt_obj_miss, size)
        }

        ++seq;
    }
    delete req;
    //for the residue segment of trace
    update_real_time_stats();
    update_stats();
    infile.close();

    return simulation_results();
}


bsoncxx::builder::basic::document FrameWork::simulation_results() {
    bsoncxx::builder::basic::document value_builder{};
    value_builder.append(kvp("no_warmup_byte_miss_ratio",
                             accumulate<vector<int64_t>::const_iterator, double>(seg_byte_miss.begin(),
                                                                                 seg_byte_miss.end(), 0) /
                             accumulate<vector<int64_t>::const_iterator, double>(seg_byte_req.begin(),
                                                                                 seg_byte_req.end(), 0)
    ));
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
    auto res = frame_work.simulate();
    return res;
}

bsoncxx::builder::basic::document simulation(string trace_file, string cache_type,
                                             uint64_t cache_size, map<string, string> params) {
    if (cache_type == "Adaptive-TinyLFU")
        return _simulation_tinylfu(trace_file, cache_type, cache_size, params);
    else
        return _simulation(trace_file, cache_type, cache_size, params);
}
