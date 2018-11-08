//
// Created by zhenyus on 11/8/18.
//

#ifndef WEBCACHESIM_SIMULATION_LFO_H
#define WEBCACHESIM_SIMULATION_LFO_H


#include <map>
#include <string>

/*
 * single thread simulation. Returns results.
 */
std::map<std::string, std::string> _simulation_lfo(std::string trace_file, std::string cache_type,
                                              uint64_t cache_size, std::map<std::string, std::string> params);

#endif //WEBCACHESIM_SIMULATION_LFO_H
