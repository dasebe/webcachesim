#include "solve_mcf.h"

// comment next line to use CapacityScaling solver
#define NETWORKSIMPLEX

#ifdef NETWORKSIMPLEX
#include <lemon/network_simplex.h>

typedef NetworkSimplex<SmartDigraph, int64_t, double> SolverType;

#else
#include <lemon/capacity_scaling.h>
typedef CapacityScaling<SmartDigraph, int64_t, double, CapacityScalingDefaultTraits<SmartDigraph, int64
_t, double> > SolverType;

#endif

using namespace lemon;

double solveMCF(SmartDigraph & g, SmartDigraph::ArcMap<int64_t> & cap, SmartDigraph::ArcMap<double> & cost, SmartDigraph::NodeMap<int64_t> & supplies, SmartDigraph::ArcMap<uint64_t> & flow, int solverPar) {

    // solve the mcf instance
    SolverType solver(g);
    solver.upperMap(cap).costMap(cost).supplyMap(supplies);

    SolverType::ProblemType res;
#ifdef NETWORKSIMPLEX
    switch(solverPar) {
        case 1: res = solver.run(SolverType::FIRST_ELIGIBLE);
            break;
        case 2: res = solver.run(SolverType::BEST_ELIGIBLE);
            break;
        case 4: res = solver.run(SolverType::CANDIDATE_LIST);
            break;
        case 8: res = solver.run(SolverType::ALTERING_LIST);
            break;
        default: res = solver.run(SolverType::BLOCK_SEARCH);
            break;
    }
#else
    res = solver.run(solverPar);
#endif

    if(res==SolverType::INFEASIBLE) {
        std::cerr << "infeasible mcf" << std::endl;
        return -1;
    } else if (res==SolverType::UNBOUNDED) {
        std::cerr << "unbounded mcf" << std::endl;
        return -1;
    }

    solver.flowMap(flow);

    return solver.totalCost<double>();
}
