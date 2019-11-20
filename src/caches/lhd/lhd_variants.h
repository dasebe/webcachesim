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
    int assoc = 64;
    int admissionSamples = 8;

    LHD();

    void init_with_params(const map<string, string> &params) override {
        //set params
        for (auto& it: params) {
            if (it.first == "assoc") {
                assoc = stoul(it.second);
            } else if (it.first == "admissionSamples") {
                admissionSamples = stoull(it.second);
            } else {
                cerr << "unrecognized parameter: " << it.first << endl;
            }
        }
    }

    void setSize(const uint64_t &cs) override;

    bool lookup(SimpleRequest &req) override;

    void admit(SimpleRequest &req) override;

    void evict();
};

static Factory<LHD> factoryLHD2("LHD");

#endif /* LHD_VARIANTS_H */

