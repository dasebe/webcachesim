//
// Created by Zhenyu Song on 10/30/18.
//

#include "simulation.h"

#include <fstream>
#include <string>
#include <regex>
#include "lru_variants.h"
#include "gd_variants.h"
#include "request.h"

using namespace std;

map<string, string> _simulation_belady(string trace_file, string cache_type, uint64_t cache_size,
                                       map<string, string> params){
    // create cache
    unique_ptr<Cache> webcache = move(Cache::create_unique(cache_type));
    if(webcache == nullptr) {
        cerr<<"cache type not implemented"<<endl;
        return {};
    }

    // configure cache size
    webcache->setSize(cache_size);

    for (auto& kv: params) {
        webcache->setPar(kv.first, kv.second);
    }

    //suppose already annotated
    ifstream infile;
    long long byte_req = 0, byte_hit = 0, obj_req = 0, obj_hit = 0;
    long long t, id, size, next_t;

    trace_file += ".ant";

    infile.open(trace_file);
    if (!infile) {
        cerr << "Exception opening/reading file";
        return {};
    }


    AnnotatedRequest req(0, 0, 0);
    int i = 0;
    while (infile >> t >> id >> size >> next_t) {
        byte_req += size;
        obj_req++;

        req.reinit(id, size, next_t);
        if (webcache->lookup(req)) {
            byte_hit += size;
            obj_hit++;
        } else {
            webcache->admit(req);
        }
//        cout << i << " " << t << " " << obj_hit << endl;
        i++;
    }

    infile.close();

    map<string, string> res = {
            {"byte_hit_rate", to_string(double(byte_hit) / byte_req)},
            {"object_hit_rate", to_string(double(obj_hit) / obj_req)},
    };
    return res;
}

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

    for (auto& kv: params) {
        webcache->setPar(kv.first, kv.second);
    }

    ifstream infile;
    long long byte_req = 0, byte_hit = 0, obj_req = 0, obj_hit = 0;
    long long t, id, size;

    infile.open(trace_file);
    if (!infile) {
        cerr << "Exception opening/reading file";
        return {};
    }

    SimpleRequest req(0, 0);
    int i = 0;
    while (infile >> t >> id >> size) {
        byte_req += size;
        obj_req++;

        req.reinit(id, size);
        if (webcache->lookup(req)) {
            byte_hit += size;
            obj_hit++;
        } else {
            webcache->admit(req);
        }
//        cout << i << " " << t << " " << obj_hit << endl;
        i++;
    }

    infile.close();

    map<string, string> res = {
            {"byte_hit_rate", to_string(double(byte_hit) / byte_req)},
            {"object_hit_rate", to_string(double(obj_hit) / obj_req)},
    };
    return res;
}

map<string, string> simulation(string trace_file, string cache_type, uint64_t cache_size, map<string, string> params){
    if (cache_type == "Belady")
        return _simulation_belady(trace_file, cache_type, cache_size, params);
    else
        return _simulation(trace_file, cache_type, cache_size, params);
}
