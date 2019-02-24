#pragma once

#include <queue>
#include "rand.hpp"
#include "priority_queue.hpp"

namespace repl_competitors {

template<class Fn>
class RankedHeapPolicy : public virtual Policy {
  public:
    template<typename... Args>
    RankedHeapPolicy(Args... args)
        : fn(args...)
        , pq(*this)     { }

    ~RankedHeapPolicy() { }

    void update(candidate_t id, const parser_competitors::Request& req) {
        fn.update(id, req);
        typename Fn::rank_t rank = fn.rank(id);

        auto itr = cands.find(id);
        if (itr == cands.end()) {
            pq.push(id, -rank);
        } else {
            pq.update(itr->second, -rank);
        }
    }

    void replaced(candidate_t id) {
        auto itr = cands.find(id);
        assert(itr != cands.end());
        pq.erase(itr->second);
        cands.erase(itr);
        
        fn.replaced(id);
    }

    candidate_t rank(const parser_competitors::Request& req) {
        return pq.peek().key;
    }

    void dumpStats(cache_competitors::Cache* cache) {
        Policy::dumpStats(cache);
        fn.dumpStats();
    }

    void dumpMonitor() { }

    void priorityQueueUpdate(candidate_t id, size_t idx) {
        cands[id] = idx;
    }

  protected:
    Fn fn;
  private:
    PriorityQueue<typename Fn::rank_t, RankedHeapPolicy<Fn>, candidate_t> pq;
    std::unordered_map<candidate_t, size_t> cands;
};

} // namespace repl
