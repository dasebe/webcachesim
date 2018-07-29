#include <cmath>
#include <iomanip>
#include "adaptsize_stub.h"
#include "adaptsize_const.h"

// golden section search helpers
#define SHFT2(a,b,c) (a)=(b);(b)=(c);
#define SHFT3(a,b,c,d) (a)=(b);(b)=(c);(c)=(d);
static const int maxIterations = 15;
const double r = 0.61803399;
double v;
const double tol = 3.0e-8;

// init (needs to be updated)
AdaptSize::AdaptSize(uint64_t _cacheSize)
  : nextReconfiguration(RECONFIGURATION_INTERVAL)
  , c(1 << 15)
  , cacheSize((double)_cacheSize)
  , statSize(0) {
    v=1.0-r;
}
AdaptSize::~AdaptSize() {
}

// should be "lookup" in webcachesim, needs to be split into admit...
// called whenever a request comes in
void AdaptSize::request(candidate_t id, const parser::Request& req) {
  reconfigure();

  auto* entry = tags.lookup(id);
  if (entry) {
    // hit
    assert(entry->data == id);
    entry->remove();
    list.insert_front(entry);
  } else {
    // miss -- admit?
    entry = tags.allocate(id, id);
    if (admit(req.size())) {
      list.insert_front(entry);
    } else {
      list.insert_back(entry);
    }
  }

  if(intervalInfo.count(id)==0 && ewmaInfo.count(id)==0) {
      // new object
      statSize += req.size();
  } else {
      // keep track of changing object sizes
      if(intervalInfo.count(id)>0 && intervalInfo[id].size != req.size()) {
          // outdated size info in intervalInfo
          statSize -= intervalInfo[id].size;
          statSize += req.size();
      }
      if(ewmaInfo.count(id)>0 && ewmaInfo[id].size != req.size()) {
          // outdated size info in ewma
          statSize -= ewmaInfo[id].size;
          statSize += req.size();
      }
  }

  // record stats
  auto& info = intervalInfo[id];
  info.requestRate += 1;
  info.size = req.size();
}

void AdaptSize::setSize(uint64_t cs) {
    cacheSize = (double)cs;
}

// admit test function, needs to be extended to take SimpleRequest in webcachesim
bool AdaptSize::admit(int64_t size) {
  const uint64_t RANGE = 1ull << 32;
  double roll = (rand.next() % RANGE) * 1. / RANGE;
  double admitProb = std::exp(-size / c);

  return roll < admitProb;
}

// main AdaptSize code: called every RECONFIGURATION_INTERVAL
// calculates optimal c parameter
// needs to be minimally adapted for webcachesim

void AdaptSize::reconfigure() {
  --nextReconfiguration;
  if (nextReconfiguration > 0) {
    return;
  } else if(statSize <= cacheSize*3) {
      // not enough data has been gathered
      nextReconfiguration+=10000;
      return ;
  } else {
    nextReconfiguration = RECONFIGURATION_INTERVAL;
  }

  // smooth stats for objects
  for (auto it = ewmaInfo.begin();
       it != ewmaInfo.end();
       it++) {
    it->second.requestRate *= EWMA_DECAY;
  }

  // persist intervalinfo in ewmaInfo 
  for (auto it = intervalInfo.begin();
       it != intervalInfo.end();
       it++) {
    auto ewmaIt = ewmaInfo.find(it->first);
    if (ewmaIt != ewmaInfo.end()) {
      ewmaIt->second.requestRate += (1. - EWMA_DECAY) * it->second.requestRate;
      ewmaIt->second.size = it->second.size;
    } else {
      ewmaInfo.insert(*it);
    }
  }
  intervalInfo.clear();


  // copy stats into vector for better alignment
  // and delete small values
  alignedReqRate.clear();
  alignedObjSize.clear();
  double totalReqRate = 0.0;
  uint64_t totalObjSize = 0.0;
  for (auto it = ewmaInfo.begin();
       it != ewmaInfo.end();
       /*none*/) {
    if (it->second.requestRate < 0.1) {
      // delete from stats
      statSize -= it->second.size;
      it = ewmaInfo.erase(it);
    } else {
      alignedReqRate.push_back(it->second.requestRate);
      totalReqRate += it->second.requestRate;
      alignedObjSize.push_back(it->second.size);
      totalObjSize += it->second.size;
      ++it;
    }
  }

  std::cerr << "Reconfiguring over " << ewmaInfo.size() << " objects - log2 total size " << std::log2(totalObjSize) << " log2 statsize " << std::log2(statSize) << std::endl;

  //  assert(totalObjSize==statSize);

  //  if(totalObjSize > cacheSize*2) {

  // model hit rate and choose best admission parameter, c
  // search for best parameter on log2 scale of c, between min=x0 and max=x3
  // x1 and x2 bracket our current estimate of the optimal parameter range
  // |x0 -- x1 -- x2 -- x3|
  double x0 = 0;
  double x1 = std::log2(cacheSize);
  double x2 = x1;
  double x3 = x1;

  double bestHitRate = 0.0;
  // coarse-granular grid search
  for (int i = 2; i < x3; i+=4) {
    const double next_log2c = i; // 1.0 * (i+1) / NUM_PARAMETER_POINTS;
    const double hitRate = modelHitRate(next_log2c);
    //    printf("Model param (%f) : ohr (%f)\n",next_log2c,hitRate/totalReqRate);
    
    if (hitRate > bestHitRate) {
      bestHitRate = hitRate;
      x1 = next_log2c;
    }
  }


  double h1 = bestHitRate;
  double h2;
  // prepare golden section search into larger segment
  if(x3-x1 > x1-x0) {
    // above x1 is larger segment
    x2 = x1+v*(x3-x1);
    h2 = modelHitRate(x2);
  } else {
    //below x1 is larger segment
    x2 = x1;
    h2 = h1;
    x1 = x0+v*(x1-x0);
    h1 = modelHitRate(x1);
  }
  assert(x1<x2);

  int curIterations=0;
  // use termination condition from [Numerical recipes in C] 
  while (curIterations++<maxIterations && fabs(x3-x0) > tol*(fabs(x1)+fabs(x2))) { 
    //NAN check
    if( (h1!=h1) || (h2!=h2) ) 
      break;
    //    printf("Model param low (%f) : ohr low (%f) | param high (%f) : ohr high (%f)\n",x1,h1/totalReqRate,x2,h2/totalReqRate);
    if (h2 > h1) {
      SHFT3(x0,x1,x2,r*x1+v*x3);
      SHFT2(h1,h2,modelHitRate(x2));
    } else {
      SHFT3(x3,x2,x1,r*x2+v*x0);
      SHFT2(h2,h1,modelHitRate(x1));
    }
  }

  // check result
  if( (h1!=h1) || (h2!=h2) ) {
    // numerical failure
      std::cerr << "ERROR: numerical bug " << h1 << " " << h2 << std::endl;
    // nop
  } else if (h1 > h2) {
    // x1 should is final parameter
    c = pow(2, x1);
    std::cerr << "Choosing c of " << c << " (log2: " << x1 << ")" << std::endl;
  } else {
    c = pow(2, x2);
    std::cerr << "Choosing c of " << c << " (log2: " << x2 << ")" << std::endl;
  }

}

