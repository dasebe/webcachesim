//
// Created by zhenyus on 10/31/19.
//

#include "api.h"
#include "parallel_cache.h"

#include <utility>

using namespace webcachesim;

Interface::Interface(const string &cache_type, const int &cache_size, const int &memory_window) {
    string webcachesim_cache_type;
    if (cache_type == "LRU") {
        webcachesim_cache_type = "ParallelLRU";
    } else if (cache_type == "FIFO") {
        webcachesim_cache_type = "ParallelFIFO";
    } else {
        cerr << "Vdisk Algorithm not implemented";
        exit(-1);
    }
    pimpl = dynamic_cast<ParallelCache *>(Cache::create_unique(webcachesim_cache_type).release());
    pimpl->setSize(cache_size);
    map<string, string> params;
    params["memory_window"] = to_string(memory_window);
    pimpl->init_with_params(params);
}

//allow concurrent access
void Interface::admit(const uint64_t &key, const int64_t &size, const uint16_t extra_features[max_n_extra_feature]) {
    pimpl->parallel_admit(key, size, extra_features);
}

//allow concurrent access
uint64_t Interface::lookup(const uint64_t &key) {
    return pimpl->parallel_lookup(key);
}
