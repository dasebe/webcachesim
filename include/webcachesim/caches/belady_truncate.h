//
// Created by zhenyus on 1/15/19.
//

#ifndef WEBCACHESIM_BELADY_TRUNCATE_H
#define WEBCACHESIM_BELADY_TRUNCATE_H

//#include "cache.h"
//#include <utils.h>
//#include <unordered_map>
//#include <map>
//
//using namespace std;
//
///*
//  Belady: Optimal for unit size
//*/
//class BeladyTruncateCache : public Cache
//{
//protected:
//    // list for recency order
//    multimap<uint64_t , uint64_t, greater<uint64_t >> _valueMap;
//    // only store in-cache object, value is size
//    unordered_map<uint64_t, uint64_t> _cacheMap;
//
//public:
//    BeladyTruncateCache()
//            : Cache()
//    {
//    }
//
//    virtual bool lookup(SimpleRequest& req){};
//    virtual void admit(SimpleRequest& req);
//    virtual void evict(SimpleRequest& req){};
//    virtual void evict();
//    uint64_t lookup_truncate(AnnotatedRequest& req);
//};
//
//static Factory<BeladyTruncateCache> factoryBeladyTruncate("BeladyTruncate");
//
#endif //WEBCACHESIM_BELADY_TRUNCATE_H
