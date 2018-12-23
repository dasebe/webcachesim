//
// Created by zhenyus on 11/14/18.
//

#ifndef WEBCACHESIM_UTILS_H
#define WEBCACHESIM_UTILS_H

// hash_combine derived from boost/functional/hash/hash.hpp:212
// Copyright 2005-2014 Daniel James.
// Distributed under the Boost Software License, Version 1.0.
// (See http://www.boost.org/LICENSE_1_0.txt)
template <class T>
inline void hash_combine(std::size_t & seed, const T & v)
{
    std::hash<T> hasher;
    seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

// from boost hash combine: hashing of pairs for unordered_maps
namespace std {
    template<typename S, typename T>
    struct hash<pair<S, T>> {
        inline size_t operator()(const pair<S, T> &v) const {
            size_t seed = 0;
            hash_combine(seed, v.first);
            hash_combine(seed, v.second);
            return seed;
        }
    };
}

#define update_metric_req(byte_metric, object_metric, size) \
    {byte_metric += size; ++object_metric;}

//#define LOG_SAMPLE_RATE 0.01

#endif //WEBCACHESIM_UTILS_H
