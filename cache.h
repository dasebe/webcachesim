#ifndef CACHE_H
#define CACHE_H

#include <unordered_set>
#include <unordered_map>
#include <map>
#include <iostream>
#include <string>
#include <vector>
#include <cstdint>
#include <memory>
#include "request.h"

// uncomment to enable cache debugging:
// #define CDEBUG 1

// util for debug
#ifdef CDEBUG
inline void logMessage(std::string m, double x, double y, double z) {
    std::cerr << m << "," << x << "," << y  << "," << z << "\n";
}
#define LOG(m,x,y,z) logMessage(m,x,y,z)
#else
#define LOG(m,x,y,z)
#endif



class Cache;

class CacheFactory {
public:
    CacheFactory() {}
    virtual std::unique_ptr<Cache> create_unique() = 0;
};

class Cache {
public:
    // create and destroy a cache
    Cache()
        : _cacheSize(0),
          _currentSize(0)
    {
    }
    virtual ~Cache(){};

    // main cache management functions (to be defined by a policy)
    virtual bool lookup(SimpleRequest* req) = 0;
    virtual void admit(SimpleRequest* req) = 0;
    virtual void evict(SimpleRequest* req) = 0;
    virtual void evict() = 0;

    // configure cache parameters
    virtual void setSize(uint64_t cs) {
        _cacheSize = cs;
        while (_currentSize > _cacheSize) {
            evict();
        }
    }
    virtual void setPar(std::string parName, std::string parValue) {}

    uint64_t getCurrentSize() const {
        return(_currentSize);
    }
    uint64_t getSize() const {
        return(_cacheSize);
    }

    // helper functions (factory pattern)
    static void registerType(std::string name, CacheFactory *factory) {
        get_factory_instance()[name] = factory;
    }
    static std::unique_ptr<Cache> create_unique(std::string name) {
        std::unique_ptr<Cache> Cache_instance;
        if(get_factory_instance().count(name) != 1) {
            std::cerr << "unkown cacheType" << std::endl;
            return nullptr;
        }
        Cache_instance = get_factory_instance()[name]->create_unique();
        return Cache_instance;
    }

protected:
    // basic cache properties
    uint64_t _cacheSize; // size of cache in bytes
    uint64_t _currentSize; // total size of objects in cache in bytes

    // helper functions (factory pattern)
    static std::map<std::string, CacheFactory *> &get_factory_instance() {
        static std::map<std::string, CacheFactory *> map_instance;
        return map_instance;
    }
};

template<class T>
class Factory : public CacheFactory {
public:
    Factory(std::string name) { Cache::registerType(name, this); }
    std::unique_ptr<Cache> create_unique() {
        std::unique_ptr<Cache> newT(new T);
        return newT;
    }
};


#endif /* CACHE_H */
