//
// Created by zhenyus on 11/8/18.
//

#include "simulation_lr.h"
#include "random_variants.h"
#include <fstream>
#include "request.h"
#include "annotate.h"
#include <chrono>


using namespace std;



map<string, string> _simulation_lr(string trace_file, string cache_type, uint64_t cache_size,
                                    map<string, string> params){
    //annotate a file
    //not necessary to annotate, but it's easier
    annotate(trace_file);

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

    for (auto it = params.cbegin(); it != params.cend();) {
        if (it->first == "n_warmup") {
            n_warmup = stoull(it->second);
            it = params.erase(it);
        } else if (it->first == "uni_size") {
            uni_size = static_cast<bool>(stoi(it->second));
            it = params.erase(it);
        } else {
            ++it;
        }
    }

    webcache->init_with_params(params);

    //suppose already annotated
    ifstream infile;
    uint64_t byte_req = 0, byte_hit = 0, obj_req = 0, obj_hit = 0;
    uint64_t t, id, size, next_t;

    trace_file += ".ant";
    infile.open(trace_file);
    if (!infile) {
        cerr << "exception opening/reading file"<<endl;
        return {};
    }


    cerr<<"simulating"<<endl;
    AnnotatedRequest req(0, 0, 0, 0);
    uint64_t i = 0;
    auto t_now = chrono::system_clock::now();

    while (infile >> t >> id >> size >> next_t) {
        if (uni_size)
            size = 1;

        if (i >= n_warmup) {
            byte_req += size;
            obj_req++;
        }

        req.reinit(id, size, t, next_t);
        if (webcache->lookup(req)) {
            if (i >= n_warmup) {
                byte_hit += size;
                obj_hit++;
            }
        } else {
            webcache->admit(req);
        }
//        cerr << i << " " << t << " " << obj_hit << endl;
        if (!(++i%1000000)) {
            auto _t_now = chrono::system_clock::now();
            cerr<<"delta t: "<<chrono::duration_cast<std::chrono::seconds>(_t_now - t_now).count()<<endl;
            cerr << "seq: " << i << " hit rate: " << double(byte_hit) / byte_req << endl;
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
