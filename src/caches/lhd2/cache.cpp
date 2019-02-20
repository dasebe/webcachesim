#include <ctime>
#include <string>
#include <libconfig.h++>

#include "bytes.hpp"
#include "parser.hpp"
#include "repl.hpp"
#include "cache.hpp"
#include "config.hpp"

using namespace std;
using namespace parser;

cache::Cache* _cache;

const int64_t DEFAULT_TOTAL_ACCESSES = 512 * 1024 * 1024;
uint64_t TOTAL_ACCESSES = DEFAULT_TOTAL_ACCESSES;

int32_t filterApp = -1;

const string MSR_TRACE_PREFIX = "/n/memcachier/snia/msr/";
const string FULL_TRACE = "/n/memcachier/full.trace";
const string APP_TRACE_PREFIX = "/n/memcachier/traces/";

bool simulateCache(const Request& req) {
  if (filterApp != -1 && req.appId != filterApp) {
    return true;
  }

  _cache->access(req);
  return _cache->accesses < TOTAL_ACCESSES - parser::FAST_FORWARD;
}

int main(int argc, char* argv[]) {
  if (argc != 2) {
    fprintf(stderr, "Usage: ./cache <config-file>\n");
    exit(-1);
  }

  libconfig::Config cfgFile;

  // Read the file. If there is an error, report it and exit.
  try {
    cfgFile.readFile(argv[1]);
  } catch(const libconfig::FileIOException &fioex) {
    std::cerr << "I/O error while reading file." << std::endl;
    return(EXIT_FAILURE);
  } catch(const libconfig::ParseException &pex) {
    std::cerr << "Parse error at " << pex.getFile() << ":" << pex.getLine()
              << " - " << pex.getError() << std::endl;
    return(EXIT_FAILURE);
  }

  const libconfig::Setting& root = cfgFile.getRoot();
  misc::ConfigReader cfg(root);

  int capacity = cfg.read<int>("cache.capacity");
  TOTAL_ACCESSES = cfg.read<int>("trace.totalAccesses", DEFAULT_TOTAL_ACCESSES);
  _cache = new cache::Cache();
  _cache->availableCapacity = (uint64_t)capacity * 1024 * 1024;
  _cache->repl = repl::Policy::create(_cache, root);
  std::cout << "Cache Capacity: " << capacity << "MB" << std::endl;

  std::string trace;
  if (root.exists("trace.file")) {
    string hostname = cfg.read<const char*>("trace.file");
    if (hostname.compare("memcachier") == 0) {
      trace = FULL_TRACE;
    } else {
      trace = MSR_TRACE_PREFIX + hostname + ".trace";
    }
    std::cout << "trace: " << trace << std::endl;
  } else {
    std::cerr << "Error: No trace file specified in config [trace.file]." 
      << endl;
  }

  /* overwrite the trace file to the single-app file */
  if (cfg.exists("trace.app")) {
    cout << "trace.app exists" << endl;
    auto app = cfg.read<int>("trace.app");
    trace = APP_TRACE_PREFIX + std::to_string(app) + ".trace";
    std::cout << "Filtering apps except " << app << std::endl;
  } 

  std::cout << "Total Requests: " << TOTAL_ACCESSES << std::endl;

  time_t start = time(NULL);

  BinaryParser parser(trace.c_str(), false);
  parser.go(simulateCache);

  time_t end = time(NULL);

  _cache->dumpStats();

  std::cout << "Processed " << _cache->accesses << " in " << (end - start) << " seconds, rate of " << (1. * _cache->accesses / (end - start)) << " accs/sec" << std::endl;

  return 0;
}
