#include <stdint.h>

const uint64_t STATS_INTERVAL = 1000000;
const int RECONFIGURATION_INTERVAL = 500000;
const double EWMA_DECAY = 0.3;
const uint64_t RANGE = 1ull << 32; 
static const int maxIterations = 15; 
const double gss_r = 0.61803399;
const double tol = 3.0e-8;
