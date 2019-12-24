//
// Created by zhenyus on 1/19/19.
//

#include <string>
#include "simulation.h"
#include <map>
#include <unordered_set>
#include <cstdlib>
#include <iostream>
#include "bsoncxx/builder/basic/document.hpp"
#include "bsoncxx/json.hpp"
#include "mongocxx/client.hpp"
#include "mongocxx/instance.hpp"
#include "mongocxx/uri.hpp"

using namespace std;
using namespace chrono;
using bsoncxx::builder::basic::kvp;

string current_timestamp() {
    time_t now = system_clock::to_time_t(std::chrono::system_clock::now());
    char buf[100] = {0};
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
    return buf;
}

int main(int argc, char *argv[]) {
    // output help if insufficient params
    if (argc < 4) {
        cerr
                << "webcachesim_cli_db traceFile cacheType cacheSize [param_1 value_1] [param_2 value_2] ... [param_n value_n]"
                << endl;
        return 1;
    }

    map<string, string> params;

    for (int i = 4; i < argc; i += 2) {
        cerr << argv[i] << ": " << argv[i + 1] << endl;
        params[argv[i]] = argv[i + 1];
    }

    auto webcachesim_trace_dir = getenv("WEBCACHESIM_TRACE_DIR");
    if (!webcachesim_trace_dir) {
        cerr << "error: WEBCACHESIM_TRACE_DIR is not set, can not find trace" << endl;
        abort();
    }

    string task_id;

    bsoncxx::builder::basic::document key_builder{};
    bsoncxx::builder::basic::document value_builder{};
    key_builder.append(kvp("trace_file", argv[1]));
    key_builder.append(kvp("cache_type", argv[2]));
    key_builder.append(kvp("cache_size", argv[3]));

    for (auto &k: params) {
        //don't store authentication information
        if (unordered_set<string>({"dburl"}).count(k.first)) {
            continue;
        }
        if (!unordered_set<string>({"dbcollection", "task_id"}).count(k.first)) {
            key_builder.append(kvp(k.first, k.second));
        } else {
            value_builder.append(kvp(k.first, k.second));
        }
    }
    for (bsoncxx::document::element ele: key_builder.view())
        value_builder.append(kvp(ele.key(), ele.get_value()));


    mongocxx::instance inst;

    auto timeBegin = chrono::system_clock::now();
    auto res = simulation(string(webcachesim_trace_dir) + '/' + argv[1], argv[2], std::stoull(argv[3]),
                          params);
    auto simulation_time = chrono::duration_cast<chrono::seconds>(chrono::system_clock::now() - timeBegin).count();
    auto simulation_timestamp = current_timestamp();

    for (bsoncxx::document::element ele: res.view())
        value_builder.append(kvp(ele.key(), ele.get_value()));
    value_builder.append(kvp("simulation_time", to_string(simulation_time)));
    value_builder.append(kvp("simulation_timestamp", simulation_timestamp));

    cout << bsoncxx::to_json(value_builder.view()) << endl;

    try {
        mongocxx::client client = mongocxx::client{mongocxx::uri(params["dburl"])};
        auto db = client[mongocxx::uri(params["dburl"]).database()];
        mongocxx::options::replace option;
        db[params["dbcollection"]].replace_one(key_builder.extract(), value_builder.extract(), option.upsert(true));
        return EXIT_SUCCESS;
    } catch (const std::exception &xcp) {
        cerr << "error: db connection failed: " << xcp.what() << std::endl;
        return EXIT_FAILURE;
    }
}