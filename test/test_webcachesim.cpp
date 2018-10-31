//
// Created by Zhenyu Song on 10/30/18.
//
#define CATCH_CONFIG_MAIN  // This tells Catch to provide a main() - only do this in one cpp file
#include "catch.hpp"
#include "simulation.h"

struct InputT{
    string trace_file;
    string cache_type;
    uint64_t cache_size;
    double expected_bhr;
    double expected_ohr;
};

InputT input[] = {
        {.trace_file = "../../test/test.tr", .cache_type = "LRU", .cache_size=1,
                .expected_bhr=0.009775222164140094, .expected_ohr=0.01782310331681281},
        {.trace_file = "../../test/test.tr", .cache_type = "LRU", .cache_size=2,
                .expected_bhr=0.015734448510193413, .expected_ohr=0.027258863896301944},
        {.trace_file = "../../test/test.tr", .cache_type = "LRU", .cache_size=5,
                .expected_bhr=0.039309984317825404, .expected_ohr=0.05213495996950057},
        {.trace_file = "../../test/test.tr", .cache_type = "LRU", .cache_size=10,
                .expected_bhr=0.07684265551489806, .expected_ohr=0.08882958444529165},
        {.trace_file = "../../test/test.tr", .cache_type = "LRU", .cache_size=20,
                .expected_bhr=0.14694197595399897, .expected_ohr=0.15449866565001907},
        {.trace_file = "../../test/test.tr", .cache_type = "LRU", .cache_size=50,
                .expected_bhr=0.27083115525352847, .expected_ohr=0.2750667174990469},
        {.trace_file = "../../test/test.tr", .cache_type = "LRU", .cache_size=100,
                .expected_bhr=0.36999477260846836, .expected_ohr=0.37552420892108274},
        {.trace_file = "../../test/test.tr", .cache_type = "LRU", .cache_size=200,
                .expected_bhr=0.48133821223209616, .expected_ohr=0.4959016393442623},
        {.trace_file = "../../test/test.tr", .cache_type = "LRU", .cache_size=500,
                .expected_bhr=0.658337689492943, .expected_ohr=0.6698436904308044},
        {.trace_file = "../../test/test.tr", .cache_type = "LRU", .cache_size=1000,
                .expected_bhr=0.8051751176163094, .expected_ohr=0.809664506290507},
        {.trace_file = "../../test/test.tr", .cache_type = "LRU", .cache_size=2000,
                .expected_bhr=0.9093047569262938, .expected_ohr=0.9085017155928327},
};

TEST_CASE("webcachesim") {
    map<string, double_t > params;
    for (auto& it : input) {
        SimulationResult res = simulation(it.trace_file, it.cache_type, it.cache_size, params);
        REQUIRE( res.byte_hit_rate == it.expected_bhr);
        REQUIRE( res.object_hit_rate == it.expected_ohr);
    }
}
