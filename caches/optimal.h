#ifndef OPTIMAL_H
#define OPTIMAL_H

#include <vector>
#include <string>
#include <lemon/smart_graph.h>
#include "cache_object.h"
#include <request.h>
#include "solve_mcf.h"

using namespace lemon;


// trace entry
struct trEntry {
    const uint64_t id;
    const uint64_t size;
    double dvar; //for the interval that starts here
    double hit; //for the interval that ends here
    double utility;
    size_t nextSeen;
    int arcId;
    bool hasNext : 1;
    bool active : 1;

    trEntry(uint64_t nid, uint64_t nsize)
            : id(nid),
              size(nsize),
              dvar(0),
              hit(0),
              utility(0),
              nextSeen(0),
              arcId(-1),
              hasNext(false),
              active(false)
    {
    };
}__attribute__((packed));



uint64_t parseTraceFile(std::vector<trEntry> & trace, std::vector<SimpleRequest> reqs);
inline bool isInEjectSet(const double minUtil, const double maxUtil, const trEntry & curEntry) {
    return( curEntry.utility>=minUtil && curEntry.utility<maxUtil );
}
uint64_t createMCF(SmartDigraph & g, std::vector<trEntry > & trace, uint64_t cacheSize, SmartDigraph::ArcMap<int64_t> & cap, SmartDigraph::ArcMap<double> & cost, SmartDigraph::NodeMap<int64_t> & supplies, const double minUtil, const double maxUtil);

std::vector<double> getOptimalDecisions(std::vector<SimpleRequest> requests, uint64_t cacheSize);

#endif

