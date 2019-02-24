#pragma once

#include <math.h>
#include "ranked_age.hpp"

/* forward declaration */
namespace cache_competitors {
  class Cache;
}

namespace repl_competitors {
  namespace fn_competitors {
    class HyperbolicSizeAware{
    public:
      HyperbolicSizeAware(cache_competitors::Cache* _cache)
        : cache(_cache) 
        , accesses(0.)
        , entryTime(-1ull)
        , now(0) 
        , lastEvictionPriority(0.) {}
      ~HyperbolicSizeAware() {}

      typedef float rank_t;

      bool isPresent(candidate_t id) {
        return entryTime[id] != entryTime.DEFAULT;
      }

      rank_t rank(candidate_t id) {
        uint64_t timeSinceEntry = now - entryTime[id];
        assert(timeSinceEntry > 0);
        
        uint64_t size = cache->getSize(id);

        return - (COST * accesses[id]) / (timeSinceEntry * size);
      }

      void update(candidate_t id, const parser_competitors::Request& req) {
        if (isPresent(id)) {
          /* hit */
          accesses[id] += 1;
        }
        else {
          /* fill */
          entryTime[id] = now;
          float initCount = LEEWAY * 1. + (1. - LEEWAY) * lastEvictionPriority;
          assert(initCount > 0.);
          accesses[id] = initCount;
        }

        now++;
      }

      void replaced(candidate_t id) {
        lastEvictionPriority = -rank(id);
        accesses.erase(id);
        entryTime.erase(id);
      }

      void dumpStats() { }
      
    private:
      static constexpr rank_t COST = 1; // cost 1 is to maximize hit rate
      static constexpr float  LEEWAY = 0.1; // init access count from the last evicted rank

      cache_competitors::Cache* cache;

      CandidateMap<float> accesses;
      CandidateMap<uint64_t> entryTime;
      uint64_t now;
      rank_t lastEvictionPriority;

    }; // class HyperbolicSizeAware
  } // namespace fn

  typedef RankedPolicy<fn_competitors::HyperbolicSizeAware> RankedHyperbolicSizeAware;

} // namespace repl
