//
// Created by zhenyus on 2/21/20.
//

#include "trace_sanity_check.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <unordered_map>

using namespace std;

//max object size is 4GB
const uint64_t max_obj_size = 0xffffffff;
//max n req in a trace is 4 Billion
const uint64_t max_n_req = 0xffffffff;
//max n_extra_field = 4, as this field is statically allocated
const int max_n_extra_fields = 4;
//extra_value is uint16_t
const int max_extra = 0xffff;


int get_n_fields(const string& filename) {
    std::ifstream infile(filename);
    if (!infile) {
        throw std::runtime_error("Exception opening file "+filename);
    }
    //get whether file is in a correct format
    std::string line;
    getline(infile, line);
    std::istringstream iss(line);
    uint64_t tmp;
    int counter = 0;
    while (iss >> tmp) {
        ++counter;
    }
    infile.close();
    return counter;
}

void trace_sanity_check(const string& file_name) {
    cerr<<"running sanity check on trace: "<<file_name<<endl;
    bool if_pass = true;

    ifstream infile(file_name);
    if (!infile) {
        throw std::runtime_error("Exception opening file "+file_name);
    }

    //assume format: t id size extra0 extra1 ...
    int n_extra_fields = get_n_fields(file_name) - 3;
    if (n_extra_fields > max_n_extra_fields) {
        cerr<<"error: n_extra_fields "<<n_extra_fields<<" > max_n_extra_fields "<<max_n_extra_fields<<endl;
        if_pass = false;
    }
    cerr<<"n_extra_fields: "<<n_extra_fields<<endl;

    uint64_t t, key, size;
    vector<uint64_t > extra_features(n_extra_fields);
    //key -> size
    unordered_map<uint64_t, uint32_t> size_map;
    uint64_t n_req = 0;

    while (infile >> t >> key >> size) {
        for (int i = 0; i < n_extra_fields; ++i) {
            infile >> extra_features[i];
        }

        for (int i = 0; i < n_extra_fields; ++i) {
            if (extra_features[i] > max_extra) {
                cerr<<"req: "<<n_req<<" extra "<<i<<":"<<extra_features[i]<<" > max_extra "<<max_extra<<endl;
                if_pass = false;
            }
        }

        //check
        if (size > max_obj_size) {
            cerr<<"req: "<<n_req<<" size "<<size<<" > max_obj_size "<<max_obj_size<<endl;
            if_pass = false;
        }
        if (size == 0) {
            cerr<<"req: "<<n_req<<" size == 0"<<endl;
            if_pass = false;
        }

        auto it = size_map.find(key);
        if (it == size_map.end()) {
            size_map.insert({key, size});
        } else {
            if (it->second != size) {
                cerr<<"req: "<<n_req<<", key: "  <<key<<" size inconsistent. Old size: "<<it->second<<" new size: "<<size<<endl;
                if_pass = false;
            }
        }
        if (!(n_req%1000000)) {
            cerr<<"n_req: "<<n_req<<endl;
        }
        ++n_req;
    }

    if (n_req > max_n_req) {
        cerr<<"n_req "<<n_req<<" > max_n_req "<<max_n_req<<endl;
        if_pass = false;
    }

    if (true == if_pass) {
        cerr<<"pass sanity check"<<endl;
    } else {
        throw std::runtime_error("fail sanity check");
    }
    infile.close();
}

