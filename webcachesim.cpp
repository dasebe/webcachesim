#include <fstream>
#include <string>
#include <regex>
#include "caches/lru_variants.h"
#include "caches/gd_variants.h"
#include "request.h"

using namespace std;

int main (int argc, char* argv[])
{

    // output help if insufficient params
    if(argc < 4) {
        cerr << "webcachesim traceFile cacheType cacheSizeBytes" << endl;
        return 1;
    }

    // trace properties
    const char* path = argv[1];

    // create cache
    const string cacheType = argv[2];
    unique_ptr<Cache> webcache = Cache::create_unique(cacheType);
    if(webcache == nullptr)
        return 1;

    // configure cache size
    const uint64_t cache_size  = std::stoull(argv[3]);
    webcache->setSize(cache_size);


    ifstream infile;
    long long reqs = 0, hits = 0, batch_size = 1000;
    long long t, id, size;

    infile.open(path);
    SimpleRequest* req = new SimpleRequest(0, 0);
    while (!infile.eof()) {
        size_t counter = 0;
        hits = 0;
        while (!infile.eof() && counter < batch_size) {
            infile >> t >> id >> size;
            reqs++;
            req->reinit(id, size);
            if (webcache->lookup(req)) {
                hits++;
            } else {
                webcache->admit(req);
            }
            counter += 1;
        }
        cout << "Hits: " << hits << " Accuracy: " << static_cast<double>(hits)/batch_size << endl;
    }


    return 0;
}
