//
// Created by Zhenyu Song on 10/30/18.
//

#include "simulation.h"

#include <fstream>
#include <string>
#include <regex>
#include "lru_variants.h"
#include "gd_variants.h"
#include "random_variants.h"
#include "ucb.h"
#include "request.h"
#include "simulation_belady.h"
#include "simulation_lfo.h"
#include "simulation_lfo2.h"
#include "simulation_lr.h"
#include <chrono>
#include "utils.h"

using namespace std;


map<string, string> _simulation(string trace_file, string cache_type, uint64_t cache_size,
                                map<string, string> params){
    // create cache
    unique_ptr<Cache> webcache = move(Cache::create_unique(cache_type));
    if(webcache == nullptr) {
        cerr<<"cache type not implemented"<<endl;
        return {};
    }

    // configure cache size
    webcache->setSize(cache_size);

    uint64_t n_warmup = 0;
    bool uni_size = false;
    for (auto& kv: params) {
        webcache->setPar(kv.first, kv.second);
        if (kv.first == "n_warmup")
            n_warmup = stoull(kv.second);
        if (kv.first == "uni_size")
            uni_size = static_cast<bool>(stoi(kv.second));
    }

    ifstream infile;
    uint64_t byte_req = 0, byte_hit = 0, obj_req = 0, obj_hit = 0;
    uint64_t t, id, size;

    infile.open(trace_file);
    if (!infile) {
        cerr << "Exception opening/reading file"<<endl;
        return {};
    }

    SimpleRequest req(0, 0);
    uint64_t seq = 0;
    auto t_now = chrono::system_clock::now();
    while (infile >> t >> id >> size) {
        if (uni_size)
            size = 1;

        if (seq >= n_warmup) {
            update_metric_req(byte_req, obj_req, size);
            update_metric_req(seg_byte_req, seg_obj_req, size);
        }

        req.reinit(id, size);
        if (webcache->lookup(req)) {
            if (seq >= n_warmup) {
                update_metric_req(byte_hit, obj_hit, size);
                update_metric_req(seg_byte_hit, seg_obj_hit, size);
            }
        } else {
            webcache->admit(req);
        }

        ++seq;

        if (!(seq%window)) {
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

map<string, string> simulation(string trace_file, string cache_type,
        uint64_t cache_size, map<string, string> params){
    if (cache_type == "Belady")
        return _simulation_belady(trace_file, cache_type, cache_size, params);
    else if (cache_type == "LFO")
        return LFO::_simulation_lfo(trace_file, cache_type, cache_size, params);
    else if (cache_type == "LFO2")
        return _simulation_lfo2(trace_file, cache_type, cache_size, params);
    else if (cache_type == "LR" || cache_type == "BeladySample")
        return _simulation_lr(trace_file, cache_type, cache_size, params);
    else
        return _simulation(trace_file, cache_type, cache_size, params);
}
