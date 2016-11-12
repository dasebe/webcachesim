#include <unordered_map>
#include <map>
#include <unordered_set>
#include <vector>
#include <queue>
#include <list>
#include <tuple>
#include <assert.h>
#include <random>
#include "cache.h"

typedef multimap<long double, object_t>::iterator map_iterator_t;


/*
  GD: greedy dual eviction (base class)

  [implementation via heap: O(log n) time for each cache miss]
*/

class GreedyDualBase: public Cache {
protected:
  // the GD current value
  long double current_L=0;
  // ordered multi map of GD values, access object id + size
  multimap<long double, object_t> value_map;
  // find objects via unordered_map
  unordered_map<long, map_iterator_t> cache_map;

  virtual long double agevalue(const long cur_req, const long size) {
    return(current_L + 1);
  }

  virtual void hit(const long cur_req, const long long size) {
    // get iterator for the old position
    map_iterator_t si = cache_map[cur_req];
    // update current req's value to hval:
    value_map.erase(si);
    long double hval = agevalue(cur_req, size);
    cache_map[cur_req] = value_map.insert( pair<long double, object_t>(hval, object_t(cur_req, size)) );
    // done - gather statistics
    Cache::hit(size);
  }

  virtual void miss(const long cur_req, const long long size) {
    // object feasible to store?
    if(size >= cache_size) {
      LOG("error", 0, size, cache_size);
      return;
    }
    map_iterator_t lit;
    // check eviction needed
    while (current_size + size> cache_size) {
      // evict first list element (sorted list -> smallest value)
      lit = value_map.begin(); // something wrong here: previously: ++value_map.begin()
      if(lit == value_map.end())
	cerr << "underun: " << current_size << ' ' << size << ' ' << cache_size << "\n";
      assert(lit != value_map.end()); // bug if this happens
      object_t obj = lit->second;
      LOG("e", lit->first, get<0>( obj ), get<1>( obj ));
      cache_map.erase(get<0>( obj ));
      current_size -= get<1>( obj );
      // update L
      current_L = lit->first;
      value_map.erase(lit);
    }
    // admit new object with new GF value
    long double aval = agevalue(cur_req, size);
    LOG("a",cur_req,aval,size);
    cache_map[cur_req] =  value_map.insert( pair<long double, object_t>(aval, object_t(cur_req, size)) );
    current_size += size;
  }

public:
  GreedyDualBase(): Cache() {}
  ~GreedyDualBase(){}

  bool lookup (const long cur_req) const {
    return(cache_map.count(cur_req)>0);
  }

  virtual void evict (const long cur_req) {
    if(lookup(cur_req)) {
      map_iterator_t lit = cache_map[cur_req];
      object_t obj = lit->second;
      cache_map.erase(cur_req);
      current_size -= get<1>(obj);
      value_map.erase(lit);
    }
  }

  virtual bool request (const long cur_req, long size) {
    if (cache_map.count(cur_req) > 0) {
	if (size == get<1>(get<1>(*(cache_map[cur_req]))) ) {
	  // hit
	  LOG("h",0,cur_req,size);
	  hit(cur_req, size);
	  return(true);
	} else {
	  // inconsistent size -> treat as miss and delete inconsistent entry
	  evict(cur_req);
	  //	  cerr << "deleted outdated object" << endl;
	}
    }
    miss(cur_req, size);
    return(false);
  }
};
static Factory<GreedyDualBase> factoryGD("GD");




/*
  Greedy Dual Size policy
*/

class GDSCache: public GreedyDualBase {
protected:
  long double agevalue(const long cur_req, const long size) {
    return(current_L + 1/double(size));
  }

public:
  GDSCache(): GreedyDualBase() {}
};
static Factory<GDSCache> factoryGDS("GDS");


/*
  Greedy Dual Size Frequency policy
*/

class GDSFCache: public GreedyDualBase {
protected:
  unordered_map<long, long long> reqs_map;

  long double agevalue(const long cur_req, const long size) {
    return(current_L + reqs_map[cur_req]/double(size));
  }

public:
  GDSFCache(): GreedyDualBase() {}

  ~GDSFCache(){}

