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
#include <memory>
#include <string>
#include <stdarg.h>

using namespace std;

typedef tuple<long, long> object_t; // objectid, size
typedef list<object_t>::iterator list_iterator_t;
typedef multimap<long double, object_t>::iterator map_iterator_t;

// util for debug
#ifdef CDEBUG
#define LOG(m,x,y,z) log_message(m,x,y,z)
#else
#define LOG(m,x,y,z)
#endif
void log_message(string m, double x, double y, double z) {
  cerr << m << "," << x << "," << y  << "," << z << "\n";
}
// set to enable cache debugging
#define CDEBUG 1



/*
  general cache class
*/

class Cache;

class CacheFactory {
public:
  CacheFactory() {}
  virtual unique_ptr<Cache> create_unique() = 0;
};


class Cache {
public:
  // create and destroy a cache
  Cache() : cache_size(0), current_size(0), hits(0), bytehits(0), logStatistics(false) {
  }
  virtual ~Cache(){};
  void I_am() { cout << "I am cache\n"; }

  // configure cache parameters
  virtual void setSize(long long cs) {cache_size = cs;}
  virtual void setPar(int count, ...) {}

  // request an object from the cache
  virtual bool request (const long cur_req, const long long size) {return(false);}
  // check in cache (debugging)
  virtual bool lookup (const long cur_req) const {return(false);}

  // statistics gathering
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
  virtual long long getBytehits() const {
    return(bytehits);
  }
  virtual long long getCurrentSize() const {
    return(current_size);
  }
  virtual long long getCacheSize() const {
    return(cache_size);
  }
  // helper functions (factory pattern)
  static void registerType(string name, CacheFactory *factory) {
    get_factory_instance()[name] = factory;
  }
  static unique_ptr<Cache> create_unique(string name) {
    cout << "test " << name << endl;
    unique_ptr<Cache> Cache_instance =
      move(get_factory_instance()[name]->create_unique());
    return Cache_instance;
  }
  
protected:
  // basic cache properties
  long long cache_size;
  long long current_size;
  // cache hit statistics
  long hits;
  long long bytehits;

  bool logStatistics;

  // helper functions (factory pattern)
   static map<string, CacheFactory *> &get_factory_instance() {
     static map<string, CacheFactory *> map_instance;
     return map_instance;
  }
  // helper function (statistics gathering)
  virtual void hit(long size) {
    if(logStatistics) {
      hits++;
      bytehits+=size;
    }
  }
};

template<class T>
 class Factory : public CacheFactory {
 public:
  Factory(string name) { Cache::registerType(name, this); }
  unique_ptr<Cache> create_unique() {
    unique_ptr<Cache> newT(new T);
    return newT;
  }
};

/*
  Least Recently Used implementation
*/

class LRUCache: public Cache {
public:
  // construct and destroy LRU
  LRUCache() {}
  ~LRUCache(){}
  void I_am() { cout << "I am lru\n"; }

  // normal cache functions
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
	  //	  cerr << "deleted outdated object" << endl;
	}
      }
    miss(cur_req, size);
    return(false);
  }
  
protected:
  // list for recency order
  list<object_t> cache_list;
  // map to find objects in list
  unordered_map<long, list_iterator_t> cache_map;

  // main functionality: deal with hit and miss
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
};
static Factory<LRUCache> factoryLRU("LRU");


/*
  FIFO
*/

class FIFOCache: public LRUCache {
public:
  FIFOCache() {}
  ~FIFOCache(){}
  void I_am() { cout << "I am fifo\n"; }

protected:
  virtual void hit(unordered_map<long, list_iterator_t>::const_iterator it, long size) {
    Cache::hit(size);
  }
};
static Factory<FIFOCache> factoryFIFO("FIFO");
