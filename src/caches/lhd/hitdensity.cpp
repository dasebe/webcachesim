#include "hitdensity.hpp"
#include "cache.hpp"

#include <stdio.h>
#include <cmath>
#include <algorithm>
#include <iomanip>

using namespace repl;
using namespace fn;
using namespace hitdensity;

/*******************************************************************************
 **                                                                           **
 ** INITIALIZATION                                                            **
 **                                                                           **
 ******************************************************************************/

namespace repl { namespace fn { namespace hitdensity {

const uint64_t NUM_OBJECTS_HISTORY_LENGTH = 32;
const uint64_t DEFAULT_AVG_OBJECT_SIZE = 1024;
const float AGE_COARSENING_DECAY_FACTOR = 0.9;
uint64_t ACCS_PER_INTERVAL = 1024000;
float EWMA_DECAY = 0.9;
float AGE_COARSENING_ERROR_TOLERANCE = 1e-2;
bool FULL_DEBUG_INFO = false;

uint64_t MAX_SIZE = 1024ull * 1024;
int SIZE_CLASSES = 12;
uint32_t MAX_REFERENCES = 3;
uint32_t APP_CLASSES = 32;
uint32_t MODEL_LOOKAHEAD_DISTANCE = 10;

void initParameters() {
    // nop
}

bool PROFILING = false;

}}} // namespace repl::fn::hitdensity

HitDensity::HitDensity(cache::Cache* _cache, int numClasses)
  : cache(_cache)
  , MAX_COARSENED_AGE(1. / (AGE_COARSENING_ERROR_TOLERANCE * AGE_COARSENING_ERROR_TOLERANCE))
  , classIds(0)
  , nextUpdate(hitdensity::ACCS_PER_INTERVAL)
  , overflows(0) {

  ageCoarsening = 1. * cache->availableCapacity / (DEFAULT_AVG_OBJECT_SIZE * AGE_COARSENING_ERROR_TOLERANCE * MAX_COARSENED_AGE);
  ageCoarseningUpdateAccumulator = 0;
  intervalsSinceAgeCoarseningUpdate = 0;
  intervalsBetweenAgeCoarseningUpdates = 10;
  
  for (int i = 0; i < numClasses; i++) {
    classes.push_back(new Class(MAX_COARSENED_AGE, ageCoarsening));
  }

  std::cerr << "HitDensity initialized with:\n"
            << "  ACCS_PER_INTERVAL = " << ACCS_PER_INTERVAL << std::endl
            << "  EWMA_DECAY = " << EWMA_DECAY << std::endl
            << "  AGE_COARSENING_ERROR_TOLERANCE = " << AGE_COARSENING_ERROR_TOLERANCE << std::endl
            << "  MAX_COARSENED_AGE = " << MAX_COARSENED_AGE << std::endl
            << "  AGE_COARSENING = " << ageCoarsening << std::endl;
}

HitDensity::~HitDensity() {
  for (auto *cl : classes) {
    delete cl;
  }
  classes.clear();
}

/*******************************************************************************
 **                                                                           **
 ** STEADY STATE OPERATION                                                    **
 **                                                                           **
 ******************************************************************************/

rank_t HitDensity::rank(candidate_t id) {
  uint64_t a = age(id);

  // it can never be allowed for the saturating age to have highest
  // rank, or the cache can get stuck with all lines saturated
  if (a == MAX_COARSENED_AGE - 1) {
    return std::numeric_limits<rank_t>::max();
  }

  Class* c = cl(id);
  uint32_t s = cache->getSize(id); assert(s != -1u);

  // compute HitDensity based on the size!
  rank_t resources = s * c->lookaheadObjectLifetime[a] + c->lookaheadOtherObjectResources[a];
  rank_t rank = (resources > 1e-5)? c->lookaheadHits[a] / resources : 0.;

  // Higher HitDensity is better, so return its negation since higher rank is BAD!!!
  return -rank;
}

void HitDensity::update(candidate_t id, const parser::Request& req) {
  if (isPresent(id)) {
    // hit!
    uint64_t a = age(id);
    cl(id)->intervalHits[a] += 1;
    cl(id)->intervalResources += cache->getSize(id) * a * ageCoarsening;
    if (a == MAX_COARSENED_AGE-1) { ++overflows; }
  }

  if (--nextUpdate == 0) {
    adaptAgeCoarsening();
    reconfigure();
    reset();
  }

  Age::update(id, req);
}
    
void HitDensity::replaced(candidate_t id) {
  if (isPresent(id)) {
    // eviction!
    uint64_t a = age(id);
    cl(id)->intervalEvictions[a] += 1;
    cl(id)->intervalResources += cache->getSize(id) * a * ageCoarsening;
    if (a == MAX_COARSENED_AGE-1) { ++overflows; }
  }

  classIds.erase(id);
  Age::replaced(id);
}

/*******************************************************************************
 **                                                                           **
 ** RECONFIGURATION                                                           **
 **                                                                           **
 ******************************************************************************/

