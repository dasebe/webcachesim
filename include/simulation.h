//
// Created by Zhenyu Song on 10/30/18.
//

#ifndef WEBCACHESIM_SIMULATION_H
#define WEBCACHESIM_SIMULATION_H

#include <map>
#include <string>

using namespace std;

class SimulationResult {
public:
    SimulationResult(double bhr=0, double ohr=0)
    :   byte_hit_rate(bhr),
        object_hit_rate(ohr)
    {
    }

    double byte_hit_rate;
    double object_hit_rate;
};


/*
 * single thread simulation. Returns results.
 */
SimulationResult simulation(string tracefile, string cache_type, uint64_t cache_size, map<string, double> & params);


#endif //WEBCACHESIM_SIMULATION_H
