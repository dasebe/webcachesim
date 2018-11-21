#include <unordered_map>
#include <random>
#include <cmath>
#include <cassert>
#include "lhd_variants.h"
#include "cache.hpp"
#include "repl.hpp"
#include "parser.hpp"
#include "lru.hpp"
#include "adaptsize.hpp"
#include "hyperbolic.hpp"
#include "hitdensity.hpp"
#include "gds.hpp"
#include "ranked_lru.hpp"
#include "gdwheel.hpp"
#include "lhd.hpp"
#include "constants.hpp"

/*
  LHD variants impl
*/

LHDBase::LHDBase()
{
    lhdcache = new cache::Cache();
    lhdcache->availableCapacity = (uint64_t)1 * 1024 * 1024;
    lhdcache->repl = new repl::LRU();
}

void LHDBase::setSize(uint64_t cs) {
    _cacheSize = cs;
    lhdcache->availableCapacity = cs;
}

bool LHDBase::lookup(SimpleRequest& req)
{
    const parser::Request preq {1, (int64_t)req.get_size(), (int64_t)req.get_id()};
    return(lhdcache->access(preq));
}

void LHDBase::admit(SimpleRequest& req)
{
    // nop
}

void LHDBase::evict(SimpleRequest& req)
{
    // nop
}

void LHDBase::evict()
{
    // nop
}





LHDSampledLRU::LHDSampledLRU()
{
    lhdcache = new cache::Cache();
    lhdcache->availableCapacity = (uint64_t)1 * 1024 * 1024;
    int assoc = 64;
    int admissionSamples = 8;

    lhdcache->repl = new repl::RankedLRU(assoc, admissionSamples);
}





LHDAdaptSize::LHDAdaptSize()
{
    lhdcache = new cache::Cache();
    lhdcache->availableCapacity = (uint64_t)1 * 1024 * 1024;
    lhdcache->repl = new repl::AdaptSize(lhdcache->availableCapacity);
}

void LHDAdaptSize::setSize(uint64_t cs) {
    auto as = dynamic_cast<repl::AdaptSize*>(lhdcache->repl);
    as->setSize(cs);
    LHDBase::setSize(cs);
}




LHDHyperbolic::LHDHyperbolic()
{
    lhdcache = new cache::Cache();
    lhdcache->availableCapacity = (uint64_t)1 * 1024 * 1024;
    int assoc = 64;
    int admissionSamples = 8;
    lhdcache->repl = new repl::RankedHyperbolicSizeAware(assoc, admissionSamples, lhdcache);
}




LHDSAMPLEDGDSF::LHDSAMPLEDGDSF()
{
    lhdcache = new cache::Cache();
    lhdcache->availableCapacity = (uint64_t)1 * 1024 * 1024;
    int assoc = 64;
    int admissionSamples = 8;
    lhdcache->repl = new repl::RankedGreedyDualSizeFreq(assoc, admissionSamples);
}




LHDHeapGDSF::LHDHeapGDSF()
{
    lhdcache = new cache::Cache();
    lhdcache->availableCapacity = (uint64_t)1 * 1024 * 1024;
    lhdcache->repl = new repl::GreedyDualSizeFreq();
}






LHDBySize::LHDBySize()
{
    lhdcache = new cache::Cache();
    lhdcache->availableCapacity = (uint64_t)1 * 1024 * 1024;
    int assoc = 64;
    int admissionSamples = 8;

    repl::fn::hitdensity::initParameters();

    auto sc = repl::fn::hitdensity::SIZE_CLASSES;
    uint64_t ms = repl::fn::hitdensity::MAX_SIZE;

    lhdcache->repl = new repl::RankedHitDensityBySize(assoc, admissionSamples, lhdcache, sc, ms);

}


LHDByReuse::LHDByReuse()
{
    lhdcache = new cache::Cache();
    lhdcache->availableCapacity = (uint64_t)1 * 1024 * 1024;
    int assoc = 64;
    int admissionSamples = 8;

    repl::fn::hitdensity::initParameters();

    auto mr = repl::fn::hitdensity::MAX_REFERENCES;

    lhdcache->repl = new repl::RankedHitDensityByReuse(assoc, admissionSamples, lhdcache, mr);

}





LHD2::LHD2()
{
    lhdcache = new cache::Cache();
    lhdcache->availableCapacity = (uint64_t)1 * 1024 * 1024;
    lhdcache->repl = new repl::LHD(lhdcache);
}


LHDGDWheel::LHDGDWheel()
{
    lhdcache = new cache::Cache();
    lhdcache->availableCapacity = (uint64_t)1 * 1024 * 1024;
    lhdcache->repl = new repl::GDWheel();
}
