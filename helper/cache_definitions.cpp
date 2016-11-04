#include <functional>
#include <string>
#include <iostream>
#include "../policies/cache_policies.cpp"

using namespace std;

// cache instantiation
 Cache* tch;
 LRUCache* lclru;
 FIFOCache* lcfifo;
 GDSCache* lcgds;
 GDSFCache* lcgdsf;
 LRUKCache* lclruk;
 LFUDACache* lclfuda;
 S2LRUCache* lcs2lru;
 S4LRUCache* lcs4lru;
 InfiniteCache* lcinfinite;

 function< bool(long int, long int) > reqFun;

unordered_map<long,long> filter;
long double filterMIN;

int initCaches (const string cacheType, const long long cache_size, const long double param) {
  if (cacheType == "LRU")
    {
      lclru = new LRUCache(cache_size);
      reqFun = [&] (long int id, long int size) { return lclru->request(id,size); };
      tch = dynamic_cast<Cache*> (lclru);
    }
  else if (cacheType == "Filter")
    {
      lclru = new LRUCache(cache_size);
      filterMIN = param;
      reqFun = [&] (long int id, long int size) {
	filter[id]++;
	if(filter[id]>filterMIN)
	  return lclru->request(id,size);
	return false;};
      tch = dynamic_cast<Cache*> (lclru);
    }
  else if (cacheType == "FIFO")
    {
      lcfifo = new FIFOCache(cache_size);
      reqFun = [&] (long int id, long int size) { return lcfifo->request(id,size); };
      tch = dynamic_cast<Cache*> (lcfifo);
    }
  else if (cacheType == "GDS")
    {
      lcgds = new GDSCache(cache_size);
      reqFun = [&] (long int id, long int size) { return lcgds->request(id,size); };
      tch = dynamic_cast<Cache*> (lcgds);
    }
  else if (cacheType == "GDSF")
    {
      lcgdsf = new GDSFCache(cache_size);
      reqFun = [&] (long int id, long int size) { return lcgdsf->request(id,size); };
      tch = dynamic_cast<Cache*> (lcgdsf);
    }
  else if (cacheType == "LRUK")
    {
      lclruk = new LRUKCache(cache_size,param);
      reqFun = [&] (long int id, long int size) { return lclruk->request(id,size); };
      tch = dynamic_cast<Cache*> (lclruk);
    }
  else if (cacheType == "LFUDA")
    {
      lclfuda = new LFUDACache(cache_size);
      reqFun = [&] (long int id, long int size) { return lclfuda->request(id,size); };
      tch = dynamic_cast<Cache*> (lclfuda);
    }
  else if (cacheType == "S2LRU")
    {
      lcs2lru = new S2LRUCache(cache_size - floor(cache_size*param/100.0),floor(cache_size*param/100.0));
      reqFun = [&] (long int id, long int size) { return lcs2lru->request(id,size); };
      tch = dynamic_cast<Cache*> (lcs2lru);
    }
  else if (cacheType == "S4LRU")
    {
      lcs4lru = new S4LRUCache(cache_size - floor(cache_size*param/100.0),floor(cache_size*param/100.0/3),floor(cache_size*param/100.0/3),floor(cache_size*param/100.0/3));
      reqFun = [&] (long int id, long int size) { return lcs4lru->request(id,size); };
      tch = dynamic_cast<Cache*> (lcs4lru);
    }
  else if (cacheType == "Infinite")
    {
      lcinfinite = new InfiniteCache(cache_size);
      reqFun = [&] (long int id, long int size) { return lcinfinite->request(id,size); };
      tch = dynamic_cast<Cache*> (lcinfinite);
    }
  else {
    cerr << "wrong cache type: " << cacheType << endl;
    return 1;
  }
  return 0;
}
