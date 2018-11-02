#ifndef LRU_VARIANTS_H
#define LRU_VARIANTS_H

#include <unordered_map>
#include <list>
#include "cache.h"
#include "cache_object.h"
#include <boost/bimap.hpp>
#include <boost/bimap/multiset_of.hpp>

typedef std::list<CacheObject>::iterator ListIteratorType;
typedef std::unordered_map<CacheObject, ListIteratorType> lruCacheMapType;

/*
  LRU: Least Recently Used eviction
*/
class LRUCache : public Cache
{
protected:
    // list for recency order
    std::list<CacheObject> _cacheList;
    // map to find objects in list
    lruCacheMapType _cacheMap;

    virtual void hit(lruCacheMapType::const_iterator it, uint64_t size);

public:
    LRUCache()
        : Cache()
    {
    }
    virtual ~LRUCache()
    {
    }

    virtual bool lookup(SimpleRequest& req);
    virtual void admit(SimpleRequest& req);
    virtual void evict(SimpleRequest& req);
    virtual void evict();
};

static Factory<LRUCache> factoryLRU("LRU");


/*
  FIFO: First-In First-Out eviction
*/
class FIFOCache : public LRUCache
{
protected:
    virtual void hit(lruCacheMapType::const_iterator it, uint64_t size);

public:
    FIFOCache()
        : LRUCache()
    {
    }
    virtual ~FIFOCache()
    {
    }
};

static Factory<FIFOCache> factoryFIFO("FIFO");

///*
//  FilterCache (admit only after N requests)
//*/
//class FilterCache : public LRUCache
//{
//protected:
//    uint64_t _nParam;
//    std::unordered_map<CacheObject, uint64_t> _filter;
//
//public:
//    FilterCache();
//    virtual ~FilterCache()
//    {
//    }
//
//    virtual void setPar(std::string parName, std::string parValue);
//    virtual bool lookup(SimpleRequest* req);
//    virtual void admit(SimpleRequest* req);
//};
//
//static Factory<FilterCache> factoryFilter("Filter");
//
///*
//  ThLRU: LRU eviction with a size admission threshold
//*/
//class ThLRUCache : public LRUCache
//{
//protected:
//    uint64_t _sizeThreshold;
//
//public:
//    ThLRUCache();
//    virtual ~ThLRUCache()
//    {
//    }
//
//    virtual void setPar(std::string parName, std::string parValue);
//    virtual void admit(SimpleRequest* req);
//};
//
//static Factory<ThLRUCache> factoryThLRU("ThLRU");
//
///*
//  ExpLRU: LRU eviction with size-aware probabilistic cache admission
//*/
//class ExpLRUCache : public LRUCache
//{
//protected:
//    double _cParam;
//
//public:
//    ExpLRUCache();
//    virtual ~ExpLRUCache()
//    {
//    }
//
//    virtual void setPar(std::string parName, std::string parValue);
//    virtual void admit(SimpleRequest* req);
//};
//
//static Factory<ExpLRUCache> factoryExpLRU("ExpLRU");


typedef std::pair<std::uint64_t, std::uint64_t> KeyT;
typedef boost::bimap<boost::bimaps::set_of<KeyT>,
        boost::bimaps::multiset_of<uint64_t, std::greater<uint64_t>>> Bimap;

typedef Bimap::left_map::const_iterator left_const_iterator;
typedef Bimap::right_map::const_iterator right_const_iterator;

typedef Bimap::left_map::const_iterator left_iterator;
typedef Bimap::right_map::const_iterator right_iterator;

/*
  Belady: Optimal for unit size
*/
class BeladyCache : public Cache
{
protected:
    // list for recency order
    Bimap _cacheMap;

public:
    BeladyCache()
            : Cache()
    {
    }
    virtual ~BeladyCache()
    {
    }

    virtual bool lookup(SimpleRequest& req);
    virtual void admit(SimpleRequest& req);
    virtual void evict(SimpleRequest& req) {
        //no need to use it
    };
    virtual void evict();
};

static Factory<BeladyCache> factoryBelady("Belady");

#endif
