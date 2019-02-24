#pragma once

#include "ranked_age.hpp"

namespace repl_competitors {

  namespace fn_competitors {

    class LRU : public Age {
    public:
      typedef uint64_t rank_t;

      uint64_t rank(candidate_t id) {
	return age(id);
      }

      void dumpStats() {}
    };

  } // namespace fn

  typedef RankedPolicy<fn_competitors::LRU> RankedLRU;

} // namespace repl