// math model below can be directly copiedx
static inline double oP1(double T, double l, double p) {
  return (l * p * T * (840.0 + 60.0 * l * T + 20.0 * l*l * T*T + l*l*l * T*T*T));
}

static inline double oP2(double T, double l, double p) {
  return (840.0 + 120.0 * l * (-3.0 + 7.0 * p) * T + 60.0 * l*l * (1.0 + p) * T*T + 4.0 * l*l*l * (-1.0 + 5.0 * p) * T*T*T + l*l*l*l * p * T*T*T*T);
}

double AdaptSize::modelHitRate(double log2c) {
  // this code is adapted from the AdaptSize git repo
  // github.com/dasebe/AdaptSize
  double old_T, the_T, the_C;
  double sum_val = 0.;
  double thparam = log2c;

  for(size_t i=0; i<alignedReqRate.size(); i++) {
    sum_val += alignedReqRate[i] * (exp(-alignedObjSize[i]/ pow(2,thparam))) * alignedObjSize[i];
  }
  if(sum_val <= 0) {
    return(0);
  }
  the_T = cacheSize / sum_val;
  // prepare admission probabilities
  alignedAdmProb.clear();
  for(size_t i=0; i<alignedReqRate.size(); i++) {
      alignedAdmProb.push_back(exp(-alignedObjSize[i]/ pow(2.0,thparam)));
  }
  // 20 iterations to calculate TTL
  
  for(int j = 0; j<10; j++) {
    the_C = 0;
    if(the_T > 1e70) {
      break;
    }
    for(size_t i=0; i<alignedReqRate.size(); i++) {
      const double reqTProd = alignedReqRate[i]*the_T;
      if(reqTProd>150) {
          // cache hit probability = 1, but numerically inaccurate to calculate
          the_C += alignedObjSize[i];
      } else {
          const double expTerm = exp(reqTProd) - 1;
          const double expAdmProd = alignedAdmProb[i] * expTerm;
          const double tmp = expAdmProd / (1 + expAdmProd);
          the_C += alignedObjSize[i] * tmp;
      }
    }
    old_T = the_T;
    the_T = cacheSize * old_T/the_C;
  }

  // calculate object hit ratio
  double weighted_hitratio_sum = 0;
  for(size_t i=0; i<alignedReqRate.size(); i++) {
      const double tmp01= oP1(the_T,alignedReqRate[i],alignedAdmProb[i]);
      const double tmp02= oP2(the_T,alignedReqRate[i],alignedAdmProb[i]);
      double tmp;
      if(tmp01!=0 && tmp02==0)
          tmp = 0.0;
      else tmp= tmp01/tmp02;
      if(tmp<0.0)
          tmp = 0.0;
      else if (tmp>1.0)
          tmp = 1.0;
      weighted_hitratio_sum += alignedReqRate[i] * tmp;
  }
  return (weighted_hitratio_sum);
}
