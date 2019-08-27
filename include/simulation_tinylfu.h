//
// Created by zhenyus on 2/25/19.
//

#ifndef WEBCACHESIM_SIMULATION_TINYLFU_H
#define WEBCACHESIM_SIMULATION_TINYLFU_H

#include <map>
#include <string>
#include <bsoncxx/document/view.hpp>
#include <bsoncxx/builder/basic/document.hpp>

/*
 * single thread simulation. Returns results.
 */
bsoncxx::builder::basic::document _simulation_tinylfu(std::string trace_file, std::string cache_type,
                                                      uint64_t cache_size, std::map<std::string, std::string> params);

#endif //WEBCACHESIM_SIMULATION_TINYLFU_H
