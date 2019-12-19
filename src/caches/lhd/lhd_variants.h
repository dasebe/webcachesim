#ifndef LHD_VARIANTS_H
#define LHD_VARIANTS_H

#include <unordered_map>
#include <list>
#include "cache.h"
//#include "cache_object.h"

namespace cache {
    class Cache;
}


using namespace webcachesim;
class LHD : public Cache {
protected:
    cache::Cache *lhdcache;

public:
    LHD();

    void setSize(const uint64_t &cs) override;

    bool lookup(SimpleRequest &req) override;

    void admit(SimpleRequest &req) override;

    void evict();
};

static Factory<LHD> factoryLHD2("LHD");

#endif /* LHD_VARIANTS_H */

