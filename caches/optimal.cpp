//
// Created by Arnav Garg on 2019-11-28.
//

#include "optimal.h"

//#include <iostream>
#include <fstream>
#include <map>
#include <cassert>
#include <unordered_map>
#include <tuple>
#include <cmath>
#include "optimal.h"
#include <vector>

//using namespace lemon;

std::vector<double> getOptimalDecisions(std::vector<SimpleRequest> requests,
                                        uint64_t cacheSize) {

    std::vector<trEntry> opt_trace;
    uint64_t totalUniqC = parseTraceFile(opt_trace, requests);
    uint64_t totalReqc = opt_trace.size();

    // not sure what these do. The paper doesn't talk about this in the
    // algorithm
    uint64_t maxEjectSize = totalReqc - totalUniqC;
    uint64_t solverPar = 4;
    //-------------


    // max ejection size mustn't be larger than actual trace
    if(maxEjectSize > totalReqc - totalUniqC) {
        maxEjectSize = totalReqc - totalUniqC;
    }

    // ordered list of utilities and check that objects have size less than cache size
    std::vector<double> utilSteps2;
    for(auto & it: opt_trace) {
        if(it.size > cacheSize) {
            it.hasNext = false;
        }
        if(it.hasNext) {
            assert(it.utility>=0);
            utilSteps2.push_back(it.utility);
        }
    }

    std::sort(utilSteps2.begin(), utilSteps2.end(),std::greater<double>());

    // get utility boundaries for ejection sets (based on ejection set size)
    std::vector<double> utilSteps;
    utilSteps.push_back(1); // max util as start
    uint64_t curEjectSize = 0;
    assert(maxEjectSize>0);
    for(auto & it: utilSteps2) {
        curEjectSize++;
        if(curEjectSize >= maxEjectSize/2 && (it != *(--(utilSteps.end())) ) ) {
            utilSteps.push_back(it);
            curEjectSize = 0;
        }
    }
    utilSteps.push_back(0); // min util as end
    utilSteps2.clear();
    utilSteps2.shrink_to_fit();

    long double curCost=0, curHits, overallHits;
    uint64_t integerHits = 0;
    size_t effectiveEjectSize=0;

    // LNS iteration steps
    for(size_t k=0; k+2<utilSteps.size(); k++) {

        // set step's util boundaries
        const double minUtil = utilSteps[k+2];
        const double maxUtil = utilSteps[k];



        // create MCF digraph with arc utilities in [minUtil,maxUtil]
        SmartDigraph g; // mcf graph
        SmartDigraph::ArcMap<int64_t> cap(g); // mcf capacities
        SmartDigraph::ArcMap<double> cost(g); // mcf costs
        SmartDigraph::NodeMap<int64_t> supplies(g); // mcf demands/supplies
        effectiveEjectSize = createMCF(g, opt_trace, cacheSize, cap, cost, supplies, minUtil, maxUtil);



        // solve this MCF
        SmartDigraph::ArcMap<uint64_t> flow(g);
        curCost = solveMCF(g, cap, cost, supplies, flow, solverPar);



        // write DVAR to trace
        curHits = 0;
        overallHits = 0;
        integerHits = 0;
        for(uint64_t i=0; i<opt_trace.size(); i++) {
            if(opt_trace[i].active) {
                opt_trace[i].dvar = 1.0L - flow[g.arcFromId(opt_trace[i].arcId)]/static_cast<long double>(opt_trace[i].size);
                opt_trace[opt_trace[i].nextSeen].hit = opt_trace[i].dvar;
                curHits += opt_trace[i].dvar;
            }
            assert(opt_trace[i].dvar >= 0 && opt_trace[i].dvar<=1);
            overallHits += opt_trace[i].dvar;
            if(opt_trace[i].dvar > 0.99) {
                integerHits++;
            }
        }

    }

    std::vector<double> opt_decisions;

    for(auto & it: opt_trace) {
       opt_decisions.push_back(it.dvar);
    }

    return opt_decisions;
}

uint64_t parseTraceFile(std::vector<trEntry> & trace, std::vector<SimpleRequest> reqs) {
    uint64_t time, id, size, reqc=0, uniqc=0;
    std::unordered_map<std::pair<uint64_t, uint64_t>, uint64_t> lastSeen;

    for (auto& req : reqs) {
        time = req.getTimeStamp();
        id = req.getId();
        size = req.getSize();

        const auto idSize = std::make_pair(id,size);
        if(lastSeen.count(idSize)>0) {
            trace[lastSeen[idSize]].hasNext = true;
            trace[lastSeen[idSize]].nextSeen = reqc;
            const double intervalLength = reqc-lastSeen[idSize];
            // calculate utility
            const double utilityDenominator = size*intervalLength;
            assert(utilityDenominator>0);
            trace[lastSeen[idSize]].utility = 1.0L/utilityDenominator;
            assert(trace[lastSeen[idSize]].utility>0);
        } else {
            uniqc++;
        }
        trace.emplace_back(id,size);
        lastSeen[idSize]=reqc++;
    }
    return uniqc;
}

