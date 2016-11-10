#include <iostream>
#include <memory>
#include <string>
#include <stdarg.h>
#include <map>

using namespace std;

// util for debug
#ifdef CDEBUG
#define LOG(m,x,y,z) log_message(m,x,y,z)
#else
#define LOG(m,x,y,z)
#endif
void log_message(string m, double x, double y, double z) {
  cerr << m << "," << x << "," << y  << "," << z << "\n";
}



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
  virtual void I_am() { cout << "I am cache\n"; }

  // configure cache parameters
  virtual void setSize(long long cs) {cache_size = cs;}
  virtual void setPar(int count, ...) {}

  // request an object from the cache
  virtual bool request (const long cur_req, const long long size) {return(false);}
  // check in cache (debugging)
  virtual bool lookup (const long cur_req) const {return(false);}

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
