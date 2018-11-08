//
// Created by zhenyus on 11/8/18.
//

#include "simulation_belady.h"
#include <fstream>
#include <vector>
#include "request.h"
#include <sys/time.h>
#include "lru_variants.h"
#include <limits>

using namespace std;

void annotate(string trace_file) {
    //todo: there is a risk that multiple process write a same file

    auto expect_file = trace_file+".ant";
    ifstream cachefile(expect_file);
    if (cachefile.good()) {
        cerr<<"file has been annotated, so skip annotation"<<endl;
        return;
    }


    // parse trace file
    vector<tuple<uint64_t, uint64_t , uint64_t, uint64_t >> trace;
    uint64_t t, id, size;
    uint64_t i = 0;

    ifstream infile;
    infile.open(trace_file);
    if (!infile) {
        cerr << "Exception opening/reading annotate original file"<<endl;
        return;
    }
    while(infile>> t >> id >> size) {
        if (!(++i%1000000))
            cout<<i<<endl;
        //default with infinite future interval
        trace.emplace_back(t, id, size, numeric_limits<uint64_t >::max()-1);
    }


    uint64_t totalReqc = trace.size();
    std::cerr << "scanned trace n=" << totalReqc << std::endl;

    // get nextSeen indices
    i = 0;
    map<pair<uint64_t, uint64_t>, uint64_t > lastSeen;
    for (auto it = trace.rbegin(); it != trace.rend(); ++it) {
        if (!(++i%1000000))
            cout<<i<<endl;
        auto lit = lastSeen.find(make_pair(get<1>(*it), get<2>(*it)));
        if (lit != lastSeen.end())
            get<3>(*it) = lit->second;
        lastSeen[make_pair(get<1>(*it), get<2>(*it))] = get<0>(*it);
    }

    // get current time
    string now;
    {
        struct timeval tp;
        gettimeofday(&tp, NULL);
        now = string(to_string(tp.tv_sec) + to_string(tp.tv_usec));
    }

    ofstream outfile;
    auto tmp_file = "/tmp/" + now;
    cout<<"writing the annotated trace "<<tmp_file<<endl;

    outfile.open(tmp_file);
    if (!outfile) {
        cerr << "Exception opening/reading tmp file"<<endl;
        return;
    }

    for (auto & it: trace) {
        outfile << get<0>(it) << " " << get<1>(it) << " " << get<2>(it) << " " << get<3>(it) <<endl;
    }

    outfile.close();

    //todo: fat has bug in renaming: cross link file system. Actually it's better to do this in python with c wrapper
    system(("mv "+tmp_file+" "+expect_file).c_str());
//    if (rename(tmp_file.c_str(), expect_file.c_str())) {
//        cerr << "Exception in renaming file from "<<tmp_file<<" to "<<expect_file<<" code: "<<strerror(errno)<< endl;
//        return;
//    }

}

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
    AnnotatedRequest req(0, 0, 0);
    uint64_t i = 0;
    while (infile >> t >> id >> size >> next_t) {
        if (uni_size)
            size = 1;

        if (i >= n_warmup) {
            byte_req += size;
            obj_req++;
        }

        req.reinit(id, size, next_t);
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
            cout<<i<<endl;
    }

    infile.close();

    map<string, string> res = {
            {"byte_hit_rate", to_string(double(byte_hit) / byte_req)},
            {"object_hit_rate", to_string(double(obj_hit) / obj_req)},
    };
    return res;
}