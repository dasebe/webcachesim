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

  map<string, string> params;

  // parse cache parameters
  regex opexp ("(.*)=(.*)");
  cmatch opmatch;
  string paramSummary;
  for(int i=4; i<argc; i++) {
    if(paramSummary.length()>0) {
      paramSummary += "-";
    }
    regex_match (argv[i],opmatch,opexp);
    if(opmatch.size()!=3) {
      cerr << "each cacheParam needs to be in form name=value" << endl;
      return 1;
    }
    cerr<<opmatch[1]<<endl<<opmatch[2]<<endl;
    params[opmatch[1]] = opmatch[2];
    paramSummary += opmatch[2];
  }

  auto res = simulation(argv[1], argv[2], std::stoull(argv[3]), params);

  cout << argv[1] << " c " << argv[2] << " s " << argv[3] << " b " << res["byte_hit_rate"] <<  " o " << res["object_hit_rate"] << " " << paramSummary << endl;

  return 0;
}
