#pragma once

#include <string>
#include "candidate.hpp"

namespace cache {

class Cache;

}

namespace repl {

class Policy {
public:
  Policy() {}
  virtual ~Policy() {}

  virtual void update(candidate_t id, const parser::Request& req) = 0;
  virtual void replaced(candidate_t id) = 0;
  virtual candidate_t rank(const parser::Request& req) = 0;

  virtual void dumpStats(cache::Cache* cache) {}
  virtual void dumpMonitor() {}

};

} // namespace repl
