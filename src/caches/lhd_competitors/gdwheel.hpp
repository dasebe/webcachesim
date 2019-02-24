#pragma once

#include "lru.hpp"
#include "repl.hpp"

namespace misc_competitors {

static constexpr uint64_t POW(int N, int d) {
    return d > 0?
        N * POW(N, d-1) : 1;
}

}

namespace repl_competitors {

class GDWheel : public Policy {
  public:
    GDWheel() : freq(0) {}
    ~GDWheel() {}
    
    void update(candidate_t id, const parser_competitors::Request& req) {
        freq[id] += 1;
        
        auto* entry = tags.lookup(id);
        if (entry) {
            assert(entry->data.id == id);
            entry->remove();
        } else {
            entry = tags.allocate(id, Data{id, 0});
        }

        entry->data.rank = MAX_VALUE * freq[id] / req.size();
        if (entry->data.rank >= MAX_VALUE) {
            ++overflows;
            entry->data.rank = MAX_VALUE - 1;
        }
        
        hcw.insert(entry);
    }

    void replaced(candidate_t id) {
        auto* entry = tags.evict(id);
        assert(entry->data.id == id);
        entry->remove();
        delete entry;

        freq.erase(id);
    }

    candidate_t rank(const parser_competitors::Request& req) {
        hcw.forth();
        assert(!hcw.wheels[0].get_list().empty());
        return hcw.wheels[0].get_list().back().id;
    }

    void dumpStats(cache_competitors::Cache* cache) {
        std::cerr << "GD-Wheel overflows " << overflows << std::endl;
    }

  private:
    static constexpr int N = 256;
    static constexpr int DEPTH = 2;

    static const uint64_t MAX_VALUE = misc_competitors::POW(N, DEPTH);

    struct Data {
        candidate_t id;
        uint64_t rank;
    };

    struct HierarchicalCostWheel {
        HierarchicalCostWheel() {
            Nexp[0] = 1;
            for (int d = 1; d < DEPTH; d++) {
                Nexp[d] = Nexp[d-1] * N;
            }
        }

        struct CostWheel {
            CostWheel() : hand(0) {}
        
            int hand;
            List<Data> lists[N];

            List<Data>& get_list() { return lists[hand]; }
        };

        CostWheel wheels[DEPTH];
        uint64_t Nexp[DEPTH];

        void insert(List<Data>::Entry* entry) {
            int d = 0;
            uint64_t rank = entry->data.rank;
            while (rank >= N) {
                rank /= N;
                d += 1;
            }

            assert(d < DEPTH);
            int index = (wheels[d].hand + rank) % N;
            wheels[d].lists[index].insert_back(entry);
        }

        void forth(int d = 0) {
            if (d >= DEPTH) { return; }

            if (d > 0) {
                auto& list = wheels[d].get_list();
                List<Data>::Entry *e, *next;
                for (e = list.begin();
                     e != list.end();
                     e = next) {

                    next = e->next;
                    e->remove();

                    // a + b * N migrates from b in wheel 2 to a in wheel 1
                    uint64_t rank = e->data.rank;
                    int a = (rank / Nexp[d-1]) % N;

                    wheels[d-1].lists[a].insert_back(e);
                }
            }
            
            while (wheels[d].get_list().empty()) {
                wheels[d].hand += 1;
                wheels[d].hand %= N;

                if (wheels[d].hand == 0) {
                    forth(d + 1);
                }
            }
        }
    };

    uint64_t overflows = 0;
    Tags<Data> tags;
    HierarchicalCostWheel hcw;
    CandidateMap<uint64_t> freq;
};

} // namespace misc
