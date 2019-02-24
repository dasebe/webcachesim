#pragma once

#include <string>
#include "candidate.hpp"

namespace cache_competitors {

class Cache;

}

namespace repl_competitors {

class Policy {
public:
  Policy() {}
  virtual ~Policy() {}

  virtual void update(candidate_t id, const parser_competitors::Request& req) = 0;
  virtual void replaced(candidate_t id) = 0;
  virtual candidate_t rank(const parser_competitors::Request& req) = 0;

  virtual void dumpStats(cache_competitors::Cache* cache) {}
  virtual void dumpMonitor() {}

};

} // namespace repl
