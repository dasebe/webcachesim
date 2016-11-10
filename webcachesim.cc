#include <fstream>
#include "policies/lru_variants.cc"
//#include "policies/gd_variants.cc"

int main (int argc, char* argv[])
{

  // parameters
  if(argc <= 6) {
    cerr << "webcachesim traceFile warmUp cacheType log2CacheSize cacheParam" << endl;
    return 1;
  }

  // trace properties
  const char* path = argv[1];
  const long warmUp = atol(argv[2]);

  // create cache
  const string cacheType = argv[3];
  unique_ptr<Cache> webcache = move(Cache::create_unique(cacheType));

  // configure cache size
  const double sizeExp = atof(argv[4]);
  const long long cache_size  = pow(2.0,sizeExp);
  webcache->setSize(cache_size);

  // parse cache parameters
  const int paramCount = argc-5;
  if(paramCount == 0)
    cerr << "no param" << endl;
  else {//////
    for(int i=6; i<argc; i++) {
    setPar(paramCount, 
  const long double param = atof(argv[4]);





  ifstream infile;
  long reqs = 0, bytes = 0;
  long t, id, size;
  bool logStatistics=false;

  cerr << "running..." << endl;

  infile.open(path);
  while (infile >> t >> id >> size)
    {
      // start statistics after warm up
      if (!logStatistics && t > warmUp)
	{
	  cerr << "statistics started" << endl;
	  logStatistics = true;
	  webcache->startStatistics();
	}

      // log statistics
      if (logStatistics)
	{
	  reqs++;
	  bytes += size;
	}

      // request
      webcache->request(id,size);
    }

  infile.close();
  cout << t <<  " " << cacheType << " " << sizeExp << " " << param << " " << webcache->getHits() << " " << reqs << " " << bytes << endl;

  return 0;
}
