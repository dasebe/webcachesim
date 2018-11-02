#include <fstream>
#include <string>
#include <regex>
#include "lru_variants.h"
#include "gd_variants.h"
#include "request.h"
#include "simulation.h"
#include <map>

using namespace std;

int main (int argc, char* argv[])
{

  // output help if insufficient params
  if(argc < 4) {
    cerr << "webcachesim traceFile cacheType cacheSizeBytes [cacheParams]" << endl;
    return 1;
  }

  map<string, double> params;

  auto res = simulation(argv[1], argv[2], std::stoull(argv[3]), params);

  cout << "bhr: " << res["byte_hit_rate"] << endl << "ohr: " << res["object_hit_rate"] << endl;

// todo: omit params at first
//  // parse cache parameters
//  regex opexp ("(.*)=(.*)");
//  cmatch opmatch;
//  string paramSummary;
//  for(int i=4; i<argc; i++) {
//    regex_match (argv[i],opmatch,opexp);
//    if(opmatch.size()!=3) {
//      cerr << "each cacheParam needs to be in form name=value" << endl;
//      return 1;
//    }
//    webcache->setPar(opmatch[1], opmatch[2]);
//    paramSummary += opmatch[2];
//  }


  return 0;
}
