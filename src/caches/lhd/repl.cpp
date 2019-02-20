#include "repl.hpp"
#include "cache.hpp"
#include "constants.hpp"

#include "lhd.hpp"
#include "lru.hpp"

repl::Policy* repl::Policy::create(cache::Cache* cache) {
  misc::ConfigReader cfg(settings);

  std::string type = cfg.read<const char*>("repl.type");
  
  std::cout << "Repl: " << type << std::endl;

  // non-ranking policies
  if (type == "LRU") {
    return new LRU();
  }

  // ranking policies
  int assoc = 64;
  int admissionSamples = 8;

  std::cout << "Ranked associativity = " << assoc << std::endl;

  if (type == "LHD") {
    return new LHD(assoc, admissionSamples, cache);
  } else {
    std::cerr << "No valid policy" << std::endl;
    exit(-2);
    return nullptr;
  }
}
