//
// Created by Zhenyu Song on 10/30/18.
//

#ifndef WEBCACHESIM_SIMULATION_H
#define WEBCACHESIM_SIMULATION_H

#include <map>
#include <string>

/*
 * single thread simulation. Returns results.
 */
std::map<std::string, std::string> simulation(std::string trace_file, std::string cache_type,
        uint64_t cache_size, std::map<std::string, std::string> params);

#endif //WEBCACHESIM_SIMULATION_H
