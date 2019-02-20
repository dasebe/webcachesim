#pragma once

#include <string>
#include "candidate.hpp"

namespace cache {

class Cache;

}

namespace libconfig {
  class Setting;
}

namespace repl {

  const float NULL_MAX_RANK = 1;

class Policy {
public:
    Policy() {}
  virtual ~Policy() {}

  virtual void update(candidate_t id, const parser::Request& req) = 0;
  virtual void replaced(candidate_t id) = 0;
  virtual candidate_t rank(const parser::Request& req) = 0;

  virtual void dumpStats(cache::Cache* cache) {}

  static Policy* create(cache::Cache* cache, const libconfig::Setting &settings);
};

} // namespace repl
