#include <unordered_map>
#include <random>
#include <cmath>
#include <cassert>
#include "lhd_variants.h"
#include "cache.hpp"
#include "repl.hpp"
#include "parser.hpp"
#include "lhd.hpp"
#include "constants.hpp"

/*
  LHD variants impl
*/

LHD::LHD()
{
    lhdcache = new cache::Cache();

    lhdcache->repl = new repl::LHD(assoc, admissionSamples, lhdcache);
}

void LHD::setSize(const uint64_t &cs) {
    _cacheSize = cs;
    lhdcache->availableCapacity = cs;
}

bool LHD::lookup(SimpleRequest& req)
{
    // fixme -> app id
    //    const parser::PartialRequest preq {1, (int64_t)req.get_size(), (int64_t)req.get_id()};
    // pr.appId - 1
    const parser::Request preq { 0., 1, parser::GET, 0, (int64_t)req.get_size(), (int64_t)req.get_id(), false };
    return(lhdcache->access(preq));
}

void LHD::admit(SimpleRequest& req)
{
    // nop
}

void LHD::evict()
{
    // nop
}
