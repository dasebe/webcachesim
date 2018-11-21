#pragma once

#include <limits>
#include "repl.hpp"
#include "rand.hpp"

namespace cache {

class Cache;

}

namespace repl {

class LHD : public virtual Policy {
  public:

    LHD(cache::Cache *cache);
    ~LHD() {}

    void update(candidate_t id, const parser::Request& req);
    void replaced(candidate_t id);
    candidate_t rank(const parser::Request& req);

    void dumpStats(cache::Cache* cache) { }

  private:

    cache::Cache *cache;

    typedef uint64_t timestamp_t;
    typedef uint64_t age_t;
    typedef float rank_t;

    struct Tag {
        age_t timestamp;
        age_t lastHitAge;
        age_t lastLastHitAge;
        uint32_t app;
        
        candidate_t id;
        rank_t size; // stored redundantly with cache
        bool explorer;
    };

    struct Class {
        std::vector<rank_t> hits;
        std::vector<rank_t> evictions;
        rank_t totalHits = 0;
        rank_t totalEvictions = 0;

        std::vector<rank_t> hitDensities;
    };

    std::vector<Tag> tags;
    std::vector<Class> classes;
    std::unordered_map<candidate_t, uint64_t> indices;

    timestamp_t timestamp = 0;
    timestamp_t nextReconfiguration = 0;
    int numReconfigurations = 0;
    
    rank_t ewmaNumObjects = 0;          // for adapting age coarsening
    rank_t ewmaNumObjectsMass = 0.;

    misc::Rand rand;                    // for sampling
    
    static constexpr uint32_t ASSOCIATIVITY = 64;

    timestamp_t ageCoarseningShift = 10;
    
    static constexpr rank_t AGE_COARSENING_ERROR_TOLERANCE = 0.01;
    static constexpr age_t MAX_AGE = 20000;
    static constexpr timestamp_t ACCS_PER_RECONFIGURATION = (1 << 20);
    static constexpr rank_t EWMA_DECAY = 0.9;

    static constexpr uint32_t HIT_AGE_CLASSES = 16;
    static constexpr uint32_t APP_CLASSES = 32;
    static constexpr uint32_t NUM_CLASSES = HIT_AGE_CLASSES * APP_CLASSES;

    static constexpr bool DUMP_RANKS = false;

    // returns something like log(maxAge - age)
    inline uint32_t hitAgeClass(age_t age) const {
        if (age == 0) { return HIT_AGE_CLASSES - 1; }
        uint32_t log = 0;
        while (age < MAX_AGE && log < HIT_AGE_CLASSES - 1) {
            age <<= 1;
            log += 1;
        }
        return log;
    }

    inline uint32_t getClassId(const Tag& tag) const {
        uint32_t hitAgeId = hitAgeClass(tag.lastHitAge + tag.lastLastHitAge);
        return tag.app * HIT_AGE_CLASSES + hitAgeId;
    }
    
    inline Class& getClass(const Tag& tag) {
        return classes[getClassId(tag)];
    }

    uint64_t overflows = 0;

    inline age_t getAge(Tag tag) {
        timestamp_t age = (timestamp >> ageCoarseningShift) - (timestamp_t)tag.timestamp;

        if (age >= MAX_AGE) {
            ++overflows;
            return MAX_AGE - 1;
        } else {
            return (age_t) age;
        }
    }

    inline rank_t getHitDensity(const Tag& tag) {
        auto age = getAge(tag);
        auto& cl = getClass(tag);
        rank_t density = cl.hitDensities[age] / tag.size;
        return density;
    }
        
    void reconfigure();
    void adaptAgeCoarsening();
    void updateClass(Class& cl);
    void modelHitDensity();
    void dumpClassRanks(Class& cl);
};

} // namespace repl
