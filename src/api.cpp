//
// Created by zhenyus on 10/31/19.
//

#include "api.h"
#include "cache.h"

#include <utility>

Interface::Interface(string cache_type, int cache_size, int memory_window) {
    //TODO: use string name
    pimpl = move(Cache::create_unique(std::move(cache_type)));
}


void Interface::admit(const uint64_t &key, const int64_t &size) {

}

uint64_t Interface::lookup(const uint64_t &key) {
    return 0;
}
