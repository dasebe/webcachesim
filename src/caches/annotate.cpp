//
// Created by Zhenyu Song on 11/13/18.
//

#include "annotate.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <limits>
#include <map>
#include <chrono>

using namespace std;

void annotate(string &trace_file) {
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
            cerr<<i<<endl;
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
            cerr<<i<<endl;
        auto lit = lastSeen.find(make_pair(get<1>(*it), get<2>(*it)));
        if (lit != lastSeen.end())
            get<3>(*it) = lit->second;
        lastSeen[make_pair(get<1>(*it), get<2>(*it))] = get<0>(*it);
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