uint64_t createMCF(SmartDigraph & g, std::vector<trEntry > & trace, uint64_t cacheSize, SmartDigraph::ArcMap<int64_t> & cap, SmartDigraph::ArcMap<double> & cost, SmartDigraph::NodeMap<int64_t> & supplies, const double minUtil, const double maxUtil) {

    size_t effectiveEjectSize = 0;

    // create a graph with just arcs with utility between minUtil and maxUtil
    // mcf instance data
    SmartDigraph::Node curNode = g.addNode(); // initial node
    SmartDigraph::Arc curArc;
    SmartDigraph::Node prevNode;

    // track cached/non-cached intervals
    std::map<std::pair<uint64_t, uint64_t>, std::pair<uint64_t, int> > lastSeen;
    long double nonFlexSize = 0;
    std::map<size_t, long double> endOfIntervalSize;


    for(uint64_t i=0; i<trace.size(); i++) {
        trEntry & curEntry = trace[i];
        curEntry.active = false;

        // first: check if previous interval ended here
        if(lastSeen.count(std::make_pair(curEntry.id,curEntry.size))>0) {
            // create "outer" request arc
            const SmartDigraph::Node lastReq = g.nodeFromId(lastSeen[std::make_pair(curEntry.id,curEntry.size)].second);
            curArc = g.addArc(lastReq,curNode);
            cap[curArc] = curEntry.size;
            cost[curArc] = 1/static_cast <double>(curEntry.size);
            supplies[lastReq] += curEntry.size;
            supplies[curNode] -= curEntry.size;
            trace[lastSeen[std::make_pair(curEntry.id,curEntry.size)].first].arcId = g.id(curArc);
            trace[lastSeen[std::make_pair(curEntry.id,curEntry.size)].first].active = true;
            effectiveEjectSize++;
            // delete lastSeen entry, as the next interval might not be part
            lastSeen.erase(std::make_pair(curEntry.id,curEntry.size));
        }

        // create arcs if in ejection set
        if(isInEjectSet(minUtil, maxUtil, curEntry) ) {
            // second: if there is another request for this object
            if(curEntry.hasNext) {
                // save prev node as anchor for future arcs
                prevNode = curNode;
                lastSeen[std::make_pair(curEntry.id,curEntry.size)]=std::make_pair(i,g.id(prevNode));
                // create another node, "inner" capacity arc
                curNode = g.addNode(); // next node
                curArc = g.addArc(prevNode,curNode);
                cap[curArc] = cacheSize - std::floor(nonFlexSize);
                cost[curArc] = 0;
            }


            //not in ejection set and dvar > 0 -> need to subtract dvar*size for interval's duration
        } else if (curEntry.dvar > 0) {
            const long double curEffectiveSize = curEntry.size*curEntry.dvar;
            assert(cacheSize >= curEffectiveSize);
            nonFlexSize += curEffectiveSize;
            // assert valid flexsize
            size_t nS = curEntry.nextSeen;
            endOfIntervalSize.emplace(nS,curEffectiveSize);
        }


        // clear all nonFlexSize which are
        while(endOfIntervalSize.size() > 0 && endOfIntervalSize.begin()->first <= i+1) {
            nonFlexSize -= endOfIntervalSize.begin()->second;
            endOfIntervalSize.erase(endOfIntervalSize.begin());
        }
    }

    return effectiveEjectSize;

}



bool feasibleCacheAll(std::vector<trEntry > & trace, uint64_t cacheSize, const long double minUtil) {

    // delta data structures: from localratio technique
    int64_t curDelta;
    int64_t deltaStar;
    // map currently cached (id,size) to interval index in trace
    std::unordered_map<std::pair<uint64_t, uint64_t>, size_t> curI; //intersecting intervals at current time

    curDelta = -cacheSize;
    deltaStar = -cacheSize;

    for(size_t j=0; j<trace.size(); j++) {
        trEntry & curEntry = trace[j];

        // if no next request and in intersecting intervals -> remove
        if(!curEntry.hasNext &  (curI.count(std::make_pair(curEntry.id,curEntry.size)) > 0) ) {
            curI.erase(std::make_pair(curEntry.id,curEntry.size));
            curDelta -= curEntry.size;
            assert(curEntry.dvar==0);
        }

        // if with utility in [minUtil,1]
        if(isInEjectSet(minUtil, 1.01, curEntry) && cacheSize >= curEntry.size) {

            // if not already in current intersecting set
            if(curI.count(std::make_pair(curEntry.id,curEntry.size))<=0 ) {
                curDelta += curEntry.size;
            } // else: don't need update the size/width

            // add to current intersecting set
            curI[std::make_pair(curEntry.id,curEntry.size)] = j;

            // check if we need to update deltaStar
            if(curDelta > deltaStar) {
                deltaStar = curDelta;
            }
        }
    }
    // return feasibility bool
    return (deltaStar <=0);

}
