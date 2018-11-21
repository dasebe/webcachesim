#include "hitdensity.hpp"

using namespace repl;
using namespace fn;
using namespace hitdensity;

namespace repl { namespace fn { namespace hitdensity {
extern uint32_t MODEL_LOOKAHEAD_DISTANCE;
}}}

// Modeling how classes interact is very important (particularly for
// reuse classification). However, HitDensity cannot model the
// differences between classes over all future accesses, because the
// hit density of all objects is identical in the limit to infinity
// (see: Markov decision processes that optimize long-run average
// reward). So what we do is propagate the expected number of hits and
// resources consumed for a small number of accesses looking forward.
//
// Each classification has slightly different transitions between
// classes; the ModelLookahead class gives the basic code to model a
// few more accesses, and each HitDensity subclass specializes for its
// particular class structure.

class ModelLookahead {
  public:
    void add(Class* cl);
    uint32_t nclasses() const { return classes.size(); }
    void go();

    virtual uint32_t hitClass(uint32_t cid) const = 0;
    virtual uint32_t missClass(uint32_t cid) const = 0;

  private:
    std::vector< Class* > classes;
};

void ModelLookahead::add(Class* cl) {
    classes.push_back(cl);
}

void ModelLookahead::go() {
    const uint32_t NCLASSES = classes.size();
    const uint32_t MAX_COARSENED_AGE = classes.front()->hitProbability.size();

    uint32_t hitclasses[NCLASSES];
    uint32_t missclasses[NCLASSES];
    rank_t avgresources[NCLASSES];

    for (uint32_t c = 0; c < NCLASSES; c++) {
        hitclasses[c] = hitClass(c);
        missclasses[c] = missClass(c);
        avgresources[c] = classes[c]->ewmaResources / (classes[c]->totalEwmaHits + classes[c]->totalEwmaEvictions);
    }

    // double buffer to avoid any complexity around loop order etc
    rank_t hits[NCLASSES][2];
    rank_t lifetimes[NCLASSES][2];
    rank_t resources[NCLASSES][2];
    rank_t resourcesEvictions[NCLASSES][2];

    for (uint32_t c = 0; c < NCLASSES; c++) {
        hits[c][0] = 0;
        hits[c][1] = 0;
        lifetimes[c][0] = 0;
        lifetimes[c][1] = 0;
        resources[c][0] = 0;
        resources[c][1] = 0;
        resourcesEvictions[c][0] = 0;
        resourcesEvictions[c][1] = 0;
    }

    // model the interactions between classes across a few accesses,
    // following formulae in appendix
    for (uint32_t n = 0; n < MODEL_LOOKAHEAD_DISTANCE; n++) {
        uint32_t src = n % 2;
        uint32_t dst = (n+1) % 2;

        for (uint32_t c = 0; c < NCLASSES; c++) {
            auto* cl = classes[c];
            auto hc = hitclasses[c];
            auto mc = missclasses[c];
                
            hits[c][dst] =
                cl->hitProbability[0]
                + cl->hitProbability[0] * hits[hc][src]
                + (1 - cl->hitProbability[0]) * hits[mc][src];

            lifetimes[c][dst] =
                cl->expectedLifetime[0]
                + cl->hitProbability[0] * lifetimes[hc][src];

            resources[c][dst] =
                cl->hitProbability[0] * resources[hc][src]
                + (1 - cl->hitProbability[0]) * resourcesEvictions[mc][src];

            resourcesEvictions[c][dst] =
                avgresources[c] + resourcesEvictions[c][src];
        }
    }

    // add the results back into the expected behavior of objects at
    // all ages
    for (uint32_t a = 0; a < MAX_COARSENED_AGE; a++) {
        for (uint32_t c = 0; c < NCLASSES; c++) {
            auto* cl = classes[c];
            auto hc = hitclasses[c];
            auto mc = missclasses[c];

            cl->lookaheadHits[a] =
                cl->hitProbability[a]
                + cl->hitProbability[a] * hits[hc][MODEL_LOOKAHEAD_DISTANCE % 2]
                + (1 - cl->hitProbability[a]) * hits[mc][MODEL_LOOKAHEAD_DISTANCE % 2];

            cl->lookaheadObjectLifetime[a] =
                cl->expectedLifetime[a]
                + cl->hitProbability[a] * lifetimes[hc][MODEL_LOOKAHEAD_DISTANCE % 2];

            cl->lookaheadOtherObjectResources[a] =
                cl->hitProbability[a] * resources[hc][MODEL_LOOKAHEAD_DISTANCE % 2]
                + (1 - cl->hitProbability[a]) * resourcesEvictions[mc][MODEL_LOOKAHEAD_DISTANCE % 2];
        }
    }
}

// Now specialize this general model for each classification scheme,
// as appropriate

