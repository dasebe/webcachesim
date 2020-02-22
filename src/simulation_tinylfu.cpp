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
#include "bsoncxx/builder/basic/document.hpp"
#include "bsoncxx/json.hpp"

using namespace std;
using namespace boost;
using bsoncxx::builder::basic::kvp;
using bsoncxx::builder::basic::sub_array;

string exec(const char* cmd) {
    std::array<char, 4 * 1024 * 1024> buffer = {0};
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

bsoncxx::builder::basic::document _simulation_tinylfu(string trace_file, string cache_type, uint64_t cache_size,
                                                      map<string, string> params) {

    string cmd = "java -jar ${WEBCACHESIM_ROOT}/lib/tinylfu/tinylfu.jar " + trace_file + " Adaptive-TinyLFU " \
 + to_string(cache_size);
    for (auto k: params) {
        cmd += " " + k.first + " " + k.second;
    }
    cerr << "calling external command: " << cmd << endl;
    string r = exec(cmd.c_str());
    vector<string> strs;
    split(strs, r, is_any_of(" "));

    vector<int64_t> seg_byte_req, seg_byte_miss, seg_object_req, seg_object_miss;
    for (int i = 0; i < strs.size(); i += 4) {
        seg_byte_req.emplace_back(stoull(strs[i]));
        seg_byte_miss.emplace_back(stoull(strs[i + 1]));
        seg_object_req.emplace_back(stoull(strs[i + 2]));
        seg_object_miss.emplace_back(stoull(strs[i + 3]));
    }

    bsoncxx::builder::basic::document value_builder{};
    value_builder.append(kvp("segment_byte_miss", [&seg_byte_miss](sub_array child) {
        for (const auto &element : seg_byte_miss)
            child.append(element);
    }));
    value_builder.append(kvp("segment_byte_req", [&seg_byte_req](sub_array child) {
        for (const auto &element : seg_byte_req)
            child.append(element);
    }));
    value_builder.append(kvp("segment_object_miss", [&seg_object_miss](sub_array child) {
        for (const auto &element : seg_object_miss)
            child.append(element);
    }));
    value_builder.append(kvp("segment_object_req", [&seg_object_req](sub_array child) {
        for (const auto &element : seg_object_req)
            child.append(element);
    }));

    return value_builder;
}