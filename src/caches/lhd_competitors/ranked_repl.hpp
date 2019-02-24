#pragma once

#include <limits>
#include <unordered_map>
#include <vector>
#include "constants.hpp"
#include "rand.hpp"
#include "repl.hpp"
#include "circular_queue.hpp"

namespace repl_competitors {

  template<class Fn>
  class RankedPolicy : public virtual Policy {
  public:
    template<typename... Args>
    RankedPolicy(int _associativity, int admissionSamples, Args... args)
      : fn(args...)
      , associativity(_associativity) 
      , ADMISSION_SAMPLES(admissionSamples)
      , recentAdmissions(INVALID_CANDIDATE, ADMISSION_SAMPLES)
      , EWMA_DECAY(0.9)
      , ewmaVictimRank(0)
    {}
    ~RankedPolicy() { }

    void update(candidate_t id, const parser_competitors::Request& req) {
      bool insert = false;
      if (indices.find(id) == indices.end()) {
        /* insert */
        cands.push_back(id);
        indices[id] = cands.size() - 1;
        insert = true;
      }
      fn.update(id, req);

      if (insert && ADMISSION_SAMPLES > 0 && fn.rank(id) > ewmaVictimRank) {
        /* could potentially be a victim */
        recentAdmissions.push_back(id);
      }
    }

    void replaced(candidate_t id) {
      typename Fn::rank_t victim_rank = fn.rank(id);
      ewmaVictimRank *= EWMA_DECAY;
      ewmaVictimRank += (1-EWMA_DECAY) * victim_rank;
      // move back of cands to the current position of id and update
      // meta-state
      auto itr = indices.find(id);
      assert(itr != indices.end());
      size_t idx = itr->second;
      indices.erase(itr);

      cands[idx] = cands.back();
      cands.pop_back();

      if (idx < cands.size()) {
        indices[cands[idx]] = idx;
      }

      fn.replaced(id);
    }

    candidate_t rank(const parser_competitors::Request& req) {
      assert(!cands.empty());

      struct Candidate {
        candidate_t id;
        typename Fn::rank_t rank;
      };
      Candidate victim{INVALID_CANDIDATE, std::numeric_limits<typename Fn::rank_t>::lowest()};

      for (int i = 0; i < associativity; i++) {
        size_t idx = rand.next() % cands.size();
        Candidate cand{ cands[idx], fn.rank(cands[idx]) };
        if (cand.rank > victim.rank) {
          victim = cand;
        }
      }

      /* sample from the latest few inserted objects */
      for (unsigned int i=0; i < ADMISSION_SAMPLES; i++) {
        if (recentAdmissions[i] != INVALID_CANDIDATE && indices.find(recentAdmissions[i])!=indices.end()) {
          Candidate cand{ recentAdmissions[i], fn.rank(recentAdmissions[i]) };
          if (cand.rank > victim.rank) {
            victim = cand;
            recentAdmissions[i] = INVALID_CANDIDATE;
          }
        }
      }

      assert(victim.id != INVALID_CANDIDATE);
      return victim.id;
    }

    void dumpStats(cache_competitors::Cache* cache) {
      Policy::dumpStats(cache);

      fn.dumpStats();
    }

  protected:
    Fn fn;
  private:
    int associativity;
    misc_competitors::Rand rand;

    std::vector<candidate_t> cands;
    std::unordered_map<candidate_t, size_t> indices;
    std::unordered_map<candidate_t, size_t> slab_ids;
    const size_t ADMISSION_SAMPLES;
    misc_competitors::CircularNQueue<candidate_t> recentAdmissions;
    double EWMA_DECAY;
    double ewmaVictimRank;
  };

} // namespace repl
