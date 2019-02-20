//
// Created by Zhenyu Song on 11/13/18.
//

#include "annotate.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <limits>
#include <unordered_map>
#include <chrono>
#include <cstdint>

//max to 10 billion
const uint64_t max_next_seq = 10000000000;
using namespace std;

class AnnotatedRequest_
{
public:
    uint64_t _next_seq;
    uint64_t _t;
    uint64_t _id; // request object id
    uint64_t _size; // request size in bytes
    vector<uint64_t > _extra_features;

    // Create request
    AnnotatedRequest_(uint64_t id, uint64_t size, uint64_t t, uint64_t next_seq,
            vector<uint64_t >* extra_features = nullptr)
            : _id(id), _size(size), _t(t), _next_seq(next_seq) {
        if (extra_features)
            _extra_features = *extra_features;
    }
};


void annotate(string &trace_file, uint n_extra_fields) {
    //todo: there is a risk that multiple process write a same file

    auto expect_file = trace_file+".ant";
    ifstream cachefile(expect_file);
    if (cachefile.good()) {
        cerr<<"file has been annotated, so skip annotation"<<endl;
        return;
    }


    // parse trace file
    vector<AnnotatedRequest_> trace;
    uint64_t t, id, size;
    uint64_t i = 0;

    ifstream infile;
    infile.open(trace_file);
    if (!infile) {
        cerr << "Exception opening/reading annotate original file"<<endl;
        exit(-1);
    }

    //todo: read extra fields
    vector<uint64_t > extra_features(n_extra_fields, 0);

    while(infile>> t >> id >> size) {
        for (int j = 0; j < n_extra_fields; ++j)
            infile>>extra_features[j];
        if (!(++i%1000000))
            cerr<<"reading origin trace: "<<i<<endl;
        if (n_extra_fields) {
            //default with infinite future interval
            trace.emplace_back(id, size, t, max_next_seq, &extra_features);
        } else {
            trace.emplace_back(id, size, t, max_next_seq);
        }
    }

    uint64_t totalReqc = trace.size();
    std::cerr << "scanned trace n=" << totalReqc << std::endl;
    if (totalReqc > max_next_seq) {
        cerr<<"Error: don't support more trace length than "<<max_next_seq<<endl;
        exit(-1);
    }

    // get nextSeen indices
    // assume id has same size
    i = 0;
    unordered_map<uint64_t, uint64_t > lastSeen;
    for (auto it = trace.rbegin(); it != trace.rend(); ++it) {
        auto lit = lastSeen.find(it->_id);
        if (lit != lastSeen.end())
            it->_next_seq = lit->second;
        lastSeen[it->_id] = totalReqc - i;
        if (!(++i%1000000))
            cerr<<"computing next t: "<<i<<endl;
    }

    // get current time
    string now;
    auto timenow = chrono::system_clock::to_time_t(chrono::system_clock::now());

    ofstream outfile;
    auto tmp_file = "/tmp/" + to_string(timenow);
    cerr<<"writing the annotated trace "<<tmp_file<<endl;

    outfile.open(tmp_file);
    if (!outfile) {
        cerr << "Exception opening/reading tmp file"<<endl;
        exit(-1);
    }

    //no need to write seq, which is implicit
    i = 0;
    for (auto & it: trace) {
        outfile << it._next_seq << " " << it._t << " " << it._id << " " << it._size;
        for (int j = 0; j < n_extra_fields; ++j)
            outfile << " " << it._extra_features[j];
        outfile <<endl;
        if (!(++i%1000000))
            cerr<<"writing: "<<i<<endl;
    }

    outfile.close();

    //todo: fat has bug in renaming: cross link file system. Actually it's better to do this in python with c wrapper
    system(("mv "+tmp_file+" "+expect_file).c_str());
//    if (rename(tmp_file.c_str(), expect_file.c_str())) {
//        cerr << "Exception in renaming file from "<<tmp_file<<" to "<<expect_file<<" code: "<<strerror(errno)<< endl;
//        return;
//    }

}
