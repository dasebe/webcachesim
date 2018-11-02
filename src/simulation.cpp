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

map<string, double> _simulation_belady(string trace_file, string cache_type, uint64_t cache_size,
                                       map<string, double> params){
    // create cache
    unique_ptr<Cache> webcache = move(Cache::create_unique(cache_type));
    // todo: raise exception?
//    if(webcache == nullptr)
//        return 1;

    // configure cache size
    webcache->setSize(cache_size);

    //suppose already annotated
    ifstream infile;
    long long byte_req = 0, byte_hit = 0, obj_req = 0, obj_hit = 0;
    long long t, id, size, next_t;

//    cerr << "running..." << endl;
    trace_file += ".ant";
    infile.open(trace_file);
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

//    delete req;

    infile.close();

    map<string, double> res = {
            {"byte_hit_rate", double(byte_hit) / byte_req},
            {"object_hit_rate", double(obj_hit) / obj_req},
    };
//    return {double(byte_hit) / byte_req, double(obj_hit) / obj_req};
    return res;
}

map<string, double> _simulation(string trace_file, string cache_type, uint64_t cache_size,
                                map<string, double> params){
    // create cache
    unique_ptr<Cache> webcache = move(Cache::create_unique(cache_type));
    // todo: raise exception?
//    if(webcache == nullptr)
//        return 1;

    // configure cache size
    webcache->setSize(cache_size);

// todo: omit params at first
//  // parse cache parameters
//  regex opexp ("(.*)=(.*)");
//  cmatch opmatch;
//  string paramSummary;
//  for(int i=4; i<argc; i++) {
//    regex_match (argv[i],opmatch,opexp);
//    if(opmatch.size()!=3) {
//      cerr << "each cacheParam needs to be in form name=value" << endl;
//      return 1;
//    }
//    webcache->setPar(opmatch[1], opmatch[2]);
//    paramSummary += opmatch[2];
//  }
    ifstream infile;
    long long byte_req = 0, byte_hit = 0, obj_req = 0, obj_hit = 0;
    long long t, id, size;

//    cerr << "running..." << endl;

    infile.open(trace_file);
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

//    delete req;

    infile.close();

//    return {double(byte_hit) / byte_req, double(obj_hit) / obj_req};
    map<string, double> res = {
            {"byte_hit_rate", double(byte_hit) / byte_req},
            {"object_hit_rate", double(obj_hit) / obj_req},
    };
//    return {double(byte_hit) / byte_req, double(obj_hit) / obj_req};
    return res;
}

map<string, double> simulation(string trace_file, string cache_type, uint64_t cache_size, map<string, double> params){
    if (cache_type == "Belady")
        return _simulation_belady(trace_file, cache_type, cache_size, params);
    else
        return _simulation(trace_file, cache_type, cache_size, params);
}
