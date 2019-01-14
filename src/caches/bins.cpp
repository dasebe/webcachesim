//
// Created by zhenyus on 1/13/19.
//

#include "bins.h"
#include <algorithm>
#include <fstream>
#include "utils.h"

using namespace std;

//init with a wrong value
uint8_t BinsMeta::_max_n_past_timestamps = 0;

void BinsCache::set_future_expections(const uint64_t &t) {
    ifstream infile("/home/zhenyus/webcachesim/bins_weight"+to_string(t/threshold)+".csv");
//    ifstream infile("/home/zhenyus/webcachesim/bins_weight1.csv");
    if (!infile) {
        cerr << "cannot open bins weight" << endl;
        exit(-2);
    }

    uint64_t i0, d1, d2, d3;
    while(infile>>i0>>d1>>d2>>d3) {
        for (uint i = 0; i < n_window_bins; ++i) {
            uint64_t idx = ((((((i0 * (n_window_bins + 1) + d1) * (n_window_bins + 1)) + d2) * (n_window_bins + 1)) + d3)
                           * (n_window_bins))+i;
            double e;
            infile >> e;
            future_expections[idx] = e;
        }
    }
}


bool BinsCache::lookup(SimpleRequest &_req) {
    auto & req = dynamic_cast<AnnotatedRequest &>(_req);
    static uint64_t i = 0;
    ++i;
    //todo: load expectations

    auto it = key_map.find(req._id);
    if (it != key_map.end()) {
        //update past timestamps
        bool & list_idx = it->second.first;
        uint32_t & pos_idx = it->second.second;
        meta_holder[list_idx][pos_idx].update(req._t);
        return !list_idx;
    }
    return false;
}

void BinsCache::admit(SimpleRequest &_req) {
    AnnotatedRequest & req = static_cast<AnnotatedRequest &>(_req);
    const uint64_t & size = req._size;
    // object feasible to store?
    if (size > _cacheSize) {
        LOG("L", _cacheSize, req.get_id(), size);
        return;
    }

    auto it = key_map.find(req._id);
    if (it == key_map.end()) {
        //fresh insert
        key_map.insert({req._id, {0, (uint32_t) meta_holder[0].size()}});
        meta_holder[0].emplace_back(req._id, req._size, req._t);
        _currentSize += size;
        if (_currentSize <= _cacheSize)
            return;
    } else if (size + _currentSize <= _cacheSize){
        //bring list 1 to list 0
        //first move meta data, then modify hash table
        uint32_t tail0_pos = meta_holder[0].size();
        meta_holder[0].emplace_back(meta_holder[1][it->second.second]);
        uint32_t tail1_pos = meta_holder[1].size()-1;
        if (it->second.second !=  tail1_pos) {
            //swap tail
            meta_holder[1][it->second.second] = meta_holder[1][tail1_pos];
            key_map.find(meta_holder[1][tail1_pos]._key)->second.second = it->second.second;
        }
        meta_holder[1].pop_back();
        it->second = {0, tail0_pos};
        _currentSize += size;
        return;
    } else {
        //insert-evict
        auto epair = rank(req._t);
        auto & key0 = epair.first;
        auto & pos0 = epair.second;
        auto & pos1 = it->second.second;
        _currentSize = _currentSize - meta_holder[0][pos0]._size + req._size;
        swap(meta_holder[0][pos0], meta_holder[1][pos1]);
        swap(it->second, key_map.find(key0)->second);
    }
    // check more eviction needed?
    while (_currentSize > _cacheSize) {
        evict(req._t);
    }
}


pair<uint64_t, uint32_t> BinsCache::rank(const uint64_t & t) {
    double max_expectation;
    uint64_t max_key;
    uint32_t max_pos;
    uint64_t min_past_timestamp;

    uint32_t rand_idx = _distribution(_generator) % meta_holder[0].size();
    uint n_sample;
    if (sample_rate < meta_holder[0].size())
        n_sample = sample_rate;
    else
        n_sample = meta_holder[0].size();

    for (uint32_t i = 0; i < n_sample; i++) {
        uint32_t pos = (i+rand_idx)%meta_holder[0].size();
        auto & meta = meta_holder[0][pos];
        //fill in past_interval
        uint8_t j = 0;
        uint64_t cidx = 0;
        if ((t - meta._past_timestamp)/bin_width >= n_window_bins) {
            cidx = (n_window_bins - 1);
        } else {
            cidx = (t - meta._past_timestamp)/bin_width;
        }
        
        uint64_t this_past_timestamp = meta._past_timestamp;
        for (j = 0; j < meta._past_distance_idx && j < max_n_past_intervals-1; ++j) {
            uint8_t past_distance_idx = (meta._past_distance_idx - 1 - j) % max_n_past_intervals;
            uint64_t & past_distance = meta._past_distances[past_distance_idx];
            this_past_timestamp -= past_distance;
            if (this_past_timestamp <= t - threshold)
                cidx = cidx * (n_window_bins+1) + n_window_bins;
            else
                cidx = cidx * (n_window_bins+1) + past_distance/bin_width;
        }
        for (; j < max_n_past_intervals - 1; j++)
            cidx = cidx * (n_window_bins+1) + n_window_bins;

        double expectation_hits = 0;
        for (uint j = 0; j < n_window_bins; ++j) {
            expectation_hits += future_expections[(cidx * n_window_bins) + j] * e_weights[j];
        }

        if (!i || (expectation_hits > max_expectation) ||
                (expectation_hits == max_expectation && (meta._past_timestamp < min_past_timestamp))) {
            max_expectation = expectation_hits;
            max_key = meta._key;
            max_pos = pos;
            min_past_timestamp = meta._past_timestamp;
        }
    }

    return {max_key, max_pos};
}

void BinsCache::evict(const uint64_t & t) {
    auto epair = rank(t);
    uint64_t & key = epair.first;
    uint32_t & old_pos = epair.second;

    //bring list 0 to list 1
    uint32_t new_pos = meta_holder[1].size();

    meta_holder[1].emplace_back(meta_holder[0][old_pos]);
    uint32_t activate_tail_idx = meta_holder[0].size()-1;
    if (old_pos !=  activate_tail_idx) {
        //move tail
        meta_holder[0][old_pos] = meta_holder[0][activate_tail_idx];
        key_map.find(meta_holder[0][activate_tail_idx]._key)->second.second = old_pos;
    }
    meta_holder[0].pop_back();

    auto it = key_map.find(key);
    it->second.first = 1;
    it->second.second = new_pos;
    _currentSize -= meta_holder[1][new_pos]._size;
}
