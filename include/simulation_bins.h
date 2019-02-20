//
// Created by zhenyus on 1/13/19.
//

#ifndef WEBCACHESIM_SIMULATION_BINS_H
#define WEBCACHESIM_SIMULATION_BINS_H

#include <map>
#include <string>

/*
 * single thread simulation. Returns results.
 * data contains label of next_t
 */
std::map<std::string, std::string> _simulation_bins(std::string trace_file, std::string cache_type,
                                                   uint64_t cache_size, std::map<std::string, std::string> params);
#endif //WEBCACHESIM_SIMULATION_BINS_H
