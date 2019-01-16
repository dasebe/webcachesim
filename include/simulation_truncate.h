//
// Created by zhenyus on 1/15/19.
//

#ifndef WEBCACHESIM_SIMULATION_TRUNCATE_H
#define WEBCACHESIM_SIMULATION_TRUNCATE_H
#include <map>
#include <string>

/*
 * single thread simulation. Returns results.
 */
std::map<std::string, std::string> _simulation_truncate(std::string trace_file, std::string cache_type,
                                              uint64_t cache_size, std::map<std::string, std::string> params);

#endif //WEBCACHESIM_SIMULATION_TRUNCATE_H