  bool request (const long cur_req, const long size) {
    if (cache_map.count(cur_req) > 0) {
      if (size == get<1>(get<1>(*(cache_map[cur_req]))) ) {
	// hit and consistent object size
	LOG("h",0,cur_req,size);
	reqs_map[cur_req]++;
	hit(cur_req, size);
	return(true);
      } else {
	// inconsistent size -> treat as miss and delete inconsistent entry
	evict(cur_req);
	//	cerr << "deleted outdated object" << endl;
      }
    }
    reqs_map[cur_req]=1; //reset bec. reqs_map not updated when element removed
    miss(cur_req, size);
    return(false);
  }
};
static Factory<GDSFCache> factoryGDSF("GDSF");




/*
  LRU-K policy
*/

class LRUKCache: public GreedyDualBase {
protected:
  unordered_map<long, queue<unsigned long>> refs_map;
  unsigned int tk;
  unsigned long curtime;
  long double agevalue(const long cur_req, const long size) {
    long double newval = 0.0L;
    if(refs_map[cur_req].size()>=tk) {
	newval = refs_map[cur_req].front();
	refs_map[cur_req].pop();
    }
    //    cerr << cur_req << " " << curtime << " " << refs_map[cur_req].size() << " " << newval << " " << current_L << endl;
    return(newval);
  }

  void miss(const long cur_req, const long long size) {
    // object feasible to store?
    if(size >= cache_size) {
      LOG("error", 0, size, cache_size);
      return;
    }
    map_iterator_t lit;
    // check eviction needed
    while (current_size + size> cache_size) {
      // evict first list element (sorted list -> smallest value)
      lit = value_map.begin(); // something wrong here: previously: ++value_map.begin()
      if(lit == value_map.end())
	cerr << "underun: " << current_size << ' ' << size << ' ' << cache_size << "\n";
      assert(lit != value_map.end()); // bug if this happens
      object_t obj = lit->second;
      LOG("e", lit->first, get<0>( obj ), get<1>( obj ));
      cache_map.erase(get<0>( obj ));
      refs_map.erase(get<0>( obj )); // delete LRU-K info
      current_size -= get<1>( obj );
      // update L
      current_L = lit->first;
      value_map.erase(lit);
    }
    // admit new object with new GF value
    long double aval = agevalue(cur_req, size);
    LOG("a",cur_req,aval,size);
    cache_map[cur_req] =  value_map.insert( pair<long double, object_t>(aval, object_t(cur_req, size)) );
    current_size += size;
  }


public:
  LRUKCache(): GreedyDualBase(), tk(2), curtime(0) {}
  ~LRUKCache() {}

  virtual void setPar(string parName, string parValue) {
    if(parName=="k") {
      const int k = stoi(parValue);
      assert(k>0);
      tk = k;
    } else {
      cerr << "unrecognized parameter: " << parName << endl;
    }
  }

  bool request (const long cur_req, const long size) {
    curtime++;
    if (cache_map.count(cur_req) > 0) {
      if (size == get<1>(get<1>(*(cache_map[cur_req]))) ) {
	// hit and consistent object size
	LOG("h",0,cur_req,size);
	refs_map[cur_req].push(curtime);
	hit(cur_req, size);
	return(true);
      } else {
	// inconsistent size -> treat as miss and delete inconsistent entry
	evict(cur_req);
	//	cerr << "deleted outdated object" << endl;
      }
    }
    refs_map[cur_req].push(curtime);
    miss(cur_req, size);
    return(false);
  }

};
static Factory<LRUKCache> factoryLRUK("LRUK");


/*
  LFUDA
*/

class LFUDACache: public GreedyDualBase {
protected:
  unordered_map<long, long long> reqs_map;

  long double agevalue(const long cur_req, const long size) {
    return(current_L + reqs_map[cur_req]);
  }

public:
  LFUDACache(): GreedyDualBase() {}
  ~LFUDACache(){}

  bool request (const long cur_req, const long size) {
    if (cache_map.count(cur_req) > 0) {
      if (size == get<1>(get<1>(*(cache_map[cur_req]))) ) {
	// hit and consistent object size
	LOG("h",0,cur_req,size);
	reqs_map[cur_req]++;
	hit(cur_req, size);
	return(true);
      } else {
	// inconsistent size -> treat as miss and delete inconsistent entry
	evict(cur_req);
	//	cerr << "deleted outdated object" << endl;
      }
    }
    reqs_map[cur_req]=1; //reset bec. reqs_map not updated when element removed
    miss(cur_req, size);
    return(false);
  }
};
static Factory<LFUDACache> factoryLFUDA("LFUDA");
