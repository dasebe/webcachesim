//
// Created by zhenyus on 12/17/18.
//

#include <chrono>
#include <fstream>
#include <unordered_map>
#include "request.h"
#include "simulation_lr_belady.h"
#include "annotate.h"
#include "belady_sample.h"
#include "random_variants.h"
#include "utils.h"


using namespace std;
using namespace chrono;


map<string, string> _simulation_lr_belady(string trace_file, string cache_type, uint64_t cache_size,
                                    map<string, string> params) {

    //annotate a file
    //not necessary to annotate, but it's easier
    //make simulation faster
    annotate(trace_file);
    // create cache
    //A runs OPT, B runs real
    unique_ptr<Cache> _webcachea = move(Cache::create_unique("BeladySampleFilter"));
    if (_webcachea == nullptr) {
      cerr << "cache type not implemented" << endl;
      return {};
    }
    auto webcachea = dynamic_cast<BeladySampleCacheFilter *>(_webcachea.get());

    unique_ptr<Cache> _webcacheb = move(Cache::create_unique("LR"));
    if (_webcacheb == nullptr) {
      cerr << "cache type not implemented" << endl;
      return {};
    }
    auto webcacheb = dynamic_cast<LRCache *>(_webcacheb.get());


    uint64_t n_warmup = 0;
    bool uni_size = false;
    uint64_t sync_window = 1000000;
    uint64_t segment_window = 1000000;
    double shadow_size_ratio= 1.;

    for (auto kv = params.cbegin(); kv != params.cend();) {
        if (kv->first == "window") {
            sync_window = stoull(kv->second);
            kv = params.erase(kv);
        } else if (kv->first == "n_warmup") {
            n_warmup = stoull(kv->second);
            kv = params.erase(kv);
        } else if (kv->first == "uni_size") {
            uni_size = static_cast<bool>(stoi(kv->second));
            kv = params.erase(kv);
        } else if (kv->first == "segment_window") {
            segment_window = stoull((kv->second));
            kv = params.erase(kv);
        } else if (kv->first == "shadow_size_ratio") {
            shadow_size_ratio = stod(kv->second);
            kv = params.erase(kv);
        } else {
            ++kv;
        }
    }

    auto size_a = (uint64_t) (cache_size * shadow_size_ratio);

    // configure cache size
    webcachea->setSize(size_a);
    webcacheb->setSize(cache_size);

    //only eval after 2 window
    assert(sync_window <= n_warmup);

    webcachea->init_with_params(params);
    webcacheb->init_with_params(params);

    ifstream infile(trace_file+".ant");
    if (!infile) {
        cerr << "exception opening/reading file" << endl;
        return {};
    }

    //suppose already annotated
    uint64_t byte_req = 0, byte_hit = 0, obj_req = 0, obj_hit = 0;
    uint64_t shadow_byte_req = 0, shadow_byte_hit = 0, shadow_obj_req = 0, shadow_obj_hit = 0;
    uint64_t t, id, size, next_t;
    uint64_t seg_byte_req = 0, seg_byte_hit = 0, seg_obj_req = 0, seg_obj_hit = 0;
    string seg_bhr;
    string seg_ohr;

    AnnotatedRequest req(0, 0, 0, 0);
    uint64_t seq = 0;
    auto t_now = system_clock::now();

    while (infile >> t >> id >> size >> next_t) {
        if (uni_size)
            size = 1;
        req.reinit(id, size, t, next_t);

        //shadow cache
//        if (seq <= sync_window) {
        {
            //update model
//            if (seq && !(seq % sync_window)) {
//                cerr << "training model" << endl;
//                if (seq == sync_window){
//                    cerr<<"copying cache state A -> B"<<endl;
//                    webcacheb->_currentSize = webcachea->_currentSize;
//                    webcacheb->key_map = webcachea->key_map;
//                    webcacheb->meta_holder[0] = webcachea->meta_holder[0];
//                    webcacheb->meta_holder[1] = webcachea->meta_holder[1];
//                    cerr<<webcachea->meta_holder[0].size();
//                } else if (seq > sync_window) {
//                    cerr<<"copying cache state B -> A"<<endl;
//                    webcachea->_currentSize = webcacheb->_currentSize;
//                    webcachea->key_map = webcacheb->key_map;
//                    webcachea->meta_holder[0] = webcacheb->meta_holder[0];
//                    webcachea->meta_holder[1] = webcacheb->meta_holder[1];
//                }
//            }

            if (seq == n_warmup) {
                cerr<<"reset shadow metric to align with brighten"<<endl;
                shadow_byte_hit = shadow_byte_req = shadow_obj_hit = shadow_obj_req = 0;
            }

            update_metric_req(shadow_byte_req, shadow_obj_req, size);
            //train
            if (webcachea->lookup(req, webcacheb->pending_gradients, webcacheb->weights, webcacheb->bias,
                    webcacheb->gradient_window))
                update_metric_req(shadow_byte_hit, shadow_obj_hit, size)
            else
                webcachea->admit(req);
        }


        //brighten cache. Not sure whether it is a good name
        {
            if (seq >= n_warmup)
                update_metric_req(byte_req, obj_req, size);
            update_metric_req(seg_byte_req, seg_obj_req, size);

            //eval
            //only start from window 2+
            if (webcacheb->lookup_without_update(req)) {
                if (seq >= n_warmup)
                    update_metric_req(byte_hit, obj_hit, size);
                update_metric_req(seg_byte_hit, seg_obj_hit, size);
            } else {
                webcacheb->admit(req);
            }
        }

        ++seq;
        if (!(seq%segment_window)) {
            auto _t_now = system_clock::now();
            cerr<<"\ndelta t: "<<duration_cast<seconds>(_t_now - t_now).count()<<endl;
            cerr<<"seq: " << seq << endl;
            cerr<<"brighten bhr: " << double(byte_hit) / byte_req << endl;
            cerr<<"shadow bhr: " << double(shadow_byte_hit) / shadow_byte_req << endl;
            double _seg_bhr = double(seg_byte_hit) / seg_byte_req;
            double _seg_ohr = double(seg_obj_hit) / seg_obj_req;
            //not exec this window
            if (!seg_byte_req) {
                _seg_bhr = _seg_ohr = 0;
            }
            cerr<<"seg bhr: " << _seg_bhr << endl;
            seg_bhr+=to_string(_seg_bhr)+"\t";
            seg_ohr+=to_string(_seg_ohr)+"\t";
            seg_byte_hit=seg_obj_hit=seg_byte_req=seg_obj_req=0;
            t_now = _t_now;
        }
    }

    infile.close();

    map<string, string> res = {
            {"byte_hit_rate", to_string(double(byte_hit) / byte_req)},
            {"object_hit_rate", to_string(double(obj_hit) / obj_req)},
            {"segment_byte_hit_rate", seg_bhr},
            {"segment_object_hit_rate", seg_ohr},
    };
    return res;
}