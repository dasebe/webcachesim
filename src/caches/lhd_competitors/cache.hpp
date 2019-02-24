#pragma once
#include <iostream>
#include <unordered_map>

#include "constants.hpp"
#include "bytes.hpp"
#include "repl.hpp"

namespace cache_competitors {

struct Cache {
  repl_competitors::Policy* repl;
  uint64_t availableCapacity;
  uint64_t consumedCapacity;
  std::unordered_map<repl_competitors::candidate_t, uint32_t> sizeMap;

  Cache()
    : repl(nullptr)
    , availableCapacity(-1)
    , consumedCapacity(0) {}

  uint32_t getSize(repl_competitors::candidate_t id) const {
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

  bool access(const parser_competitors::Request& req) {
    assert(req.size() > 0);

    auto id = repl_competitors::candidate_t::make(req);
    auto itr = sizeMap.find(id);
    bool hit = (itr != sizeMap.end());

    uint32_t requestSize = req.size();
    if (requestSize >= availableCapacity) {
        std::cerr << "Request too big: " << requestSize << " > " << availableCapacity << std::endl;
        // skip request
        return false;
    }
    assert(requestSize < availableCapacity);
    
    uint32_t cachedSize = 0;
    if (hit) {
      cachedSize = itr->second;
      consumedCapacity -= cachedSize;
    }

    while (consumedCapacity + requestSize > availableCapacity) {
      // need to evict stuff!
      repl_competitors::candidate_t victim = repl->rank(req);
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

      consumedCapacity -= victimItr->second;
      sizeMap.erase(victimItr);
    }

    // insert request
    sizeMap[id] = requestSize;
    consumedCapacity += requestSize;

    assert(consumedCapacity <= availableCapacity);
    repl->update(id, req);

    return hit;
  }

}; // struct Cache

}
