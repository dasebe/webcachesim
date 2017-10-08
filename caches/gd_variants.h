#ifndef GD_VARIANTS_H
#define GD_VARIANTS_H

#include <unordered_map>
#include <map>
#include <queue>
#include "cache.h"
#include "cache_object.h"

typedef std::multimap<long double, CacheObject> ValueMapType;
typedef ValueMapType::iterator ValueMapIteratorType;
typedef std::unordered_map<CacheObject, ValueMapIteratorType> GdCacheMapType;
typedef std::unordered_map<CacheObject, uint64_t> CacheStatsMapType;

/*
  GD: greedy dual eviction (base class)

  [implementation via heap: O(log n) time for each cache miss]
*/
class GreedyDualBase : public Cache
{
protected:
    // the GD current value
    long double _currentL = 0;
    // ordered multi map of GD values, access object id + size
    ValueMapType _valueMap;
    // find objects via unordered_map
    GdCacheMapType _cacheMap;

    virtual long double ageValue(SimpleRequest* req);
    virtual void hit(SimpleRequest* req);

public:
    GreedyDualBase()
        : Cache(),
          _currentL(0)
    {
    }
    virtual ~GreedyDualBase()
    {
    }

    virtual bool lookup(SimpleRequest* req);
    virtual void admit(SimpleRequest* req);
    virtual void evict(SimpleRequest* req);
    virtual void evict();
};

static Factory<GreedyDualBase> factoryGD("GD");

/*
  Greedy Dual Size policy
*/
class GDSCache : public GreedyDualBase
{
protected:
    virtual long double ageValue(SimpleRequest* req);

public:
    GDSCache()
        : GreedyDualBase()
    {
    }
    virtual ~GDSCache()
    {
    }
};

static Factory<GDSCache> factoryGDS("GDS");

/*
  Greedy Dual Size Frequency policy
*/
class GDSFCache : public GreedyDualBase
{
protected:
    CacheStatsMapType _reqsMap;

    virtual long double ageValue(SimpleRequest* req);

public:
    GDSFCache()
        : GreedyDualBase()
    {
    }
    virtual ~GDSFCache()
    {
    }

    virtual bool lookup(SimpleRequest* req);
};

static Factory<GDSFCache> factoryGDSF("GDSF");

/*
  LRU-K policy
*/
typedef std::unordered_map<CacheObject, std::queue<uint64_t>> lrukMapType;

class LRUKCache : public GreedyDualBase
{
protected:
    lrukMapType _refsMap;
    unsigned int _tk;
    uint64_t _curTime;

    virtual long double ageValue(SimpleRequest* req);

public:
    LRUKCache();
    virtual ~LRUKCache()
    {
    }

    virtual void setPar(std::string parName, std::string parValue);
    virtual bool lookup(SimpleRequest* req);
    virtual void evict(SimpleRequest* req);
    virtual void evict();
};

static Factory<LRUKCache> factoryLRUK("LRUK");

/*
  LFUDA
*/
class LFUDACache : public GreedyDualBase
{
protected:
    CacheStatsMapType _reqsMap;

    virtual long double ageValue(SimpleRequest* req);

public:
    LFUDACache()
        : GreedyDualBase()
    {
    }
    virtual ~LFUDACache()
    {
    }

    virtual bool lookup(SimpleRequest* req);
};

static Factory<LFUDACache> factoryLFUDA("LFUDA");

#endif /* GD_VARIANTS_H */
