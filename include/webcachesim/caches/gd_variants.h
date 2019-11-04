#ifndef GD_VARIANTS_H
#define GD_VARIANTS_H

#include <unordered_map>
#include "utils.h"
#include <map>
#include <queue>
#include "cache.h"

typedef std::multimap<long double, uint64_t> ValueMapType;
typedef ValueMapType::iterator ValueMapIteratorType;
typedef std::unordered_map<uint64_t, ValueMapIteratorType> GdCacheMapType;
typedef std::unordered_map<uint64_t, uint64_t> CacheStatsMapType;

#ifdef EVICTION_LOGGING
#include "mongocxx/client.hpp"
#endif

using namespace std;
using namespace webcachesim;

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
    unordered_map<uint64_t , uint64_t > _sizemap;
#ifdef EVICTION_LOGGING
    uint32_t current_t;
    unordered_map<uint64_t, uint32_t> future_timestamps;
    vector<uint8_t> eviction_qualities;
    vector<uint16_t> eviction_logic_timestamps;
    uint64_t byte_million_req;
    string task_id;
    string dburl;
#endif


    virtual long double ageValue(SimpleRequest& req);
    virtual void hit(SimpleRequest& req);
    bool has(const uint64_t& id) {return _cacheMap.find(id) != _cacheMap.end();}

public:
    GreedyDualBase()
        : Cache(),
          _currentL(0)
    {
    }
    virtual ~GreedyDualBase()
    {
    }

    bool lookup(SimpleRequest &req) override;

    void admit(SimpleRequest &req) override;

    void evict(SimpleRequest &req);

    void evict();
};

static Factory<GreedyDualBase> factoryGD("GD");
//
///*
//  Greedy Dual Size policy
//*/
//class GDSCache : public GreedyDualBase
//{
//protected:
//    virtual long double ageValue(SimpleRequest* req);
//
//public:
//    GDSCache()
//        : GreedyDualBase()
//    {
//    }
//    virtual ~GDSCache()
//    {
//    }
//};
//
//static Factory<GDSCache> factoryGDS("GDS");

/*
  Greedy Dual Size Frequency policy
*/
class GDSFCache : public GreedyDualBase
{
protected:
    CacheStatsMapType _reqsMap;

    virtual long double ageValue(SimpleRequest& req);

public:
    GDSFCache()
        : GreedyDualBase()
    {
    }
    virtual ~GDSFCache()
    {
    }

    virtual bool lookup(SimpleRequest& req);
};

static Factory<GDSFCache> factoryGDSF("GDSF");

/*
  LRU-K policy
*/
typedef std::unordered_map<uint64_t , std::queue<uint64_t>> lrukMapType;

class LRUKCache : public GreedyDualBase
{
protected:
    lrukMapType _refsMap;
    unsigned int _tk;
    uint64_t _curTime;

    virtual long double ageValue(SimpleRequest& req);

public:
    LRUKCache();
    virtual ~LRUKCache()
    {
    }

    void init_with_params(const map<string, string> &params) override {
        //set params
        for (auto& it: params) {
            if (it.first == "k") {
                _tk = stoul(it.second);
#ifdef EVICTION_LOGGING
            } else if (it.first == "byte_million_req") {
                byte_million_req = stoull(it.second);
            } else if (it.first == "task_id") {
                task_id = it.second;
            } else if (it.first == "dburl") {
                dburl = it.second;
#endif
            } else {
                cerr << "unrecognized parameter: " << it.first << endl;
            }
        }
    }


#ifdef EVICTION_LOGGING
    void update_stat(bsoncxx::v_noabi::builder::basic::document &doc) override {
        //Log to GridFs because the value is too big to store in mongodb
        try {
            mongocxx::client client = mongocxx::client{mongocxx::uri(dburl)};
            mongocxx::database db = client["webcachesim"];
            auto bucket = db.gridfs_bucket();

            auto uploader = bucket.open_upload_stream(task_id + ".evictions");
            for (auto &b: eviction_qualities)
                uploader.write((uint8_t *) (&b), sizeof(uint8_t));
            uploader.close();
            uploader = bucket.open_upload_stream(task_id + ".eviction_timestamps");
            for (auto &b: eviction_logic_timestamps)
                uploader.write((uint8_t *) (&b), sizeof(uint16_t));
            uploader.close();
        } catch (const std::exception &xcp) {
            cerr << "error: db connection failed: " << xcp.what() << std::endl;
            abort();
        }
    }
#endif

    bool lookup(SimpleRequest &req) override;

    void evict(SimpleRequest &req);

    void evict();
};

static Factory<LRUKCache> factoryLRUK("LRUK");

/*
  LFUDA
*/
class LFUDACache : public GreedyDualBase
{
protected:
    CacheStatsMapType _reqsMap;

    virtual long double ageValue(SimpleRequest& req);

public:
    LFUDACache()
        : GreedyDualBase()
    {
    }
    virtual ~LFUDACache()
    {
    }

#ifdef EVICTION_LOGGING
    void init_with_params(map<string, string> params) override {
        //set params
        for (auto &it: params) {
            if (it.first == "byte_million_req") {
                byte_million_req = stoull(it.second);
            } else if (it.first == "task_id") {
                task_id = it.second;
            } else if (it.first == "dburl") {
                dburl = it.second;
            } else {
                cerr << "unrecognized parameter: " << it.first << endl;
            }
        }
    }
#endif


#ifdef EVICTION_LOGGING
    void update_stat(bsoncxx::builder::basic::document &doc) override {
        //Log to GridFs because the value is too big to store in mongodb
        try {
            mongocxx::client client = mongocxx::client{mongocxx::uri(dburl)};
            mongocxx::database db = client["webcachesim"];
            auto bucket = db.gridfs_bucket();

            auto uploader = bucket.open_upload_stream(task_id + ".evictions");
            for (auto &b: eviction_qualities)
                uploader.write((uint8_t *) (&b), sizeof(uint8_t));
            uploader.close();
            uploader = bucket.open_upload_stream(task_id + ".eviction_timestamps");
            for (auto &b: eviction_logic_timestamps)
                uploader.write((uint8_t *) (&b), sizeof(uint16_t));
            uploader.close();
        } catch (const std::exception &xcp) {
            cerr << "error: db connection failed: " << xcp.what() << std::endl;
            abort();
        }

    }
#endif

    virtual bool lookup(SimpleRequest& req);
};

static Factory<LFUDACache> factoryLFUDA("LFUDA");


/*
  LFU
*/
class LFUCache : public GreedyDualBase
{
protected:
    CacheStatsMapType _reqsMap;

    virtual long double ageValue(SimpleRequest& req);

public:
    LFUCache()
        : GreedyDualBase()
    {
    }
    virtual ~LFUCache()
    {
    }

    virtual bool lookup(SimpleRequest& req);
};

static Factory<LFUCache> factoryLFU("LFU");
#endif /* GD_VARIANTS_H */
