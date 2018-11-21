#include "hitdensity.hpp"
#include "cache.hpp"
#include "constants.hpp"

using namespace repl;
using namespace fn;
using namespace hitdensity;


/*******************************************************************************
 **                                                                           **
 ** INITIALIZATION                                                            **
 **                                                                           **
 ******************************************************************************/

Class::Class(const uint64_t _MAX_COARSENED_AGE, const uint64_t& _ageCoarsening)
    : MAX_COARSENED_AGE(_MAX_COARSENED_AGE)
    , ageCoarsening(_ageCoarsening) {
  // ranks.resize(MAX_COARSENED_AGE, 0);

  intervalHits.resize(MAX_COARSENED_AGE, 0);
  intervalEvictions.resize(MAX_COARSENED_AGE, 0);
  // intervalEvictions.back() = 1;	// avoid divide-by-zeros
  intervalResources = 0.;

  ewmaHits.resize(MAX_COARSENED_AGE, 0.);
  ewmaEvictions.resize(MAX_COARSENED_AGE, 0.);
  ewmaResources = 0.;

  totalEwmaHits = 0;
  totalEwmaEvictions = 0;

  cumulativeHits = 0;
  cumulativeEvictions = 0;
  hitProbability.resize(MAX_COARSENED_AGE, 0.);
  expectedLifetime.resize(MAX_COARSENED_AGE, 0.);
  
  lookaheadHits.resize(MAX_COARSENED_AGE, 0.);
  lookaheadObjectLifetime.resize(MAX_COARSENED_AGE, 0.);
  lookaheadOtherObjectResources.resize(MAX_COARSENED_AGE, 0.);
}

Class::~Class() {
}

/*******************************************************************************
 **                                                                           **
 ** MONITORING                                                                **
 **                                                                           **
 ******************************************************************************/

// Keep a moving average of object behavior in this class.

void Class::update() {
  totalIntervalHits = 0;
  totalIntervalEvictions = 0;
  totalEwmaHits = 0;
  totalEwmaEvictions = 0;

  // average in monitored stats
  for (uint32_t a = 0; a < MAX_COARSENED_AGE; a++) {
    ewmaHits[a] *= EWMA_DECAY;
    ewmaHits[a] += intervalHits[a];

    ewmaEvictions[a] *= EWMA_DECAY;
    ewmaEvictions[a] += intervalEvictions[a];

    // stats
    totalIntervalHits += intervalHits[a];
    totalIntervalEvictions += intervalEvictions[a];

    totalEwmaHits += ewmaHits[a];
    totalEwmaEvictions += ewmaEvictions[a];

    // reset
    intervalHits[a] = 0;
    intervalEvictions[a] = 0;
  }

  ewmaResources *= EWMA_DECAY;
  ewmaResources += intervalResources;
  intervalResources = 0;

  cumulativeHits += totalIntervalHits;
  cumulativeEvictions += totalIntervalEvictions;
  // intervalEvictions.back() = 1;
}

void Class::reset() {
  // dumpStats();

  // interval counters have already been reset in update() just above
  // (done there to increase reuse for performance)
}

/*******************************************************************************
 **                                                                           **
 ** RECONFIGURATION                                                           **
 **                                                                           **
 ******************************************************************************/

// Compute key metrics for hit density (hit probability, lifetime)
// using conditional probability based on an object's age.
//
// This is made efficient by going backwards over ages in two passes,
// so that we build up the expected lifetime in linear time.

