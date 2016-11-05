#include <iostream>
#include <unordered_map>
#include <map>
#include <unordered_set>
#include <vector>
#include <queue>
#include <list>
#include <tuple>
#include <assert.h>
#include <random>

using namespace std;

typedef tuple<long, long> object_t; // objectid, size
typedef list<object_t>::iterator list_iterator_t;
typedef multimap<long double, object_t>::iterator map_iterator_t;

// set to enable cache debugging
//#define CDEBUG 1

// util for debug
#ifdef CDEBUG
#define LOG(m,x,y,z) log_message(m,x,y,z)
#else
#define LOG(m,x,y,z)
#endif

void log_message(string m, double x, double y, double z) {
  cerr << m << "," << x << "," << y  << "," << z << "\n";
}

/*
  general cache class
*/

class Cache {
protected:
  // caching definitions
  const long long cache_size;
  long long current_size; // size of the cache
  // hit statistics
  long hits;
  long long bytehits;

  bool logStatistics;
  virtual void hit(long size) {
    if(logStatistics) {
      hits++;
      bytehits+=size;
    }
  }

public:
  Cache(long long cs) : cache_size(cs), current_size(0), hits(0), bytehits(0), logStatistics(false) {
  }

  virtual ~Cache(){};

  virtual bool request (const long cur_req, const long long size) {return(false);}

  // statistics
  virtual void startStatistics() {
    logStatistics = true;
  }

  virtual void stopStatistics() {
    logStatistics = false;
  }

  virtual void resetStatistics() {
    hits = 0;
    bytehits = 0;
  }
  
  virtual long getHits() const {
    return(hits);
  }

  virtual bool getLogStatistics() const {
    return(logStatistics);
  }

  virtual long long getBytehits() const {
    return(bytehits);
  }

  virtual long long getCurrentSize() const {
    return(current_size);
  }

  virtual long long getCacheSize() const {
    return(cache_size);
  }
};

/*
  Least Recently Used implementation
*/

class LRUCache: public Cache {
protected:
  // ordered LRU list
  list<object_t> cache_list;
  // access to objects by storing respective iterators in unordered_map
  unordered_map<long, list_iterator_t> cache_map;

  virtual void hit(unordered_map<long, list_iterator_t>::const_iterator it, long size) {
    cache_list.splice(cache_list.begin(), cache_list, it->second);
    Cache::hit(size);
  }

  virtual void miss(const long cur_req, const long size) {
    // object feasible to store?
    if(size >= cache_size) {
      LOG("error",0,size,cache_size);
      return;
    }
    list_iterator_t lit;  
    // check eviction needed
    while (current_size + size > cache_size) {
      // evict least popular (i.e. last element)
      lit = cache_list.end();
      lit--;
      long esize = get<1>(*lit);
      LOG("e",current_size,get<0>(*lit),esize);
      current_size -= esize;
      cache_map.erase(get<0>(*lit));
      cache_list.erase(lit);
    }
    // admit new object
    cache_list.push_front(object_t(cur_req, size)); 
    cache_map[cur_req] = cache_list.begin();
    current_size += size;
    LOG("a",current_size,cur_req,size);
  }


public:
  LRUCache(long long cs): Cache(cs) {}

  LRUCache(const LRUCache& rhs): Cache(rhs.getCacheSize()) { /* copy constructor*/
    this->cache_list = rhs.getCacheList();
    for (list_iterator_t it = cache_list.begin(); it!=cache_list.end(); it++)
      this->cache_map[get<0>(*it)] = it;
    current_size = rhs.getCurrentSize();
    hits = rhs.getHits();
    bytehits = rhs.getBytehits();
    logStatistics = rhs.getLogStatistics();
    assert(this->cache_list.size() == this->cache_map.size());
    //    cerr << "copied cache with " << cache_map.size() << " objects, " << double(current_size)/cache_size << " full" << endl;
  }

  ~LRUCache(){}

  bool lookup (const long cur_req) const {

    return(cache_map.count(cur_req)>0);
  }

  void evict (const long cur_req) {
    if(lookup(cur_req)) {
      list_iterator_t lit = cache_map[cur_req];
      cache_map.erase(cur_req);
      current_size -= get<1>(*lit);
      cache_list.erase(lit);
    }
  }

  bool request (const long cur_req, const long size) {
    unordered_map<long, list_iterator_t>::const_iterator it;
    it = cache_map.find(cur_req);
    if(it != cache_map.end())
      {
	if (size == get<1>(*(it->second))) {
	  // hit and consistent size
	  LOG("h",0,cur_req,size);
	  hit(it, size);
	  return(true);
	}
	else { // inconsistent size -> treat as miss and delete inconsistent entry
	  evict(cur_req);
	  cerr << "deleted outdated object" << endl;
	}
      }
    miss(cur_req, size);
    return(false);
  }

