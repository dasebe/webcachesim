#include <lemon/smart_graph.h>

using namespace lemon;

double solveMCF(SmartDigraph & g, SmartDigraph::ArcMap<int64_t> & cap, SmartDigraph::ArcMap<double> & cost, SmartDigraph::NodeMap<int64_t> & supplies, SmartDigraph::ArcMap<uint64_t> & flow, int solverPar);