void Class::reconfigure() {
  // count events first.
  std::vector<double> events(MAX_COARSENED_AGE);
  std::vector<double> totalEventsAbove(MAX_COARSENED_AGE + 1);

  totalEventsAbove[ MAX_COARSENED_AGE ] = 0.;
    
  for (uint32_t a = MAX_COARSENED_AGE - 1; a < MAX_COARSENED_AGE; a--) {
    events[a] = ewmaHits[a] + ewmaEvictions[a];
    totalEventsAbove[a] = totalEventsAbove[a+1] + events[a];
  }

  uint32_t a = MAX_COARSENED_AGE - 1;
  hitProbability[a] =
    (totalEventsAbove[a] > 1e-5)
    ? ewmaHits[a] / totalEventsAbove[a]
    : 0.;
  expectedLifetime[a] = ageCoarsening;
  double expectedLifetimeUnconditioned = ageCoarsening * totalEventsAbove[a];
  double totalHitsAbove = ewmaHits[a];

  // computed assuming events are uniformly distributed within each
  // coarsened region.

  a = MAX_COARSENED_AGE - 2;
  for ( ; a < MAX_COARSENED_AGE; a--) {
    hitProbability[a] = 0.;
    expectedLifetime[a] = 0.;

    totalHitsAbove += ewmaHits[a];
    expectedLifetimeUnconditioned += ageCoarsening * totalEventsAbove[a];

    if (totalEventsAbove[a] > 1e-5) {
        break;
    }
  }

  a--;

  for ( ; a < MAX_COARSENED_AGE; a--) {
    totalHitsAbove += ewmaHits[a];
    expectedLifetimeUnconditioned += ageCoarsening * totalEventsAbove[a];

    hitProbability[a] = totalHitsAbove / totalEventsAbove[a];
    expectedLifetime[a] = expectedLifetimeUnconditioned / totalEventsAbove[a];
  }
}

/*******************************************************************************
 **                                                                           **
 ** DEBUGGING                                                                 **
 **                                                                           **
 ******************************************************************************/

void Class::dumpStats(cache::Cache *cache) {
  // if (false) { return; }
    
    //  double intervalHitRate = 1. * totalIntervalHits / (totalIntervalHits + totalIntervalEvictions);
    //  double ewmaHitRate = 1. * totalEwmaHits / (totalEwmaHits + totalEwmaEvictions);
    //  double cumulativeHitRate = 1. * cumulativeHits / (cumulativeHits + cumulativeEvictions);

  if (false) {
    float objectAvgSize = 1. * cache->consumedCapacity / cache->getNumObjects();
    rank_t left;

    left = totalEwmaHits + totalEwmaEvictions;
    std::cerr << "Ranks for avg object: ";
    for (uint32_t a = 0; a < MAX_COARSENED_AGE; a++) {
      std::stringstream rankStr;
      rank_t resources = objectAvgSize * lookaheadObjectLifetime[a] + lookaheadOtherObjectResources[a];
      rank_t rank = (resources > 1e-5)? lookaheadHits[a] / resources : 0.;
      rankStr << rank << ", ";
      std::cerr << rankStr.str();

      left -= ewmaHits[a] + ewmaEvictions[a];
      if (rankStr.str() == "0, " && left < 1e-2) {
        break;
      }
    }
    std::cerr << std::endl;

    left = totalEwmaHits + totalEwmaEvictions;
    std::cerr << "Hits: ";
    for (uint32_t a = 0; a < MAX_COARSENED_AGE; a++) {
      std::stringstream rankStr;
      rankStr << ewmaHits[a] << ", ";
      std::cerr << rankStr.str();

      left -= ewmaHits[a] + ewmaEvictions[a];
      if (rankStr.str() == "0, " && left < 1e-2) {
        break;
      }
    }
    std::cerr << std::endl;

    left = totalEwmaHits + totalEwmaEvictions;
    std::cerr << "Evictions: ";
    for (uint32_t a = 0; a < MAX_COARSENED_AGE; a++) {
      std::stringstream rankStr;
      rankStr << ewmaEvictions[a] << ", ";
      std::cerr << rankStr.str();

      left -= ewmaHits[a] + ewmaEvictions[a];
      if (rankStr.str() == "0, " && left < 1e-2) {
        break;
      }
    }
    std::cerr << std::endl;
  }

  //  printf("Class | interval hitRate %g | ewma hitRate %g, ewmaMass %g | cumulative hitRate %g\n",
  //	 intervalHitRate, ewmaHitRate, totalEwmaHits + totalEwmaEvictions, cumulativeHitRate);
}
