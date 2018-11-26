//
// Created by Zhenyu Song on 10/30/18.
//
#define CATCH_CONFIG_MAIN  // This tells Catch to provide a main() - only do this in one cpp file
#include "catch.hpp"
#include "simulation.h"

using namespace std;

struct InputT{
    string trace_file;
    string cache_type;
    uint64_t cache_size;
    map<string, string> params;
    double expected_bhr;
    double expected_ohr;
};


#define webcachesim_test(input, _margin) \
    for (auto& it : input) { \
        auto res = simulation(it.trace_file, it.cache_type, it.cache_size, it.params); \
        REQUIRE( stod(res["byte_hit_rate"]) == Approx(it.expected_bhr).margin(_margin)); \
        REQUIRE( stod(res["object_hit_rate"]) == Approx(it.expected_ohr).margin(_margin)); \
    }

InputT inputLRU[] = {
        {.trace_file = "../../test/test.tr", .cache_type = "LRU", .cache_size=1, .params={},
                .expected_bhr=0.009775222164140094, .expected_ohr=0.01782310331681281},
        {.trace_file = "../../test/test.tr", .cache_type = "LRU", .cache_size=2, .params={},
                .expected_bhr=0.015734448510193413, .expected_ohr=0.027258863896301944},
        {.trace_file = "../../test/test.tr", .cache_type = "LRU", .cache_size=5, .params={},
                .expected_bhr=0.039309984317825404, .expected_ohr=0.05213495996950057},
        {.trace_file = "../../test/test.tr", .cache_type = "LRU", .cache_size=10, .params={},
                .expected_bhr=0.07684265551489806, .expected_ohr=0.08882958444529165},
        {.trace_file = "../../test/test.tr", .cache_type = "LRU", .cache_size=20, .params={},
                .expected_bhr=0.14694197595399897, .expected_ohr=0.15449866565001907},
        {.trace_file = "../../test/test.tr", .cache_type = "LRU", .cache_size=50, .params={},
                .expected_bhr=0.27083115525352847, .expected_ohr=0.2750667174990469},
        {.trace_file = "../../test/test.tr", .cache_type = "LRU", .cache_size=100, .params={},
                .expected_bhr=0.36999477260846836, .expected_ohr=0.37552420892108274},
        {.trace_file = "../../test/test.tr", .cache_type = "LRU", .cache_size=200, .params={},
                .expected_bhr=0.48133821223209616, .expected_ohr=0.4959016393442623},
        {.trace_file = "../../test/test.tr", .cache_type = "LRU", .cache_size=500, .params={},
                .expected_bhr=0.658337689492943, .expected_ohr=0.6698436904308044},
        {.trace_file = "../../test/test.tr", .cache_type = "LRU", .cache_size=1000, .params={},
                .expected_bhr=0.8051751176163094, .expected_ohr=0.809664506290507},
        {.trace_file = "../../test/test.tr", .cache_type = "LRU", .cache_size=2000, .params={},
                .expected_bhr=0.9093047569262938, .expected_ohr=0.9085017155928327},
};

TEST_CASE("webcachesimLRU") {
    webcachesim_test(inputLRU, 0.00001)
}


InputT inputFIFO[] = {
        {.trace_file = "../../test/test.tr", .cache_type = "FIFO", .cache_size=1, .params={},
                .expected_bhr=0.009775222164140094, .expected_ohr=0.01782310331681281},
        {.trace_file = "../../test/test.tr", .cache_type = "FIFO", .cache_size=2, .params={},
                .expected_bhr=0.015577626764244642, .expected_ohr=0.026972931757529545},
        {.trace_file = "../../test/test.tr", .cache_type = "FIFO", .cache_size=5, .params={},
                .expected_bhr=0.03863042341871406, .expected_ohr=0.050895920701486845},
        {.trace_file = "../../test/test.tr", .cache_type = "FIFO", .cache_size=10, .params={},
                .expected_bhr=0.07360167276529012, .expected_ohr=0.08377811666031262},
        {.trace_file = "../../test/test.tr", .cache_type = "FIFO", .cache_size=20, .params={},
                .expected_bhr=0.13068478829064298, .expected_ohr=0.13667556233320624},
        {.trace_file = "../../test/test.tr", .cache_type = "FIFO", .cache_size=50, .params={},
                .expected_bhr=0.232723470987977, .expected_ohr=0.23770491803278687},
        {.trace_file = "../../test/test.tr", .cache_type = "FIFO", .cache_size=100, .params={},
                .expected_bhr=0.3238369053842133, .expected_ohr=0.32901258101410596},
        {.trace_file = "../../test/test.tr", .cache_type = "FIFO", .cache_size=200, .params={},
                .expected_bhr=0.4341871406168322, .expected_ohr=0.4442432329393824},
        {.trace_file = "../../test/test.tr", .cache_type = "FIFO", .cache_size=500, .params={},
                .expected_bhr=0.6151594354417146, .expected_ohr=0.6239992375142966},
        {.trace_file = "../../test/test.tr", .cache_type = "FIFO", .cache_size=1000, .params={},
                .expected_bhr=0.7763721902770517, .expected_ohr=0.7821197102554327},
        {.trace_file = "../../test/test.tr", .cache_type = "FIFO", .cache_size=2000, .params={},
                .expected_bhr=0.9093047569262938, .expected_ohr=0.9085017155928327},
};

