#pragma once

#include "ranked_repl.hpp"
#include "ranked_heap_repl.hpp"

namespace repl_competitors {

  namespace fn_competitors {
    class GreedyDualSize {
    public:
      GreedyDualSize() : ranks(0), L(0) {}
      ~GreedyDualSize() {}

      typedef float rank_t;

      rank_t rank(candidate_t id) {
	return -ranks[id];
      }

      void update(candidate_t id, const parser_competitors::Request& req) {
	ranks[id] = L + COST / req.size();
      }

      void replaced(candidate_t id) {
	L = std::max(L, ranks[id]);
	// std::cerr << "Evicted " << ranks[id] << " L is now " << L << std::endl;
	ranks.erase(id);
      }

      void dumpStats() { }
      
    private:
      CandidateMap<rank_t> ranks;
      rank_t L;			// "inflation value" from GDS paper -- eviction threshold
      static constexpr rank_t COST = 1; // cost 1 is to maximize hit rate

    }; // class GreedyDualSize

    class GreedyDualSizeFreq {
    public:
      GreedyDualSizeFreq() : ranks(0), freq(0), L(0) {}
      ~GreedyDualSizeFreq() {}

      typedef float rank_t;

      rank_t rank(candidate_t id) {
	return -ranks[id];
      }

      void update(candidate_t id, const parser_competitors::Request& req) {
	freq[id] += 1;
	ranks[id] = L + COST * freq[id] / req.size();
      }

      void replaced(candidate_t id) {
	L = std::max(L, ranks[id]);
	// std::cerr << "Evicted " << ranks[id] << " L is now " << L << std::endl;
	ranks.erase(id);
	freq.erase(id);
      }

      void dumpStats() { }
      
    private:
      CandidateMap<rank_t> ranks;
      CandidateMap<uint64_t> freq;
      rank_t L;			// "inflation value" from GDS paper -- eviction threshold
      static constexpr rank_t COST = 1; // cost 1 is to maximize hit rate

    }; // class GreedyDualSize
  } // namespace fn

  typedef RankedPolicy<fn_competitors::GreedyDualSize> RankedGreedyDualSize;
  typedef RankedPolicy<fn_competitors::GreedyDualSizeFreq> RankedGreedyDualSizeFreq;

  typedef RankedHeapPolicy<fn_competitors::GreedyDualSize> GreedyDualSize;
  typedef RankedHeapPolicy<fn_competitors::GreedyDualSizeFreq> GreedyDualSizeFreq;

} // namespace repl
