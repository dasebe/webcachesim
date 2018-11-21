#pragma once

#include <vector>
#include "lru.hpp"
#include "rand.hpp"

namespace repl {

  // Policy described in Berger et al, NSDI'17
  class AdaptSize : public Policy {
  public:

    AdaptSize(uint64_t _cacheSize);
    ~AdaptSize();

    void update(candidate_t id, const parser::Request& req);
    void replaced(candidate_t id);
    candidate_t rank(const parser::Request& req);

    void dumpStats(cache::Cache* cache);

    //added for compatibility with webcachesim
    virtual void setSize(uint64_t cs);

  private:
    uint64_t nextReconfiguration;
    double c;
    misc::Rand rand;
    double cacheSize;
    uint64_t statSize;

    struct ObjInfo {
      double requestRate;
      int64_t size;

      ObjInfo() : requestRate(0.), size(0) { }
    };

    std::unordered_map<candidate_t, ObjInfo> ewmaInfo;
    std::unordered_map<candidate_t, ObjInfo> intervalInfo;

    void reconfigure();
    bool admit(int64_t size);
    double modelHitRate(double c);

    // align data for vectorization
    std::vector<double> alignedReqRate;
    std::vector<double> alignedObjSize;
    std::vector<double> alignedAdmProb;

    // see lru.hpp
    List<candidate_t> list;
    Tags<candidate_t> tags;
    typedef typename List<candidate_t>::Entry Entry;
  };

}