TEST_CASE("webcachesimFIFO") {
    webcachesim_test(inputFIFO, 0.00001);
}

InputT inputBelady[] = {
        {.trace_file = "../../test/test.tr", .cache_type = "Belady", .cache_size=1, .params={},
                .expected_bhr=0.05781495033978045, .expected_ohr=0.10541364849409074},
        {.trace_file = "../../test/test.tr", .cache_type = "Belady", .cache_size=2, .params={},
                .expected_bhr=0.08724516466283325, .expected_ohr=0.15268776210446053},
        {.trace_file = "../../test/test.tr", .cache_type = "Belady", .cache_size=5, .params={},
                .expected_bhr=0.15446941975953998, .expected_ohr=0.20301181852840258},
        {.trace_file = "../../test/test.tr", .cache_type = "Belady", .cache_size=10, .params={},
                .expected_bhr=0.2740198640878202, .expected_ohr=0.2919367136866184},
        {.trace_file = "../../test/test.tr", .cache_type = "Belady", .cache_size=20, .params={},
                .expected_bhr=0.36408782017773134, .expected_ohr=0.3844834159359512},
        {.trace_file = "../../test/test.tr", .cache_type = "Belady", .cache_size=50, .params={},
                .expected_bhr=0.48285415577626767, .expected_ohr=0.5090545177277926},
        {.trace_file = "../../test/test.tr", .cache_type = "Belady", .cache_size=100, .params={},
                .expected_bhr=0.5843178254051229, .expected_ohr=0.6003621807091117},
        {.trace_file = "../../test/test.tr", .cache_type = "Belady", .cache_size=200, .params={},
                .expected_bhr=0.685572399372713, .expected_ohr=0.6991040792985131},
        {.trace_file = "../../test/test.tr", .cache_type = "Belady", .cache_size=500, .params={},
                .expected_bhr=0.8112911657083115, .expected_ohr=0.8205299275638582},
        {.trace_file = "../../test/test.tr", .cache_type = "Belady", .cache_size=1000, .params={},
                .expected_bhr=0.8855201254573968, .expected_ohr=0.8845787266488754},
        {.trace_file = "../../test/test.tr", .cache_type = "Belady", .cache_size=2000, .params={},
                .expected_bhr=0.9093047569262938, .expected_ohr=0.9085017155928327},
};

TEST_CASE("webcachesimBelady") {
    webcachesim_test(inputBelady, 0);
}

InputT inputGDSF[] = {
        {.trace_file = "../../test/test.tr", .cache_type = "GDSF", .cache_size=50, .params={},
                .expected_bhr=0.35561944589649763, .expected_ohr=0.4109797941288601},
        {.trace_file = "../../test/test.tr", .cache_type = "GDSF", .cache_size=100, .params={},
                .expected_bhr=0.41338212232096183, .expected_ohr=0.49571101791841404},
        {.trace_file = "../../test/test.tr", .cache_type = "GDSF", .cache_size=200, .params={},
                .expected_bhr=0.4970726607422896, .expected_ohr=0.6010293556995806},
        {.trace_file = "../../test/test.tr", .cache_type = "GDSF", .cache_size=500, .params={},
                .expected_bhr=0.6610559330893884, .expected_ohr=0.7553373999237515},
        {.trace_file = "../../test/test.tr", .cache_type = "GDSF", .cache_size=1000, .params={},
                .expected_bhr=0.8143753267119708, .expected_ohr=0.8672321768966832},
};

