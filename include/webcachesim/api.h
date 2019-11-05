//
// Created by zhenyus on 10/31/19.
//

#ifndef WEBCACHESIM_API_H
#define WEBCACHESIM_API_H

#include <memory>

namespace webcachesim {
    const uint max_n_extra_feature = 4;
    class ParallelCache;

    class Interface {
    public:
        Interface(const std::string &cache_type, const int &cache_size, const int &memory_window);

        ~Interface() = default;

        void admit(const uint64_t &key, const int64_t &size, const uint16_t extra_features[max_n_extra_feature]);

        uint64_t lookup(const uint64_t &key);

    private:
        ParallelCache *pimpl;
    };
}

#endif //WEBCACHESIM_API_H
