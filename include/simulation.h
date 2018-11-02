//
// Created by Zhenyu Song on 10/30/18.
//

#ifndef WEBCACHESIM_SIMULATION_H
#define WEBCACHESIM_SIMULATION_H

#include <map>
#include <string>

using namespace std;

/*
 * single thread simulation. Returns results.
 */
map<string, string> simulation(string trace_file, string cache_type, uint64_t cache_size, map<string, string> params);


#endif //WEBCACHESIM_SIMULATION_H