TEST_CASE("webcachesimGDSF") {
    webcachesim_test(inputGDSF, 0.01);
}

InputT inputUnitSize[] = {
        {.trace_file = "../../test/test.tr", .cache_type = "LRU", .cache_size=1, .params={{"uni_size", "1"}},
                .expected_bhr=0.01782310331681281, .expected_ohr=0.01782310331681281},
        {.trace_file = "../../test/test.tr", .cache_type = "LRU", .cache_size=10, .params={{"uni_size", "1"}},
                .expected_bhr=0.13963019443385435, .expected_ohr=0.13963019443385435},
        {.trace_file = "../../test/test.tr", .cache_type = "LRU", .cache_size=100, .params={{"uni_size", "1"}},
                .expected_bhr=0.47960350743423563, .expected_ohr=0.47960350743423563},
        {.trace_file = "../../test/test.tr", .cache_type = "LRU", .cache_size=1000, .params={{"uni_size", "1"}},
                .expected_bhr=0.9085017155928327, .expected_ohr=0.9085017155928327},
};

TEST_CASE("webcachesimUnitSize") {
    webcachesim_test(inputUnitSize, 0);
}

InputT inputWarmup[] = {
        {.trace_file = "../../test/test.tr", .cache_type = "LRU", .cache_size=100, .params={{"n_warmup", "10"}},
                .expected_bhr=0.37005442746493616, .expected_ohr=0.3755008586147682},
        {.trace_file = "../../test/test.tr", .cache_type = "LRU", .cache_size=100, .params={{"n_warmup", "100"}},
                .expected_bhr=0.3711585172668708, .expected_ohr=0.37644341801385683},
        {.trace_file = "../../test/test.tr", .cache_type = "LRU", .cache_size=100, .params={{"n_warmup", "1000"}},
                .expected_bhr=0.37391555812608446, .expected_ohr=0.3782132321955331},
        {.trace_file = "../../test/test.tr", .cache_type = "LRU", .cache_size=100, .params={{"n_warmup", "10000"}},
                .expected_bhr=0.40046565774155995, .expected_ohr=0.3983739837398374},
};

TEST_CASE("webcachesimWarmup") {
    webcachesim_test(inputWarmup, 0);
}

InputT inputBeladyUnitSize[] = {
        {.trace_file = "../../test/test.tr", .cache_type = "Belady", .cache_size=1, .params={{"uni_size", "1"}},
                .expected_bhr=0.11799466260007625, .expected_ohr=0.11799466260007625},
        {.trace_file = "../../test/test.tr", .cache_type = "Belady", .cache_size=10, .params={{"uni_size", "1"}},
                .expected_bhr=0.36913839115516583, .expected_ohr=0.36913839115516583},
        {.trace_file = "../../test/test.tr", .cache_type = "Belady", .cache_size=100, .params={{"uni_size", "1"}},
                .expected_bhr=0.6897636294319481, .expected_ohr=0.6897636294319481},
        {.trace_file = "../../test/test.tr", .cache_type = "Belady", .cache_size=1000, .params={{"uni_size", "1"}},
                .expected_bhr=0.9085017155928327, .expected_ohr=0.9085017155928327},
};

TEST_CASE("webcachesimBeladyUnitSize") {
    webcachesim_test(inputBeladyUnitSize, 0);
}

InputT inputBeladyWarmup[] = {
        {.trace_file = "../../test/test.tr", .cache_type = "Belady", .cache_size=100, .params={{"n_warmup", "10"}},
                .expected_bhr=0.5846242411555369, .expected_ohr=0.6005533295172677},
        {.trace_file = "../../test/test.tr", .cache_type = "Belady", .cache_size=100, .params={{"n_warmup", "100"}},
                .expected_bhr=0.5876016474812547, .expected_ohr=0.6033487297921478},
        {.trace_file = "../../test/test.tr", .cache_type = "Belady", .cache_size=100, .params={{"n_warmup", "1000"}},
                .expected_bhr=0.5909774436090226, .expected_ohr=0.6045090602612726},
        {.trace_file = "../../test/test.tr", .cache_type = "Belady", .cache_size=100, .params={{"n_warmup", "10000"}},
                .expected_bhr=0.6216530849825378, .expected_ohr=0.6097560975609756},
};

TEST_CASE("webcachesimBeladyWarmup") {
    webcachesim_test(inputBeladyWarmup, 0);
}
