//
// Created by zhenyus on 2/25/19.
//

#include "simulation_tinylfu.h"
#include <cstdio>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <array>
#include <string.h>
#include <boost/algorithm/string.hpp>
#include <vector>

using namespace std;
using namespace boost;

string exec(const char* cmd) {
    std::array<char, 4096> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

using namespace std;

map<string, string> _simulation_tinylfu(string trace_file, string cache_type, uint64_t cache_size,
                                       map<string, string> params) {

    cerr<<"error: need to refactor as other simulation"<<endl;
    abort();
//    string cmd = "java -jar ${WEBCACHESIM_ROOT}/lib/tinylfu/tinylfu.jar " + trace_file + " Adaptive-TinyLFU " \
//            + to_string(cache_size);
//    for (auto k: params) {
//        cmd += " "+k.first+" "+k.second;
//    }
//    string r = exec(cmd.c_str());
//    vector<string> strs;
//    split(strs, r, is_any_of("\n"));
//
//    map<string, string> res = {
//            {"byte_hit_rate", strs[0]},
//            {"object_hit_rate", strs[1]},
//    };
//
//    return res;
}