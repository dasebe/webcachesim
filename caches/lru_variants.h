#ifndef LRU_VARIANTS_H
#define LRU_VARIANTS_H

#include <unordered_map>
#include <list>
#include <random> // AdaptSize random number generation 
#include <ctime> // AdaptSize random number generation; Seed the generator 
#include "cache.h"
#include "cache_object.h"
#include "adaptsize_const.h" /** AdaptSize implementation */


typedef std::list<CacheObject>::iterator ListIteratorType;
typedef std::unordered_map<CacheObject, ListIteratorType> lruCacheMapType;

/*
  LRU: Least Recently Used eviction
*/
class LRUCache : public Cache
{
protected:
    // list for recency order
    // std::list is a container, usually, implemented as a doubly-linked list 
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

    virtual bool lookup(SimpleRequest* req);
    virtual void admit(SimpleRequest* req);
    virtual void evict(SimpleRequest* req);
    virtual void evict();
    virtual SimpleRequest* evict_return();
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

/*
  FilterCache (admit only after N requests)
*/
class FilterCache : public LRUCache
{
protected:
    uint64_t _nParam;
    std::unordered_map<CacheObject, uint64_t> _filter;

public:
    FilterCache();
    virtual ~FilterCache()
    {
    }

    virtual void setPar(std::string parName, std::string parValue);
    virtual bool lookup(SimpleRequest* req);
    virtual void admit(SimpleRequest* req);
};

static Factory<FilterCache> factoryFilter("Filter");

/*
  ThLRU: LRU eviction with a size admission threshold
*/
class ThLRUCache : public LRUCache
{
protected:
    uint64_t _sizeThreshold;

public:
    ThLRUCache();
    virtual ~ThLRUCache()
    {
    }

    virtual void setPar(std::string parName, std::string parValue);
    virtual void admit(SimpleRequest* req);
};

static Factory<ThLRUCache> factoryThLRU("ThLRU");

/*
  ExpLRU: LRU eviction with size-aware probabilistic cache admission
*/
class ExpLRUCache : public LRUCache
{
protected:
    double _cParam;

public:
    ExpLRUCache();
    virtual ~ExpLRUCache()
    {
    }

    virtual void setPar(std::string parName, std::string parValue);
    virtual void admit(SimpleRequest* req);
};

static Factory<ExpLRUCache> factoryExpLRU("ExpLRU");

class AdaptSizeCache : public LRUCache
{
public: 
	AdaptSizeCache();
	virtual ~AdaptSizeCache()
	{
	}

	virtual bool lookup(SimpleRequest*);
	virtual void admit(SimpleRequest*);

private: 
	uint64_t nextReconfiguration;
	double c;
	// cacheSize abolished. To be replaced by what webcachesim provides 
	// (i.e., the command line input) 
	// double cacheSize;
	uint64_t statSize;
	double v; // declared as global variable in adaptsize_stub.cpp 
	// for random number generation 
	const unsigned int SEED = 1534262824; // const seed for repeated results
	std::mt19937_64 randGenerator0 = std::mt19937_64(SEED);
	// for random number generation 
	std::uniform_real_distribution<double> uniform_real_distribution0 = 
		std::uniform_real_distribution<double>(0.0, 1.0); 

	struct ObjInfo {
		uint64_t requestCount; // requestRate in adaptsize_stub.h
		uint64_t objSize;

		ObjInfo() : requestCount(0), objSize(0) { }
	};
	std::unordered_map<CacheObject, ObjInfo> lruCacheMapType;

	std::unordered_map<CacheObject, ObjInfo> ewmaInfo;
	std::unordered_map<CacheObject, ObjInfo> intervalInfo;

	void reconfigure();
	double modelHitRate(double c);

	// align data for vectorization
	std::vector<double> alignedReqCount;
	std::vector<double> alignedObjSize;
	std::vector<double> alignedAdmProb;
};

static Factory<AdaptSizeCache> factoryAdaptSize("AdaptSize");

/*
  S4LRU

  enter at segment 0
  if hit on segment i, segment i+1
  if evicted on segment i, segment i-1

*/
class S4LRUCache : public Cache
{
protected:
    LRUCache segments[4];

public:
    S4LRUCache()
        : Cache()
    {
        segments[0] = LRUCache();
        segments[1] = LRUCache();
        segments[2] = LRUCache();
        segments[3] = LRUCache();
    }
    virtual ~S4LRUCache()
    {
    }

    virtual void setSize(uint64_t cs);
    virtual bool lookup(SimpleRequest* req);
    virtual void admit(SimpleRequest* req);
    virtual void segment_admit(uint8_t idx, SimpleRequest* req);
    virtual void evict(SimpleRequest* req);
    virtual void evict();
};

static Factory<S4LRUCache> factoryS4LRU("S4LRU");



#endif
