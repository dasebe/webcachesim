#include <chrono>
#include <fstream>
#include <unordered_map>
#include <LightGBM/application.h>
#include <LightGBM/c_api.h>
#include "request.h"
#include "simulation_lfo2.h"
#include "annotate.h"
#include "lfo2.h"


using namespace std;
using namespace chrono;


void move_a_b(LFOACache * c_a, LFOBCache * c_b) {
    for (auto it_key = c_a->object_size.begin(); it_key != c_a->object_size.end(); ++it_key) {
        const uint64_t & key = it_key->first;
        const uint64_t & size = it_key->second;
        const uint64_t & past_timestamp = c_a->past_timestamp.find(key)->second;
        const uint64_t & future_timestamp = c_a->future_timestamp.find(key)->second;
        const list<uint64_t > & past_intervals = c_a->past_intervals.find(key)->second;
        uint8_t list_idx;
        if (c_a->_cacheMap.left.find(key) != c_a->_cacheMap.left.end()) {
            //add to list 1
            list_idx = 0;
            c_b->_currentSize += size;
        } else {
            list_idx = 1;
        }
        c_b->key_map.insert({key, {list_idx, (uint32_t) c_b->meta_holder[list_idx].size() }});
        c_b->meta_holder[list_idx].emplace_back(key, size, past_timestamp, future_timestamp, past_intervals);
    }
    assert(c_a->_currentSize == c_b->_currentSize);

}

void move_b_a(LFOBCache * c_b, LFOACache * c_a) {
    c_a->_currentSize = 0;
    c_a->_cacheMap.clear();

    for (auto & meta: c_b->meta_holder[0]) {
        c_a->_cacheMap.left.insert({meta._key, meta._future_timestamp});
        c_a->_currentSize += meta._size;
    }

    assert(c_a->_currentSize == c_b->_currentSize);
}

map<string, string> _simulation_lfo2(string trace_file, string cache_type, uint64_t cache_size,
                                    map<string, string> params) {

    //annotate a file
    //not necessary to annotate, but it's easier
    //make simulation faster
    annotate(trace_file);
    // create cache
    //A runs OPT, B runs LFO2
    unique_ptr<Cache> _webcachea = move(Cache::create_unique("LFOA"));
    if (_webcachea == nullptr) {
      cerr << "cache type not implemented" << endl;
      return {};
    }
    auto webcachea = dynamic_cast<LFOACache *>(_webcachea.get());

    unique_ptr<Cache> _webcacheb = move(Cache::create_unique("LFOB"));
    if (_webcacheb == nullptr) {
      cerr << "cache type not implemented" << endl;
      return {};
    }
    auto webcacheb = dynamic_cast<LFOBCache *>(_webcacheb.get());

    // configure cache size
    webcachea->setSize(cache_size);
    webcacheb->setSize(cache_size);

    uint64_t n_warmup = 0;
    bool uni_size = false;
    uint64_t window_size = 1000000;

    for (auto &kv: params) {
        if (kv.first == "window")
          window_size = stoull(kv.second);
        if (kv.first == "uni_size")
            uni_size = static_cast<bool>(stoi(kv.second));
        if (kv.first == "n_warmup")
            n_warmup = stoull(kv.second);
    }

    //only eval after 2 window
    assert(window_size <= n_warmup);

    ifstream infile(trace_file+".ant");
    if (!infile) {
        cerr << "exception opening/reading file" << endl;
        return {};
    }

    //suppose already annotated
    uint64_t byte_req = 0, byte_hit = 0, obj_req = 0, obj_hit = 0;
    uint64_t shadow_byte_req = 0, shadow_byte_hit = 0, shadow_obj_req = 0, shadow_obj_hit = 0;
    uint64_t t, id, size, next_t;

    cerr << "simulating" << endl;
    AnnotatedRequest req(0, 0, 0, 0);
    uint64_t seq = 0;
    auto t_now = system_clock::now();

    while (infile >> t >> id >> size >> next_t) {
        if (uni_size)
            size = 1;
        //can only look window far
        next_t = min(next_t, t + window_size);
        req.reinit(id, size, t, next_t);

        //shadow cache
        {
            //update model
            if (seq && !(seq % window_size)) {
                cerr << "training model" << endl;
                webcachea->train();
                webcacheb->booster = webcachea->booster;
                if (seq == window_size){
                    cerr<<"copying cache state A -> B"<<endl;
                    move_a_b(webcachea, webcacheb);
                } else if (seq > window_size) {
                    //todo: cache state B -> A
                    cerr<<"copying cache state B -> A"<<endl;
                    move_b_a(webcacheb, webcachea);
                }
            }

            if (seq == n_warmup) {
                cerr<<"reset shadow metric to align with brighten"<<endl;
                shadow_byte_hit = shadow_byte_req = shadow_obj_hit = shadow_obj_req = 0;
            }

            shadow_byte_req += size;
            shadow_obj_req++;
            //train
            if (webcachea->lookup(req)) {
                shadow_byte_hit += size;
                shadow_obj_hit++;
            } else
                webcachea->admit(req);

        }


        //brighten cache. Not sure whether it is a good name
        {
            if (seq >= window_size) {
                if (seq >= n_warmup) {
                    byte_req += size;
                    obj_req++;
                }

                //eval
                //only start from window 2+
                if (webcacheb->lookup(req)) {
                    if (seq >= n_warmup) {
                        byte_hit += size;
                        obj_hit++;
                    }
                } else {
                    webcacheb->admit(req);
                }
            }
        }

        ++seq;
        if (seq && !(seq%10000)) {
            auto _t_now = system_clock::now();
            cerr<<"\ndelta t: "<<duration_cast<seconds>(_t_now - t_now).count()<<endl;
            cerr<<"seq: " << seq << endl;
            cerr<<"brighten bhr: " << double(byte_hit) / byte_req << endl;
            cerr<<"shadow bhr: " << double(shadow_byte_hit) / shadow_byte_req << endl;
            t_now = _t_now;
        }
    }

  infile.close();

  map<string, string> res = {
          {"byte_hit_rate",   to_string(double(byte_hit) / byte_req)},
          {"object_hit_rate", to_string(double(obj_hit) / obj_req)},
  };
  return res;
}
