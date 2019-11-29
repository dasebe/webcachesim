#include <fstream>
#include <string>
#include <regex>
#include "caches/lru_variants.h"
//#include "caches/gd_variants.h"
#include "caches/lfo_cache.h"
#include "request.h"

using namespace std;

void run_simulation(const string path, const string cacheType, const uint64_t cache_size) {

    unique_ptr<Cache> webcache = Cache::create_unique(cacheType);
    if(webcache == nullptr)
        exit(0);

    // configure cache size
    webcache->setSize(cache_size);


    ifstream infile;
    long long reqs = 0, hits = 0, batch_size = 1000;
    long long t, id, size;

    infile.open(path);
    SimpleRequest* req = new SimpleRequest(0, 0, 0);
    while (!infile.eof()) {
        size_t counter = 0;
        hits = 0;
        while (!infile.eof() && counter < batch_size) {
            infile >> t >> id >> size;
            reqs++;
            req->reinit(id, size, t);
            if (webcache->lookup(req)) {
                hits++;
            } else {
                webcache->admit(req);
            }
            counter += 1;
        }
        cout << "Hits: " << hits << " Accuracy: " << static_cast<double>(hits)/batch_size << endl;
    }
}

int main (int argc, char* argv[])
{

    // output help if insufficient params
    if(argc < 4) {
        cerr << "webcachesim traceFile cacheType cacheSizeBytes" << endl;
        return 1;
    }

    const char* path = argv[1];
    const string cacheType = argv[2];
    const uint64_t cache_size  = std::stoull(argv[3]);



    run_simulation(path, cacheType, cache_size);


    return 0;
}
