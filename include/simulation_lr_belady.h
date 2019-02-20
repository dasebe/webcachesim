//
// Created by zhenyus on 12/17/18.
//

#ifndef WEBCACHESIM_SIMULATION_LR_BELADY_H
#define WEBCACHESIM_SIMULATION_LR_BELADY_H


#include <map>
#include <string>

/*
 * single thread simulation. Returns results.
 */
std::map<std::string, std::string> _simulation_lr_belady(std::string trace_file, std::string cache_type,
                                              uint64_t cache_size, std::map<std::string, std::string> params);

#endif //WEBCACHESIM_SIMULATION_LR_BELADY_H
