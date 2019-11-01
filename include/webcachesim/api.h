//
// Created by zhenyus on 10/31/19.
//

#ifndef WEBCACHESIM_API_H
#define WEBCACHESIM_API_H


#include <memory>

namespace webcachesim {

    class Cache;

    class Interface {
    public:
        Interface(std::string cache_type, int cache_size, int memory_window);

        ~Interface() = default;

        void admit(const uint64_t &key, const int64_t &size);

        uint64_t lookup(const uint64_t &key);

    private:
        std::unique_ptr<Cache> pimpl;
    };
}

#endif //WEBCACHESIM_API_H
