#ifndef LHD_VARIANTS_H
#define LHD_VARIANTS_H

#include <unordered_map>
#include <list>
#include "cache.h"
//#include "cache_object.h"

namespace cache {
    class Cache;
}


class LHD : public Cache
{
protected:
    cache::Cache* lhdcache;

public:
    LHD();

    virtual void setSize(uint64_t cs);
    virtual bool lookup(SimpleRequest& req);
    virtual void admit(SimpleRequest& req);
    virtual void evict(SimpleRequest& req);
    virtual void evict();
};

static Factory<LHD> factoryLHD2("LHD");

#endif /* LHD_VARIANTS_H */

