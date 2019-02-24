#pragma once

#include <stdint.h>

const int MONITOR = 0;

namespace cache_competitors {

const uint64_t STATS_INTERVAL = 1000000;

}

namespace libconfig {
  class Setting;
}

namespace parser_competitors{
  const uint64_t FAST_FORWARD = 0;
}

namespace repl_competitors {

namespace fn_competitors {

namespace eva {

const uint64_t ACCS_PER_INTERVAL = 1024000;
const float EWMA_DECAY = 0.9;
const uint64_t MAX_AGE = 25600;
const uint64_t AGE_COARSENING_DIVISOR = 10;
const bool FULL_DEBUG_INFO = false;

}

namespace hitdensity_competitors {

void initParameters();

extern uint64_t ACCS_PER_INTERVAL;
extern float EWMA_DECAY;
extern float AGE_COARSENING_ERROR_TOLERANCE;
extern bool FULL_DEBUG_INFO;

extern uint64_t MAX_SIZE;
extern int SIZE_CLASSES;
extern uint32_t MAX_REFERENCES;
extern uint32_t APP_CLASSES;

}

namespace adaptsize_competitors {

const int RECONFIGURATION_INTERVAL = 500000;
const double EWMA_DECAY = 0.3;

}

}
}

