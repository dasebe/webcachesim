#pragma once
#include <iostream>
#include <fstream>
#include <string>
#include <stdint.h>
#include <cassert>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "constants.hpp"

namespace parser {

using std::cout;
using std::cerr;
using std::endl;
using std::flush;
using std::string;
using std::ifstream;

typedef float float32_t;

enum {
  GET = 1,
  SET,
  DELETE,
  ADD,
  INCREMENT,
  STATS,
  OTHER
};

static const int64_t MAX_REQUEST_SIZE = 1024 * 1024;
static const int64_t MEMCACHED_OVERHEAD = 56 + 8 + 8 + 2;

struct Request {
  float32_t time;
  int32_t appId;
  int32_t type;
  int32_t keySize;
  int64_t valueSize;
  int64_t id;
  int8_t  miss;

  inline int64_t size() const { return keySize + valueSize + MEMCACHED_OVERHEAD; }
} __attribute__((packed));

struct MediumRequest {
  float32_t time;
  int32_t appId;
  int32_t type;
  int32_t keySize;
  int64_t valueSize;
  int64_t id;
  int8_t  miss;

  inline int64_t size() const { return keySize + valueSize + MEMCACHED_OVERHEAD; }
} __attribute__((packed));

static constexpr Request NULL_REQUEST{0., 0, 0, 0, 0, 0, false};

struct PartialRequest {
  int32_t appId;
  int64_t size;
  int64_t id;
} __attribute__((packed));

static uint64_t file_size(const char* fname) {
  struct stat64 stats;
  int rc = stat64(fname, &stats);
  return rc == 0? stats.st_size : (uint64_t)-1;
}

class BinaryParser {
public:
  BinaryParser(string filename, bool progressBar = false)
    : file(filename.c_str(), ifstream::in | ifstream::binary), ticks(0) {

    std::cout << "Parsing: " << filename << std::endl;
      
    assert(file.good());

    fileSize = file_size(filename.c_str());

    if (progressBar) {
      struct winsize terminal;
      ioctl(STDOUT_FILENO, TIOCGWINSZ, &terminal);
      if (terminal.ws_col > 0) {
        bytesPerProgressTick = fileSize / terminal.ws_col;
      } else {
        bytesPerProgressTick = -1;
      }
    } else {
      bytesPerProgressTick = -1;
    }

    while (file.good()) {
      char c = '\0';
      file.read(&c, 1);
      header.push_back(c);
      if (c == '!') { break; }
    }
  }

  void go(bool (*visit)(const Request& req)) {
    if (header == "appId.size.id-=iqi!") {
      goPartial(visit);
    } else if (header == "Time.appId.type.keySize.valueSize.id.miss-=fiiiqi?!") {
      goFull<MediumRequest>(visit);
    } else if (header == "Time.appId.type.keySize.valueSize.id.miss-=fiiiqq?!") {
      goFull<Request>(visit);
    } else {
      cerr << "Invalid header in trace: " << header << endl;
      assert(false);
    }

    if (bytesPerProgressTick != -1ull) { cout << endl; }
  }

  void goPartial(bool (*visit)(const Request& req)) {
    cout << "goPartial: Trace file contains " << (fileSize / sizeof(PartialRequest)) << " requests.\n";
    while (file.good()) {
      PartialRequest pr;
      file.read((char*)&pr, sizeof(pr)); // >> r;
      pr.size = std::max(static_cast<uint64_t >(pr.size), static_cast<uint64_t>(1));
      if (pr.size > MAX_REQUEST_SIZE) {
        std::cerr << "Trimming object of size: " << pr.size << std::endl;
        pr.size = MAX_REQUEST_SIZE - MEMCACHED_OVERHEAD;
        assert(pr.size > 0);
      }
      Request req { 0., pr.appId, GET, 0, pr.size, pr.id, false };
      tick();
      if (!visit(req)) { break; }
    }
  }

  template<typename RequestType>
  void goFull(bool (*visit)(const Request& req)) {
    cout << "goFull: Trace file contains " 
         << (fileSize / sizeof(RequestType)) << " requests (each " << sizeof(RequestType) << "B).\n";
    RequestType r;
    std::streampos start = file.tellg();
    while (true) {
      if (!file.good()) {
        file.clear();
        file.seekg(start);
        assert(file.good());
        std::cout << "Reset back to head of file" << std::endl;
      }
      file.read((char*)&r, sizeof(r));
      r.valueSize = std::max(static_cast<uint64_t>(r.valueSize), static_cast<uint64_t>(1));
      if (r.size() > MAX_REQUEST_SIZE) {
        std::cout << "Trimming object of size: " << r.valueSize << std::endl;
        r.valueSize = MAX_REQUEST_SIZE - r.keySize - MEMCACHED_OVERHEAD;
        assert(r.size() > 0);
      }
      Request req { r.time, r.appId, r.type, r.keySize, r.valueSize, r.id, r.miss };
      tick();
      if (!visit(req)) { break; }
    }
  }

private:
  void tick() {
    uint32_t curTicks = file.tellg() / bytesPerProgressTick;
    if (curTicks > ticks) {
      cout << ".";
      cout.flush();
      ticks = curTicks;
    }
  }

  string header;
  ifstream file;
  uint64_t fileSize;
  uint64_t bytesPerProgressTick;
  uint32_t ticks;
};

}
