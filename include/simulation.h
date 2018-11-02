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
map<string, double> simulation(string trace_file, string cache_type, uint64_t cache_size, map<string, double> params);


#endif //WEBCACHESIM_SIMULATION_H
