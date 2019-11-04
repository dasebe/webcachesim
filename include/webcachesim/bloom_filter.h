//
// Created by zhenyus on 8/29/19.
//

#ifndef WEBCACHESIM_BLOOM_FILTER_H
#define WEBCACHESIM_BLOOM_FILTER_H

#include <bf/bloom_filter/basic.hpp>

using namespace std;

class AkamaiBloomFilter {
/*
 * From Algorithm Nugget @ Akamai Paper
 */
public:

    AkamaiBloomFilter() {
        for (auto &_filter : _filters)
            _filter = make_unique<bf::basic_bloom_filter>(fp_rate, max_n_element);
    }

    inline bool exist(const uint64_t &key) {
        return (_filters[0]->lookup(key)) || (_filters[1]->lookup(key));
    }

    inline bool exist_or_insert(const uint64_t &key) {
        ++n_seen_req;
        if (exist(key))
            return true;
        else
            insert(key);
        return false;
    }

    void insert(const uint64_t &key) {
        if (n_seen_req > switch_interval) {
            _filters[1 - current_filter]->clear();
            current_filter = 1 - current_filter;
            n_seen_req = 0;
        }
        _filters[current_filter]->add(key);
    }

private:
    const size_t max_n_element = 40000000;
    constexpr static const double fp_rate = 0.001;
    //8 is a magic number
    const size_t switch_interval = 8 * max_n_element;
    uint8_t current_filter = 0;
    int n_seen_req = 0;
    unique_ptr<bf::basic_bloom_filter> _filters[2];
};


#endif //WEBCACHESIM_BLOOM_FILTER_H
