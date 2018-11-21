#pragma once

#include <algorithm>
#include <vector>
#include <list>

#include "ranked_age.hpp"
#include "constants.hpp"

namespace cache {
  class Cache;
}

namespace repl {

  namespace fn {

    class HitDensity;

    namespace hitdensity {

      typedef float rank_t;
      typedef uint32_t counter_t;

    // This struct holds the information for a single class of objects
    // (e.g., those that have been referenced twice).
    //
    // During steady-state operation, HitDensity uses lookaheadHits,
    // lookaheadObjectLifetime, and lookaheadOtherObjectResources to
    // rank objects. It also updates the histograms intervalHits and
    // intervalEvictions and intervalResources accumulator to monitor
    // how objects behave.
    //
    // Infrequently, during a reconfiguration, HitDensity averages
    // intervalHits/Evictions/Resources into
    // ewmaHits/Evictions/Resources and uses them to re-compute
    // hitProbability and expectedLifetime, from which hit density can
    // be derived. To model the interactions between classes, we model
    // a few future accesses to produce
    // lookaheadHits/ObjectLifetime/OtherObjectResources.

      struct Class {
        Class(const uint64_t _MAX_COARSENED_AGE, const uint64_t& _ageCoarsening);
	virtual ~Class();

	void update();
	void reconfigure();
	void reset();

	// monitoring state
	std::vector<counter_t> intervalHits;
	uint64_t totalIntervalHits;
	std::vector<counter_t> intervalEvictions;
	uint64_t totalIntervalEvictions;
	std::vector<rank_t> ewmaHits;
	rank_t totalEwmaHits;
	std::vector<rank_t> ewmaEvictions;
	rank_t totalEwmaEvictions;

        rank_t intervalResources;
        rank_t ewmaResources;

	// intermediate computed values
	std::vector<rank_t> hitProbability;
	std::vector<rank_t> expectedLifetime;
          
        std::vector<rank_t> lookaheadHits;
        std::vector<rank_t> lookaheadObjectLifetime;
        std::vector<rank_t> lookaheadOtherObjectResources;

	// debug state
	uint64_t cumulativeHits;
	uint64_t cumulativeEvictions;
        const uint64_t MAX_COARSENED_AGE;
        const uint64_t& ageCoarsening;

        void dumpStats(cache::Cache *cache);
      }; // struct Class

    } // namespace hitdensity


  // The main class. HitDensity is an age-based ranking function that
  // monitors the behavior of different object classes (as defined by
  // sub-classes) to find which objects offer the best "bank for the
  // buck", in terms of expected hits-per-resource.
    class HitDensity : public Age {
    public:

      HitDensity(cache::Cache*, int numClasses = 1);
      ~HitDensity();

      typedef hitdensity::rank_t rank_t;

        // ** The main replacement policy interface.

        // Returns the hit density (actually, negative hit density,
        // since higher is worse) of a given object "id".
      rank_t rank(candidate_t id);

        // Called when an object "id" is referenced by "req".
      void update(candidate_t id, const parser::Request& req);

        // Called when an object "id" is evicted.
      void replaced(candidate_t id);

        // Console output at end of simulation.
      void dumpStats();

        // Misc.
      uint32_t getNumClasses() const { return classes.size(); }

    protected:
      cache::Cache* cache;
      const uint64_t MAX_COARSENED_AGE;
      uint64_t ageCoarsening;

      CandidateMap<uint16_t> classIds;
      std::vector<hitdensity::Class*> classes;
    private:
      misc::Rand rand;
      uint64_t nextUpdate;
      uint64_t overflows; // Debugging

    private:
        // Re-compute hit density of different classes
      void reconfigure();
      void reset();
    protected:
      virtual void modelClassInteractions();
    private:
      void adaptAgeCoarsening();
      uint64_t ageCoarseningUpdateAccumulator;
      int intervalsSinceAgeCoarseningUpdate;
      int intervalsBetweenAgeCoarseningUpdates;
      std::list<uint64_t> numObjectsHistory;
        
    protected:
      inline uint64_t age(candidate_t id) {
        uint64_t exactAge = Age::age(id);
        uint64_t coarsenedAge = exactAge / ageCoarsening;
        return std::min(MAX_COARSENED_AGE - 1, coarsenedAge);
      }
      inline hitdensity::Class* cl(candidate_t id) {
        auto cid = classIds[id];
        assert(cid < classes.size());
        return classes[cid];
      }

    };
  
  } // namespace fn

  typedef RankedPolicy<fn::HitDensity> RankedHitDensity;

} // namespace repl

#include "hitdensity_variants.hpp"