  const list<object_t> getCacheList() const {
    return (cache_list);
  }
  const unordered_map<long, list_iterator_t> getCacheMap() const {
    return (cache_map);
  }
};



/*
  FIFO
*/

class FIFOCache: public LRUCache {
protected:
  virtual void hit(unordered_map<long, list_iterator_t>::const_iterator it, long size) {
    Cache::hit(size);
  }
public:
  FIFOCache(long long cs): LRUCache(cs) {}
  ~FIFOCache(){}
};





/*
  greedy dual implementation base

  implementation in log n time for each cache miss
  using multi map: insert in O(log n), erase in O(1), get lowest value in O(1) + store object id and size
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

  void hit(const long cur_req, const long long size) {
    // get iterator for the old position
    map_iterator_t si = cache_map[cur_req];
    // update current req's value to hval:
    value_map.erase(si);
    long double hval = agevalue(cur_req, size);
    cache_map[cur_req] = value_map.insert( pair<long double, object_t>(hval, object_t(cur_req, size)) );
    // done - gather statistics
    Cache::hit(size);
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
  GreedyDualBase(long long cs): Cache(cs) {}

  ~GreedyDualBase(){}

  bool lookup (const long cur_req) const {
    return(cache_map.count(cur_req)>0);
  }

  void evict (const long cur_req) {
    if(lookup(cur_req)) {
      map_iterator_t lit = cache_map[cur_req];
      object_t obj = lit->second;
      cache_map.erase(cur_req);
      current_size -= get<1>(obj);
      value_map.erase(lit);
    }
  }

  bool request (const long cur_req, long size) {
    if (cache_map.count(cur_req) > 0) {
	if (size == get<1>(get<1>(*(cache_map[cur_req]))) ) {
	  // hit
	  LOG("h",0,cur_req,size);
	  hit(cur_req, size);
	  return(true);
	} else {
	  // inconsistent size -> treat as miss and delete inconsistent entry
	  evict(cur_req);
	  cerr << "deleted outdated object" << endl;
	}
    }
    miss(cur_req, size);
    return(false);
  }
};




/*
  Greedy Dual Size policy
*/

class GDSCache: public GreedyDualBase {
protected:
  long double agevalue(const long cur_req, const long size) {
    return(current_L + 1/double(size));
  }

public:
  GDSCache(long long cs): GreedyDualBase(cs) {}
};



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
  GDSFCache(long long cs): GreedyDualBase(cs) {}

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
	cerr << "deleted outdated object" << endl;
      }
    }
    reqs_map[cur_req]=1; //reset bec. reqs_map not updated when element removed
    miss(cur_req, size);
    return(false);
  }
};





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
  LRUKCache(long long cs, unsigned int k): GreedyDualBase(cs), tk(k), curtime(0) {}

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
	cerr << "deleted outdated object" << endl;
      }
    }
    refs_map[cur_req].push(curtime);
    miss(cur_req, size);
    return(false);
  }

};


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
  LFUDACache(long long cs): GreedyDualBase(cs) {}

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
	cerr << "deleted outdated object" << endl;
      }
    }
    reqs_map[cur_req]=1; //reset bec. reqs_map not updated when element removed
    miss(cur_req, size);
    return(false);
  }
};



/*
  S2LRU
*/

class S2LRUCache: public LRUCache {
protected:
  LRUCache previous;

  virtual void miss(const long cur_req, const long size) {
    // object feasible to store?
    if(size >= cache_size) {
      LOG("error",0,size,cache_size);
      return;
    }
    list_iterator_t lit;  
    // check eviction needed
    while (current_size + size > cache_size) {
      // evict least popular (i.e. last element)
      lit = cache_list.end();
      lit--;
      long esize = get<1>(*lit);
      LOG("e",current_size,get<0>(*lit),esize);
      previous.request(get<0>(*lit), esize); // move to front of previous cache
      current_size -= esize;
      cache_map.erase(get<0>(*lit));
      cache_list.erase(lit);
    }
    // admit new object
    cache_list.push_front(object_t(cur_req, size)); 
    cache_map[cur_req] = cache_list.begin();
    current_size += size;
    LOG("a",current_size,cur_req,size);
  }

public:
  S2LRUCache(long long fcs, long long scs): LRUCache(scs), previous(fcs) {  }

  ~S2LRUCache(){}

  void evict (const long cur_req) {
    previous.evict(cur_req);
    LRUCache::evict(cur_req);
  }

  bool lookup2nd (const long cur_req) {
    return (LRUCache::lookup(cur_req));
  }

