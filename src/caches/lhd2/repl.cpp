#include "repl.hpp"
#include "cache.hpp"
#include "constants.hpp"

#include "lhd.hpp"
#include "lru.hpp"

#include <libconfig.h++>
#include "config.hpp"

repl::Policy* repl::Policy::create(cache::Cache* cache, const libconfig::Setting &settings) {
  misc::ConfigReader cfg(settings);

  std::string type = cfg.read<const char*>("repl.type");
  
  std::cout << "Repl: " << type << std::endl;

  // non-ranking policies
  if (type == "LRU") {
    return new LRU();
  }

  // ranking policies
  int assoc = cfg.read<int>("cache.assoc");
  int admissionSamples = cfg.read<int>("cache.admissionSamples");

  std::cout << "Ranked associativity = " << assoc << std::endl;

  if (type == "LHD") {
    return new LHD(assoc, admissionSamples, cache);
  } else {
    std::cerr << "No valid policy" << std::endl;
    exit(-2);
    return nullptr;
  }
}
