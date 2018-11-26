//
// Created by zhenyus on 11/8/18.
//

#include "simulation_belady.h"
#include <fstream>
#include "request.h"
#include "lru_variants.h"
#include "annotate.h"

using namespace std;



map<string, string> _simulation_belady(string trace_file, string cache_type, uint64_t cache_size,
                                       map<string, string> params){
    //annotate a file
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
    for (auto& kv: params) {
        webcache->setPar(kv.first, kv.second);
        if (kv.first == "n_warmup")
            n_warmup = stoull(kv.second);
        if (kv.first == "uni_size")
            uni_size = static_cast<bool>(stoi(kv.second));
    }

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

    cout<<"simulating"<<endl;
    AnnotatedRequest req(0, 0, 0, 0);
    uint64_t i = 0;
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
//        cout << i << " " << t << " " << obj_hit << endl;
        if (!(++i%1000000))
            cout <<"seq: "<< i <<" hit rate: "<<double(byte_hit) / byte_req<< endl;
    }

    infile.close();

    map<string, string> res = {
            {"byte_hit_rate", to_string(double(byte_hit) / byte_req)},
            {"object_hit_rate", to_string(double(obj_hit) / obj_req)},
    };
    return res;
}