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


#define webcachesim_test(input) \
    for (auto& it : input) { \
        auto res = simulation(it.trace_file, it.cache_type, it.cache_size, {}); \
        REQUIRE( res["byte_hit_rate"] == to_string(it.expected_bhr)); \
        REQUIRE( res["object_hit_rate"] == to_string(it.expected_ohr)); \
    }



InputT inputLRU[] = {
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

TEST_CASE("webcachesimLRU") {
    webcachesim_test(inputLRU);
}

InputT inputBelady[] = {
        {.trace_file = "../../test/test.tr", .cache_type = "Belady", .cache_size=1,
                .expected_bhr=0.05781495033978045, .expected_ohr=0.10541364849409074},
        {.trace_file = "../../test/test.tr", .cache_type = "Belady", .cache_size=2,
                .expected_bhr=0.08724516466283325, .expected_ohr=0.15268776210446053},
        {.trace_file = "../../test/test.tr", .cache_type = "Belady", .cache_size=5,
                .expected_bhr=0.15446941975953998, .expected_ohr=0.20301181852840258},
        {.trace_file = "../../test/test.tr", .cache_type = "Belady", .cache_size=10,
                .expected_bhr=0.2740198640878202, .expected_ohr=0.2919367136866184},
        {.trace_file = "../../test/test.tr", .cache_type = "Belady", .cache_size=20,
                .expected_bhr=0.36408782017773134, .expected_ohr=0.3844834159359512},
        {.trace_file = "../../test/test.tr", .cache_type = "Belady", .cache_size=50,
                .expected_bhr=0.48285415577626767, .expected_ohr=0.5090545177277926},
        {.trace_file = "../../test/test.tr", .cache_type = "Belady", .cache_size=100,
                .expected_bhr=0.5843178254051229, .expected_ohr=0.6003621807091117},
        {.trace_file = "../../test/test.tr", .cache_type = "Belady", .cache_size=200,
                .expected_bhr=0.685572399372713, .expected_ohr=0.6991040792985131},
        {.trace_file = "../../test/test.tr", .cache_type = "Belady", .cache_size=500,
                .expected_bhr=0.8112911657083115, .expected_ohr=0.8205299275638582},
        {.trace_file = "../../test/test.tr", .cache_type = "Belady", .cache_size=1000,
                .expected_bhr=0.8855201254573968, .expected_ohr=0.8845787266488754},
        {.trace_file = "../../test/test.tr", .cache_type = "Belady", .cache_size=2000,
                .expected_bhr=0.9093047569262938, .expected_ohr=0.9085017155928327},
};

TEST_CASE("webcachesimBelady") {
    webcachesim_test(inputBelady);
}

InputT inputFIFO[] = {
        {.trace_file = "../../test/test.tr", .cache_type = "FIFO", .cache_size=1,
                .expected_bhr=0.009775222164140094, .expected_ohr=0.01782310331681281},
        {.trace_file = "../../test/test.tr", .cache_type = "FIFO", .cache_size=2,
                .expected_bhr=0.015577626764244642, .expected_ohr=0.026972931757529545},
        {.trace_file = "../../test/test.tr", .cache_type = "FIFO", .cache_size=5,
                .expected_bhr=0.03863042341871406, .expected_ohr=0.050895920701486845},
        {.trace_file = "../../test/test.tr", .cache_type = "FIFO", .cache_size=10,
                .expected_bhr=0.07360167276529012, .expected_ohr=0.08377811666031262},
        {.trace_file = "../../test/test.tr", .cache_type = "FIFO", .cache_size=20,
                .expected_bhr=0.13068478829064298, .expected_ohr=0.13667556233320624},
        {.trace_file = "../../test/test.tr", .cache_type = "FIFO", .cache_size=50,
                .expected_bhr=0.232723470987977, .expected_ohr=0.23770491803278687},
        {.trace_file = "../../test/test.tr", .cache_type = "FIFO", .cache_size=100,
                .expected_bhr=0.3238369053842133, .expected_ohr=0.32901258101410596},
        {.trace_file = "../../test/test.tr", .cache_type = "FIFO", .cache_size=200,
                .expected_bhr=0.4341871406168322, .expected_ohr=0.4442432329393824},
        {.trace_file = "../../test/test.tr", .cache_type = "FIFO", .cache_size=500,
                .expected_bhr=0.6151594354417146, .expected_ohr=0.6239992375142966},
        {.trace_file = "../../test/test.tr", .cache_type = "FIFO", .cache_size=1000,
                .expected_bhr=0.7763721902770517, .expected_ohr=0.7821197102554327},
        {.trace_file = "../../test/test.tr", .cache_type = "FIFO", .cache_size=2000,
                .expected_bhr=0.9093047569262938, .expected_ohr=0.9085017155928327},
};

TEST_CASE("webcachesimFIFO") {
    webcachesim_test(inputFIFO);
}