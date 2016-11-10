#include <fstream>
#include "policies/lru_variants.cc"

int main (int argc, char* argv[])
{

  // parameters
  if(argc != 6) {
    cerr << "webcachesim traceFile cacheType sizeExp param warmUp" << endl;
    return 1;
  }

  const char* path = argv[1];
  const string cacheType = argv[2];
  const double sizeExp = atof(argv[3]);
  const long double param = atof(argv[4]);
  const long warmUp = atol(argv[5]);

  const long long cache_size  = pow(2.0,sizeExp);

  unique_ptr<Cache> webcache = move(Cache::create_unique(cacheType));
  webcache->setSize(cache_size);

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
