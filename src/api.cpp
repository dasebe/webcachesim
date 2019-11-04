//
// Created by zhenyus on 10/31/19.
//

#include "api.h"
#include "parallel_cache.h"

#include <utility>

using namespace webcachesim;

Interface::Interface(string cache_type, int cache_size, int memory_window) {
    //TODO: use string name
    pimpl = dynamic_cast<ParallelCache *>(Cache::create_unique(std::move(cache_type)).get());
}


void Interface::admit(const uint64_t &key, const int64_t &size) {

}

uint64_t Interface::lookup(const uint64_t &key) {
    return 0;
}
