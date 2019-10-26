//
// Created by zhenyus on 8/29/19.
//

#ifndef WEBCACHESIM_BLOOM_FILTER_H
#define WEBCACHESIM_BLOOM_FILTER_H

#include <cstdint>
#include <unordered_set>

using namespace std;

class BloomFilter {
public:
    uint8_t current_filter = 0;
    unordered_set<uint64_t> *_filters;
    static const size_t max_n_element = 40000000;

    BloomFilter() {
        _filters = new unordered_set<uint64_t>[2];
        for (int i = 0; i < 2; ++i)
            _filters[i].reserve(max_n_element);
    }

    inline bool exist(uint64_t &key) {
        return (_filters[0].count(key)) || (_filters[1].count(key));
    }

    inline bool exist_or_insert(uint64_t &key) {
        if (exist(key))
            return true;
        else
            insert(key);
        return false;
    }

    void insert(uint64_t &key) {
        if (_filters[current_filter].size() > max_n_element) {
            //if accumulate more than 40 million, switch
            if (!_filters[1 - current_filter].empty())
                _filters[1 - current_filter].clear();
            current_filter = 1 - current_filter;
        }
        _filters[current_filter].insert(key);
    }
};


#endif //WEBCACHESIM_BLOOM_FILTER_H