void HitDensity::reconfigure() {
  uint64_t intervalHits = 0;
  uint64_t intervalEvictions = 0;
  rank_t ewmaHits = 0;
  rank_t ewmaEvictions = 0;
  uint64_t cumHits = 0;
  uint64_t cumEvictions = 0;

  for (auto* cl : classes) {
    cl->update();

    if (!PROFILING) {
        intervalHits += cl->totalIntervalHits;
        intervalEvictions += cl->totalIntervalEvictions;

        ewmaHits += cl->totalEwmaHits;
        ewmaEvictions += cl->totalEwmaEvictions;

        cumHits += cl->cumulativeHits;
        cumEvictions += cl->cumulativeEvictions;
    }
  }

  for (auto* cl : classes) {
    cl->reconfigure();
  }

  modelClassInteractions();

  for (auto* cl : classes) {
    cl->dumpStats(cache);
  }

  // if (!PROFILING) {
  //     printf("HitDensity        | interval hitRate %lu / %lu = %g, overflows %lu / %lu (%g) | ewmaMass %g | cumulativeHitRate %g | ageCoarsening %lu\n",
  //            intervalHits, intervalHits + intervalEvictions,
  //            (intervalHits + intervalEvictions > 0)? 1. * intervalHits / (intervalHits + intervalEvictions) : 0.,
  //            overflows, ACCS_PER_INTERVAL,
  //            1. * overflows / ACCS_PER_INTERVAL,
  //            ewmaHits + ewmaEvictions,
  //            (cumHits + cumEvictions)? 1. * cumHits / (cumHits + cumEvictions) : 0.,
  //            ageCoarsening);
  // }

}

void HitDensity::reset() {
  for (auto* cl : classes) {
    cl->reset();
  }

  nextUpdate = ACCS_PER_INTERVAL;
  overflows = 0;
}

void HitDensity::dumpStats() {
  for (auto* cl: classes) {
    cl->dumpStats(cache);
  }
}

// The correct age coarsening value to use depends on the number of
// objects, but since object size varies widely across different
// traces, we do not know from the cache size how many objects there
// will be in the cache a priori. Instead, we let the cache run for a
// little bit and then update the age coarsening value based on how
// many objects we see. Thereafter, we update the age coarsening value
// very infrequently.
//
// HitDensity is not very sensitive to the age coarsening value, it
// just needs to be in the right order-of-magnitude neighborhood. If
// you can roughly estimate the size of your objects, then you can
// just set the age coarsening value once during initialization and
// remove the adaptive age coarsening below.

void HitDensity::adaptAgeCoarsening() {
  // dynamic age scaling ... use the number of objects to set the age coarsening!
  uint64_t numObjects = cache->getNumObjects();
  assert(numObjects > 0);

  numObjectsHistory.push_back(numObjects);
  if (numObjectsHistory.size() > NUM_OBJECTS_HISTORY_LENGTH) {
    numObjectsHistory.pop_front();
  }
  std::vector<uint64_t> findMedianNumObjects(numObjectsHistory.begin(), numObjectsHistory.end());
  std::nth_element(&findMedianNumObjects[ 0 ],
		   &findMedianNumObjects[ findMedianNumObjects.size() / 2 ],
		   &findMedianNumObjects[ findMedianNumObjects.size() ]);
  
  numObjects = findMedianNumObjects[ findMedianNumObjects.size() / 2 ];

  std::cerr << "Num objects history: ";
  for (auto no : numObjectsHistory) {
    std::cerr << no << ", ";
  }
  std::cerr << std::endl;
  std::cerr << "Median num objects: " << numObjects << std::endl;
  
  // this is the general formula ... but if we assume
  // MAX_COARSENED_AGE is initialized as above, then it reduces to
  // simply:
  // 
  //   ageCoarsening = numObjects * AGE_COARSENING_ERROR_TOLERANCE
  // 
  uint64_t nextAgeCoarsening = 1. * numObjects / (AGE_COARSENING_ERROR_TOLERANCE * MAX_COARSENED_AGE);
  nextAgeCoarsening = std::max(nextAgeCoarsening, 1ul);

  std::cerr << "There are " << numObjects << " objects in the cache, implying an ageCoarsening " << nextAgeCoarsening << " (ageCoarsening is actually: " << ageCoarsening << ")" << std::endl;

  ageCoarseningUpdateAccumulator += nextAgeCoarsening;
  intervalsSinceAgeCoarseningUpdate += 1;

  if (intervalsSinceAgeCoarseningUpdate >= intervalsBetweenAgeCoarseningUpdates) {
    ageCoarsening = ageCoarseningUpdateAccumulator / intervalsSinceAgeCoarseningUpdate;

    std::cerr << "Updating ageCoarsening to " << ageCoarsening << " after " << intervalsSinceAgeCoarseningUpdate << " intervals." << std::endl;

    ageCoarseningUpdateAccumulator = 0;
    intervalsSinceAgeCoarseningUpdate = 0;
    intervalsBetweenAgeCoarseningUpdates = 100;
  }
}

