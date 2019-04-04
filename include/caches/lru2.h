//
// Created by zhenyus on 4/4/19.
//

#ifndef WEBCACHESIM_LRU2_H
#define WEBCACHESIM_LRU2_H

#include <unordered_map>
#include <list>
#include <cstddef>
#include <stdexcept>
#include "cache.h"

typedef uint64_t KeyT;
typedef uint64_t ValueT;

class LRU2Cache: public Cache {
public:
    typedef typename std::pair<KeyT, ValueT> key_value_pair_t;
    typedef typename std::list<key_value_pair_t>::iterator list_iterator_t;

    void admit(SimpleRequest & req) override {
        auto & key = req._id;
        auto & size = req._size;
        //don't admit larger than cache size
        if (size > _cacheSize)
            return;

        auto it = _cache_items_map.find(key);
        _cache_items_list.push_front(key_value_pair_t(key, 1));
        if (it != _cache_items_map.end()) {
            _cache_items_list.erase(it->second);
            _cache_items_map.erase(it);
        }
        _cache_items_map[key] = _cache_items_list.begin();
        _size_map[key] = size;
        _currentSize += size;

        while (_currentSize > _cacheSize) {
            auto last = _cache_items_list.end();
            last--;
            auto & e_key = last->first;
            _currentSize -= _size_map[e_key];
            _size_map.erase(e_key);
            _cache_items_map.erase(e_key);
            _cache_items_list.pop_back();
        }
    }

    bool lookup(SimpleRequest& req) override {
        auto & key = req._id;
        auto it = _cache_items_map.find(key);
        if (it == _cache_items_map.end()) {
            return false;
        } else {
            _cache_items_list.splice(_cache_items_list.begin(), _cache_items_list, it->second);
            return true;
        }
    }

    void evict(SimpleRequest& req) override {};
    void evict() override {};

private:
    std::list<key_value_pair_t> _cache_items_list;
    std::unordered_map<KeyT, list_iterator_t> _cache_items_map;
    std::unordered_map<KeyT, uint64_t > _size_map;
};
static Factory<LRU2Cache> factoryLRU2("LRU2");

#endif //WEBCACHESIM_LRU2_H