void HitDensity::modelClassInteractions() {
    class ModelLookaheadNull : public ModelLookahead {
      public:
        uint32_t hitClass(uint32_t clid) const { assert(clid == 0); return 0; }
        uint32_t missClass(uint32_t clid) const { assert(clid == 0); return 0; }
    };
    ModelLookaheadNull model;
    model.add(classes.front());
    model.go();
}

void HitDensityByReuse::modelClassInteractions() {
    class ModelLookaheadByReuse : public ModelLookahead {
      public:
        uint32_t hitClass(uint32_t clid) const { return (clid+1 < nclasses()) ? clid+1 : clid; }
        uint32_t missClass(uint32_t clid) const { return 0; }
    };
    ModelLookaheadByReuse model;
    for (auto* cl : classes) {
        model.add(cl);
    }
    model.go();
}

// App classification requires a "dummy" class that averages behavior
// across all apps, since we do not know which app the replacement
// object will come from.

void HitDensityByApp::modelClassInteractions() {
    assert("Untested" && false);
    
    class ModelLookaheadByApp : public ModelLookahead {
      public:
        // Assume that the dummy class stays in the dummy class, which
        // seems reasonable.
        uint32_t hitClass(uint32_t clid) const { return clid; }
        uint32_t missClass(uint32_t clid) const { return nclasses()-1; }
    };
    ModelLookaheadByApp model;
    for (auto* cl : classes) {
        model.add(cl);
    }

    // dummy class that tracks what the "average object" is doing
    Class avgObject(MAX_COARSENED_AGE, ageCoarsening);
    for (auto* cl : classes) {
        for (uint32_t a = 0; a < MAX_COARSENED_AGE; a++) {
            avgObject.ewmaHits[a] += cl->ewmaHits[a];
            avgObject.ewmaEvictions[a] += cl->ewmaEvictions[a];
        }
        avgObject.ewmaResources += cl->ewmaResources;
        avgObject.totalEwmaHits += cl->totalEwmaHits;
        avgObject.totalEwmaEvictions += cl->totalEwmaEvictions;
    }
    avgObject.reconfigure();
    model.add(&avgObject);
    
    model.go();
}

void HitDensityByAppAndReuse::modelClassInteractions() {
    class ModelLookaheadByAppAndReuse : public ModelLookahead {
      public:
        ModelLookaheadByAppAndReuse(HitDensityByAppAndReuse* _util) : util(_util) {}
        // Hits from a valid app increase ref count as normal;
        // the dummy classes do as well, but with a different code path
        uint32_t hitClass(uint32_t clid) const {
            uint32_t refs = clid / (util->numAppClasses + 1);
            uint32_t app = clid % (util->numAppClasses + 1);

            uint32_t nextRefs = (refs+1 <= util->maxReferenceCount)? refs+1 : refs;
            uint32_t nextApp = app;
            return nextRefs * (util->numAppClasses + 1) + nextApp;
        }
        // Misses always become generic, non-reused objects
        uint32_t missClass(uint32_t clid) const {
            // uint32_t refs = clid / (util->numAppClasses + 1);
            // uint32_t app = clid % (util->numAppClasses + 1);
            
            uint32_t nextRefs = 0;
            uint32_t nextApp = util->numAppClasses;
            return nextRefs * (util->numAppClasses + 1) + nextApp;
        }
      private:
        HitDensityByAppAndReuse* util;
    };
    ModelLookaheadByAppAndReuse model(this);

    Class* avgObjects[maxReferenceCount+1];

    // add all the per-app classes and build up a dummy, generic
    // object for each ref count
    for (uint32_t refs = 0; refs <= maxReferenceCount; refs++) {
        // avg of all objects with this many refs
        avgObjects[refs] = new Class(MAX_COARSENED_AGE, ageCoarsening);
        
        for (uint32_t app = 0; app < numAppClasses; app++) {
            uint32_t clid = getClassId(app, refs);
            auto* cl = classes[clid];
            model.add(cl);

            for (uint32_t a = 0; a < MAX_COARSENED_AGE; a++) {
                avgObjects[refs]->ewmaHits[a] += cl->ewmaHits[a];
                avgObjects[refs]->ewmaEvictions[a] += cl->ewmaEvictions[a];
            }
            avgObjects[refs]->ewmaResources += cl->ewmaResources;
            avgObjects[refs]->totalEwmaHits += cl->totalEwmaHits;
            avgObjects[refs]->totalEwmaEvictions += cl->totalEwmaEvictions;
        }
        avgObjects[refs]->reconfigure();
        model.add(avgObjects[refs]);
    }

    model.go();

    for (uint32_t refs = 0; refs <= maxReferenceCount; refs++) {
        delete avgObjects[refs];
    }
}

