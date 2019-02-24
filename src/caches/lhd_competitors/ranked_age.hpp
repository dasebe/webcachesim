#pragma once

#include <unordered_map>
#include <stdint.h>

#include "ranked_repl.hpp"

namespace repl_competitors {

  namespace fn_competitors {

    class Age {
    public:
      Age()
	: now(0)
	, lastUse(-1ull) {}

      uint64_t age(candidate_t id) {
	assert(lastUse[id] != lastUse.DEFAULT);
	return now - lastUse[id];
      }

      bool isPresent(candidate_t id) {
	return lastUse[id] != lastUse.DEFAULT;
      }

      void update(candidate_t id, const parser_competitors::Request& req) {
	lastUse[id] = now;
	++now;
      }

      void replaced(candidate_t id) {
	lastUse.erase(id);
      }

    private:
      uint64_t now;
      CandidateMap<uint64_t> lastUse;
    };

  } // namespace fn

} // namespace repl
