//
// Created by Zhenyu Song on 10/30/18.
//

#include "simulation.h"

#include <fstream>
#include <string>
#include <regex>
#include "lru_variants.h"
#include "gd_variants.h"
#include "request.h"

using namespace std;

SimulationResult simulation(string tracefile, string cache_type, uint64_t cache_size, map<string, double> & params){

    // create cache
    unique_ptr<Cache> webcache = move(Cache::create_unique(cache_type));
    // todo: raise exception?
//    if(webcache == nullptr)
//        return 1;

    // configure cache size
    webcache->setSize(cache_size);

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

    ifstream infile;
    long long reqs = 0, hits = 0;
    long long t, id, size;

    cerr << "running..." << endl;

    infile.open(tracefile);
    SimpleRequest* req = new SimpleRequest(0, 0);
    while (infile >> t >> id >> size)
    {
        reqs++;

        req->reinit(id,size);
        if(webcache->lookup(req)) {
            hits++;
        } else {
            webcache->admit(req);
        }
    }

    delete req;

    infile.close();
//    cout << cacheType << " " << cache_size << " " << paramSummary << " "
//         << reqs << " " << hits << " "
//         << double(hits)/reqs << endl;
    return SimulationResult(double(hits)/reqs);
}