  bool request (const long cur_req, const long size) {
    if(previous.lookup(cur_req)) { // found in previous layer
      previous.evict(cur_req); //delete in previous
      miss(cur_req, size); //admit to current
      LOG("h",101,cur_req,size);
      return(LRUCache::request(cur_req, size)); //hit in current
    } else if(LRUCache::lookup(cur_req)) {// found in current
      LOG("h",102,cur_req,size);
      return(LRUCache::request(cur_req, size)); //hit in current
    }
    if(previous.request(cur_req, size)) { //request in all prev. layers
      Cache::hit(size); // hidden hit
      return(true);
    }
    return(false);
  }
};



/*
  S3LRU
*/

class S3LRUCache: public LRUCache {
protected:
  S2LRUCache previous;

  virtual void miss(const long cur_req, const long size) {
    // object feasible to store?
    if(size >= cache_size) {
      LOG("error",0,size,cache_size);
      return;
    }
    list_iterator_t lit;  
    // check eviction needed
    while (current_size + size > cache_size) {
      // evict least popular (i.e. last element)
      lit = cache_list.end();
      lit--;
      long esize = get<1>(*lit);
      LOG("e",current_size,get<0>(*lit),esize);
      previous.request(get<0>(*lit), esize); // move to front of previous cache
      current_size -= esize;
      cache_map.erase(get<0>(*lit));
      cache_list.erase(lit);
    }
    // admit new object
    cache_list.push_front(object_t(cur_req, size)); 
    cache_map[cur_req] = cache_list.begin();
    current_size += size;
    LOG("a",current_size,cur_req,size);
  }

public:
  S3LRUCache(long long fcs, long long scs, long long tcs): LRUCache(tcs), previous(fcs,scs) {  }

~S3LRUCache(){}

  void evict (const long cur_req) {
    previous.evict(cur_req);
    LRUCache::evict(cur_req);
  }

  bool lookup3rd (const long cur_req) {
    return (LRUCache::lookup(cur_req));
  }

  bool request (const long cur_req, const long size) {
    if(previous.lookup2nd(cur_req)) { // found in previous
      //      cerr << "MOVE UP 2->3"  << endl;
      previous.evict(cur_req); //delete in previous
      miss(cur_req, size); //admit to current
      //      assert(LRUCache::lookup(cur_req));
      LOG("h",102,cur_req,size);
      return(LRUCache::request(cur_req, size)); //hit in current
    } else if(LRUCache::lookup(cur_req)) {// found in current
      LOG("h",103,cur_req,size);
      return(LRUCache::request(cur_req, size)); //hit in current
    }
    if(previous.request(cur_req, size)) { //request in all prev. layers
      Cache::hit(size); // hidden hit
      return(true);
    }
    return(false);
  }
};





/*
  S4LRU
*/

class S4LRUCache: public LRUCache {
protected:
  S3LRUCache previous;

  virtual void miss(const long cur_req, const long size) {
    // object feasible to store?
    if(size >= cache_size) {
      LOG("error",0,size,cache_size);
      return;
    }
    list_iterator_t lit;  
    // check eviction needed
    while (current_size + size > cache_size) {
      // evict least popular (i.e. last element)
      lit = cache_list.end();
      lit--;
      long esize = get<1>(*lit);
      LOG("e",current_size,get<0>(*lit),esize);
      previous.request(get<0>(*lit), esize); // move to front of previous cache
      current_size -= esize;
      cache_map.erase(get<0>(*lit));
      cache_list.erase(lit);
    }
    // admit new object
    cache_list.push_front(object_t(cur_req, size)); 
    cache_map[cur_req] = cache_list.begin();
    current_size += size;
    LOG("a",current_size,cur_req,size);
  }

public:
  S4LRUCache(long long fcs, long long scs, long long tcs, long long fourthcs): LRUCache(fourthcs), previous(fcs,scs,tcs) {  }

  ~S4LRUCache(){}

  void evict (const long cur_req, const long size) {
    previous.evict(cur_req);
    LRUCache::evict(cur_req);
  }

  bool request (const long cur_req, const long size) {
    if(previous.lookup3rd(cur_req)) { // found in previous
      //      cerr << "MOVE UP 3->4"  << endl;
      previous.evict(cur_req); //delete in previous
      miss(cur_req, size); //admit to current
      LOG("h",103,cur_req,size);
      //      assert(LRUCache::lookup(cur_req));
      return(LRUCache::request(cur_req, size)); //hit in current
    } else if(LRUCache::lookup(cur_req)) {// found in current
      LOG("h",104,cur_req,size);
      return(LRUCache::request(cur_req, size)); //hit in current
    }
    if(previous.request(cur_req, size)) { //request in all prev. layers
      Cache::hit(size); // hidden hit
      return(true);
    }
    return(false);
  }
};
