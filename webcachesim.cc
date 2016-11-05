#include <fstream>
#include "helpers/cache_definitions.cpp"

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


  if(initCaches(cacheType,cache_size,param)!=0)
    return 1;

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
	  tch->startStatistics();
	}

      // log statistics
      if (logStatistics)
	{
	  reqs++;
	  bytes += size;
	}

      // request
      reqFun(id, size);
    }

  infile.close();
  cout << t <<  " " << cacheType << " " << sizeExp << " " << param << " " << tch->getHits() << " " << reqs << " " << bytes << endl;

  return 0;
}
