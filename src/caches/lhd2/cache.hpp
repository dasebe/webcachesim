#pragma once
#include <iostream>
#include <unordered_map>

#include "constants.hpp"
#include "bytes.hpp"
#include "repl.hpp"

namespace cache {

// Note: because candidates do not have the same size, in general:
// 
//    accesses != hits + evictions + fills     (not equal!)
// 
// A single access can result in multiple evictions to make space for
// the new object. That is, evictions != misses.
// 
// Fills is the number of misses that don't require an eviction, that
// is there is sufficient available space to fit the object. Evictions
// is the number of evictions, _not_ the number of misses that lead to
// an eviction.
struct Cache {
  repl::Policy* repl;
  uint64_t hits;
  uint64_t misses;
  uint64_t compulsoryMisses;
  uint64_t fills;
  uint64_t evictions;
  uint64_t accessesTriggeringEvictions;
  uint64_t missesTriggeringEvictions;
  uint64_t cumulativeAllocatedSpace;
  uint64_t cumulativeFilledSpace;
  uint64_t cumulativeEvictedSpace;
  uint64_t accesses;
  uint64_t availableCapacity;
  uint64_t consumedCapacity;
  std::unordered_map<repl::candidate_t, uint32_t> sizeMap;
  repl::CandidateMap<bool> historyAccess;

  Cache()
    : repl(nullptr)
    , hits(0)
    , misses(0)
    , compulsoryMisses(0)
    , fills(0)
    , evictions(0)
    , accessesTriggeringEvictions(0)
    , missesTriggeringEvictions(0)
    , cumulativeAllocatedSpace(0)
    , cumulativeFilledSpace(0)
    , cumulativeEvictedSpace(0)
    , accesses(0)
    , availableCapacity(-1)
    , consumedCapacity(0)
    , historyAccess(false) {}

  uint32_t getSize(repl::candidate_t id) const {
    auto itr = sizeMap.find(id);
    if (itr != sizeMap.end()) {
      return itr->second;
    } else {
      return -1u;
    }
  }

  uint32_t getNumObjects() const {
    return sizeMap.size();
  }

    bool access(const parser::Request& req) {
    assert(req.size() > 0);
    if (req.type != parser::GET) { return(false); }

    auto id = repl::candidate_t::make(req);
    auto itr = sizeMap.find(id);
    bool hit = (itr != sizeMap.end());

    if (!historyAccess[id]) {
      // first time requests are considered as compulsory misses
      ++compulsoryMisses;
      historyAccess[id] = true;
    }

    if (hit) { ++hits; } else { ++misses; }
    ++accesses;

    // stats?
    if ((STATS_INTERVAL > 0) && ((accesses % STATS_INTERVAL) == 0)) {
        std::cout << "Stats: "
                  << hits << ", "
                  << misses << ", "
                  << fills << ", "
                  << compulsoryMisses << ", "
                  << (100. * hits / accesses)
                  << std::endl;
    }

    uint32_t requestSize = req.size();
    if (requestSize >= availableCapacity) {
        std::cerr << "Request too big: " << requestSize << " > " << availableCapacity << std::endl;
    }
    assert(requestSize < availableCapacity);
    
    uint32_t cachedSize = 0;
    if (hit) {
      cachedSize = itr->second;
      consumedCapacity -= cachedSize;
    }

    uint32_t evictionsFromThisAccess = 0;
    uint64_t evictedSpaceFromThisAccess = 0;

    while (consumedCapacity + requestSize > availableCapacity) {
      // need to evict stuff!
      repl::candidate_t victim = repl->rank(req);
      auto victimItr = sizeMap.find(victim);
      if (victimItr == sizeMap.end()) {
        std::cerr << "Couldn't find victim: " << victim << std::endl;
      }
      assert(victimItr != sizeMap.end());

      repl->replaced(victim);

      // replacing candidate that just hit; don't free space twice
      if (victim == id) {
        continue;
      }

      evictionsFromThisAccess += 1;
      evictedSpaceFromThisAccess += victimItr->second;
      consumedCapacity -= victimItr->second;
      sizeMap.erase(victimItr);
    }

    // indicate where first eviction happens
    if (evictionsFromThisAccess > 0) {
      ++accessesTriggeringEvictions;
      if (evictions == 0) { std::cout << "x"; }
    }

    // measure activity
    evictions += evictionsFromThisAccess;
    cumulativeEvictedSpace += evictedSpaceFromThisAccess;
    if (hit) {
      if (requestSize > cachedSize) {
        cumulativeAllocatedSpace += requestSize - cachedSize;
      }
    } else {
      cumulativeAllocatedSpace += requestSize;
      if (evictionsFromThisAccess == 0) {
        // misses that don't require evictions are fills by definition
        ++fills;
        cumulativeFilledSpace += requestSize;
      } else {
        ++missesTriggeringEvictions;
      }
    }

    // insert request
    sizeMap[id] = requestSize;
    consumedCapacity += requestSize;

    assert(consumedCapacity <= availableCapacity);
    repl->update(id, req);

    return hit;
  }

  void dumpStats() {
    using std::endl;
    std::cout 
      << "Accesses: " << accesses
      << "\t(" << misc::bytes(cumulativeAllocatedSpace) << ")" << endl
      << "Hits: " << hits << " " << (100. * hits / accesses) << "%" << endl
      << "Misses: " << misses << " " << (100. * misses / accesses) << "%" << endl
      << "Compulsory misses: " << compulsoryMisses << " " << (100. * compulsoryMisses / accesses) << "%" << endl
      << "Non-compulsory hit rate: " << (100. * hits / (accesses - compulsoryMisses)) << "%" << endl
      << "  > Fills: " << fills << " " << (100. * fills / accesses) << "%"
      << "\t(" << misc::bytes(cumulativeFilledSpace) << ")" << endl
      << "  > Misses triggering evictions: " << missesTriggeringEvictions << " " << (100. * missesTriggeringEvictions / accesses) << "%" << endl
      << "  > Evictions: " << evictions << " " << (100. * evictions / accesses) << "%"
      << "\t(" << misc::bytes(cumulativeEvictedSpace) << ")" << endl
      << "  > Accesses triggering evictions: " << accessesTriggeringEvictions << " (" << (1. * evictions / accessesTriggeringEvictions) << " evictions per trigger)" << endl
      ;
  }

}; // struct Cache

}
