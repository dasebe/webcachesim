//
// Created by zhenyus on 10/31/19.
//

#include "api.h"
#include "parallel_cache.h"

#include <utility>

using namespace webcachesim;

Interface::Interface(string cache_type, int cache_size, int memory_window) {
    pimpl = dynamic_cast<ParallelCache *>(Cache::create_unique(std::move(cache_type)).get());
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
