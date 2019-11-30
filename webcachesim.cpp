#include <fstream>
#include <string>
#include <regex>
#include "caches/lru_variants.h"
//#include "caches/gd_variants.h"
#include "caches/lfo_cache.h"
#include "request.h"
#include "caches/optimal.h"

using namespace std;

uint64_t run_model(vector<SimpleRequest> & prev_requests,
               vector<vector<double>> & prev_features,
               unique_ptr<Cache> & webcache,
               ifstream & infile,
               size_t batch_size) {

    uint64_t time, id, size;
    uint64_t counter = 0;
    uint64_t hit = 0;

    while (!infile.eof()) {

        if (counter >= batch_size) {
            break;
        }

        infile >> time >> id >> size;

        SimpleRequest req(id, size, time);
        prev_requests.push_back(req);

        vector<double> prev_feature = webcache->get_lfo_feature(&req).get_vector();
        if (!prev_feature.empty()) {
            prev_features.push_back(prev_feature);
        }

        if (webcache->lookup(&req)) {
            hit++;
        } else {
            webcache->admit(&req);
        }
        counter += 1;
    }

    return hit;

}

void run_simulation(const string path, const string cacheType, const uint64_t cache_size) {
    unique_ptr<Cache> webcache = Cache::create_unique(cacheType);
    if(webcache == nullptr)
        exit(0);

    // configure cache size
    webcache->setSize(cache_size);

    ifstream infile;
    size_t batch_size = 1000000;
    bool changed_to_lfo = false;

    vector<SimpleRequest> prev_requests;
    vector<vector<double >> prev_features;


    infile.open(path);
    while (!infile.eof()) {
        uint64_t hits = run_model(prev_requests, prev_features, webcache, infile, batch_size);

        cout << "Hit accuracy: " << static_cast<double>(hits)/batch_size << "\n";

        if (!prev_features.empty() && !prev_requests.empty()
            && prev_features.size() == prev_requests.size()){
            vector<double> optimal_decisions = getOptimalDecisions(prev_requests, webcache->getSize());
            cout << "The number of optimal decisions: " << optimal_decisions.size() << endl;

            if (!changed_to_lfo) {
                webcache->setSize(cache_size);
                changed_to_lfo = !changed_to_lfo;
            }
            webcache->train_lightgbm(prev_features, optimal_decisions);
            prev_features.clear();
            prev_requests.clear();
        }
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
