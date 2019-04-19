#include <algorithm>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iostream>
#include <list>
#include <map>
#include <math.h>
#include <random>
#include <regex>
#include <string>
#include <unordered_map>
#include <vector>
#include <LightGBM/application.h>
#include <LightGBM/c_api.h>
#include <utils.h>
#include "lfo.h"
#include "request.h"
#include "simulation_lfo.h"

#define HISTFEATURES 50

using namespace std;
using namespace chrono;

struct optEntry {
  uint64_t idx;
  uint64_t volume;
  bool hasNext;

  optEntry(uint64_t idx) : idx(idx), volume(numeric_limits<uint64_t>::max()), hasNext(false) {};
};

struct trEntry {
  uint64_t id;
  uint64_t size;
  double cost;
  bool toCache;

  trEntry(uint64_t id, uint64_t size, double cost) : id(id), size(size), cost(cost), toCache(false) {};
};

namespace LFO {

    random_device rd;  //Will be used to obtain a seed for the random number engine
    mt19937 gen(rd()); //Standard mersenne_twister_engine seeded with rd()
    uniform_real_distribution<> dis(0.0, 1.0);

    uint64_t cacheSize;
    uint64_t windowSize;
    uint64_t sampleSize;
    double cutoff;
    int sampling;
    bool init = true;
    BoosterHandle booster;
    unordered_map<string, string> trainParams = {
            {"boosting",                   "gbdt"},
            {"objective",                  "binary"},
            {"metric",                     "binary_logloss,auc"},
            {"metric_freq",                "1"},
            {"is_provide_training_metric", "true"},
            {"max_bin",                    "255"},
            {"num_iterations",             "50"},
            {"learning_rate",              "0.1"},
            {"num_leaves",                 "31"},
            {"tree_learner",               "serial"},
            {"num_threads",                "4"},
            {"feature_fraction",           "0.8"},
            {"bagging_freq",               "5"},
            {"bagging_fraction",           "0.8"},
            {"min_data_in_leaf",           "50"},
            {"min_sum_hessian_in_leaf",    "5.0"},
            {"is_enable_sparse",           "true"},
            {"two_round",                  "false"},
            {"save_binary",                "false"}
    };

// from (id, size) to idx
    unordered_map<pair<uint64_t, uint64_t>, uint64_t> windowLastSeen;
    vector<optEntry> windowOpt;
    vector<trEntry> windowTrace;
    uint64_t windowByteSum = 0;

//    ofstream resultFile;
//
    void calculateOPT() {
      auto timeBegin = chrono::system_clock::now();

      sort(windowOpt.begin(), windowOpt.end(), [](const optEntry &lhs, const optEntry &rhs) {
          return lhs.volume < rhs.volume;
      });

      uint64_t cacheVolume = cacheSize * windowSize;
      uint64_t currentVolume = 0;
      uint64_t hitc = 0;
      uint64_t bytehitc = 0;
      for (auto &it: windowOpt) {
        if (currentVolume > cacheVolume) {
          break;
        }
        if (it.hasNext) {
          windowTrace[it.idx].toCache = true;
          hitc++;
          bytehitc += windowTrace[it.idx].size;
          currentVolume += it.volume;
        }
      }
//      cerr << cacheSize << " " << windowSize << " " << double(hitc) / windowSize << " "
//                 << double(bytehitc) / windowByteSum << endl;
//
//      cerr << "Calculate OPT: "
//                 << chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - timeBegin).count()
//                 << " ms"
//                 << endl;
    }

// purpose: derive features and count how many features are inconsistent
    void deriveFeatures(vector<float> &labels, vector<int32_t> &indptr, vector<int32_t> &indices, vector<double> &data,
                        int sampling) {
//      auto timeBegin = chrono::system_clock::now();

      int64_t cacheAvailBytes = cacheSize;
      // from id to intervals
      unordered_map<uint64_t, list<uint64_t> > statistics;
      // from id to size
      unordered_map<uint64_t, uint64_t> cache;
      uint64_t negCacheSize = 0;

      uint64_t i = 0;
      indptr.push_back(0);
      for (auto &it: windowTrace) {
        auto &curQueue = statistics[it.id];
        const auto curQueueLen = curQueue.size();
        // drop features larger than 50
        if (curQueueLen > HISTFEATURES) {
          curQueue.pop_back();
        }

        bool flag = true;
        if (sampling == 1) {
          flag = i >= (windowSize - sampleSize);
        }
        if (sampling == 2) {
          double rand = dis(gen);
          flag = rand < (double) sampleSize / windowSize;
        }
        if (flag) {
          labels.push_back(it.toCache ? 1 : 0);

          // derive features
          int32_t idx = 0;
          uint64_t lastReqTime = i;
          for (auto &lit: curQueue) {
            const uint64_t dist = lastReqTime - lit; // distance
            indices.push_back(idx);
            data.push_back(dist);
            idx++;
            lastReqTime = lit;
          }

          // object size
          indices.push_back(HISTFEATURES);
          data.push_back(round(100.0 * log2(it.size)));

//      double currentSize = cacheAvailBytes <= 0 ? 0 : round(100.0 * log2(cacheAvailBytes));
//      indices.push_back(HISTFEATURES + 1);
//      data.push_back(currentSize);
          indices.push_back(HISTFEATURES + 2);
          data.push_back(it.cost);

          indptr.push_back(indptr[indptr.size() - 1] + idx + 2);
        }

        // update cache size
        if (cache.count(it.id) == 0) {
          // we have never seen this id
          if (it.toCache) {
            cacheAvailBytes -= it.size;
            cache[it.id] = it.size;
          }
        } else {
          // repeated request to this id
          if (!it.toCache) {
            // used to be cached, but not any more
            cacheAvailBytes += cache[it.id];
            cache.erase(it.id);
          }
        }

        if (cacheAvailBytes < 0) {
          negCacheSize++; // that's bad
        }

        // update queue
        curQueue.push_front(i++);
      }

      if (negCacheSize > 0) {
        cerr << "Negative cache size: " << negCacheSize << endl;
      }

//      cerr << "Derive features: "
//                 << chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - timeBegin).count()
//                 << " ms"
//                 << endl;
    }

    void evaluateModel(vector<double> &result) {
      // evaluate booster
      vector<float> labels;
      vector<int32_t> indptr;
      vector<int32_t> indices;
      vector<double> data;
      deriveFeatures(labels, indptr, indices, data, 0);
//      cerr << "Data size for evaluation: " << labels.size() << endl;

//      auto timeBegin = chrono::system_clock::now();
      int64_t len;
      result.resize(indptr.size() - 1);
      LGBM_BoosterPredictForCSR(booster, static_cast<void *>(indptr.data()), C_API_DTYPE_INT32, indices.data(),
                                static_cast<void *>(data.data()), C_API_DTYPE_FLOAT64,
                                indptr.size(), data.size(), HISTFEATURES + 3,
                                C_API_PREDICT_NORMAL, 0, trainParams, &len, result.data());

//      uint64_t fp = 0, fn = 0;
//
//      for (size_t i = 0; i < labels.size(); i++) {
//        if (labels[i] < cutoff && result[i] >= cutoff) {
//          fp++;
//        }
//        if (labels[i] >= cutoff && result[i] < cutoff) {
//          fn++;
//        }
//      }

//      cerr << cacheSize << " " << windowSize << " " << sampleSize << " " << cutoff << " " << sampling << " "
//                 << (double) fp / labels.size() << " " << (double) fn / labels.size() << endl;
//      cerr << "Evaluate model: "
//                 << chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - timeBegin).count()
//                 << " ms"
//                 << endl;
    }

    void trainModel(vector<float> &labels, vector<int32_t> &indptr, vector<int32_t> &indices, vector<double> &data) {
//      auto timeBegin = chrono::system_clock::now();

      // create training dataset
      DatasetHandle trainData;
      LGBM_DatasetCreateFromCSR(static_cast<void *>(indptr.data()), C_API_DTYPE_INT32, indices.data(),
                                static_cast<void *>(data.data()), C_API_DTYPE_FLOAT64,
                                indptr.size(), data.size(), HISTFEATURES + 3,
                                trainParams, nullptr, &trainData);
      LGBM_DatasetSetField(trainData, "label", static_cast<void *>(labels.data()), labels.size(), C_API_DTYPE_FLOAT32);

      if (init) {
        // init booster
        LGBM_BoosterCreate(trainData, trainParams, &booster);
        // train
        for (int i = 0; i < stoi(trainParams["num_iterations"]); i++) {
          int isFinished;
          LGBM_BoosterUpdateOneIter(booster, &isFinished);
          if (isFinished) {
            break;
          }
        }
        init = false;
      } else {
        BoosterHandle newBooster;
        LGBM_BoosterCreate(trainData, trainParams, &newBooster);

        // refit existing booster
//    resultFile << "Refit existing booster" << endl;
//    int64_t len;
//    LGBM_BoosterCalcNumPredict(booster, indptr.size() - 1, C_API_PREDICT_LEAF_INDEX, 0, &len);
//    vector<double> tmp(len);
//    LGBM_BoosterPredictForCSR(booster, static_cast<void*>(indptr.data()), C_API_DTYPE_INT32, indices.data(),
//                              static_cast<void*>(data.data()), C_API_DTYPE_FLOAT64,
//                              indptr.size(), data.size(), HISTFEATURES + 3,
//                              C_API_PREDICT_LEAF_INDEX, 0, trainParams, &len, tmp.data());
//    vector<int32_t> predLeaf(tmp.begin(), tmp.end());
//    tmp.clear();
//    LGBM_BoosterMerge(newBooster, booster);
//    LGBM_BoosterRefit(newBooster, predLeaf.data(), indptr.size() - 1, predLeaf.size() / (indptr.size() - 1));

        // alternative: train a new booster
//        cerr << "Train a new booster" << endl;
        for (int i = 0; i < stoi(trainParams["num_iterations"]); i++) {
          int isFinished;
          LGBM_BoosterUpdateOneIter(newBooster, &isFinished);
          if (isFinished) {
            break;
          }
        }
        LGBM_BoosterFree(booster);
        booster = newBooster;
      }
      LGBM_DatasetFree(trainData);

//      cerr << "Train model: "
//                 << chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - timeBegin).count()
//                 << " ms"
//                 << endl;
    }

    void annotate(uint64_t seq, uint64_t id, uint64_t size, double cost) {
      const uint64_t idx = (seq - 1) % windowSize;
      const auto idsize = make_pair(id, size);
      // why size would be <= 0?
      if (windowLastSeen.count(idsize) > 0) {
        windowOpt[windowLastSeen[idsize]].hasNext = true;
        windowOpt[windowLastSeen[idsize]].volume = (idx - windowLastSeen[idsize]) * size;
      }
      windowByteSum += size;
      windowLastSeen[idsize] = idx;
      windowOpt.emplace_back(idx);
      windowTrace.emplace_back(id, size, cost);
    }

    map<string, string> _simulation_lfo(string trace_file, string cache_type, uint64_t cache_size,
                                        map<string, string> params) {
    // create cache
    unique_ptr<Cache> webcache = move(Cache::create_unique(cache_type));
    if(webcache == nullptr) {
        cerr<<"cache type not implemented"<<endl;
        exit(-2);
    }

    // configure cache size
    webcache->setSize(cache_size);
    cacheSize = cache_size;

    uint64_t n_warmup = 0;
    bool uni_size = false;
    uint64_t segment_window = 1000000;
    uint n_extra_fields = 0;

    for (auto it = params.cbegin(); it != params.cend();) {
    if (it->first == "n_warmup") {
        n_warmup = stoull(it->second);
        it = params.erase(it);
    } else if (it->first == "uni_size") {
        uni_size = static_cast<bool>(stoi(it->second));
        it = params.erase(it);
    } else if (it->first == "segment_window") {
        segment_window = stoull((it->second));
        it = params.erase(it);
    } else if (it->first == "n_extra_fields") {
        n_extra_fields = stoull(it->second);
        ++it;
    } else if (it->first == "window") {
        windowSize = stoull(it->second);
        it = params.erase(it);
    } else if (it->first == "sample_size") {
        sampleSize = stoull(it->second);
        it = params.erase(it);
    } else if (it->first == "sample_type") {
        sampling = stoi(it->second);
        it = params.erase(it);
    } else if (it->first == "cutoff") {
        cutoff = stod(it->second);
        it = params.erase(it);
    } else {
        ++it;
    }
    }

    webcache->init_with_params(params);

//    auto timenow = chrono::system_clock::to_time_t(chrono::system_clock::now());
//    resultFile.open("/tmp/" + to_string(timenow));
//    resultFile << "Start: " << ctime(&timenow) << trace_file << " " << cacheSize << " " << windowSize << " "
//             << sampleSize
//             << " " << cutoff << " " << sampling << endl << endl;

    //suppose already annotated
    ifstream infile;
    infile.open(trace_file);
    if (!infile) {
        cerr << "exception opening/reading file" << endl;
        exit(-1);
    }
    //get whether file is in a correct format
    {
        std::string line;
        getline(infile, line);
        istringstream iss(line);
        int64_t tmp;
        int counter = 0;
        while (iss>>tmp) {++counter;}
        //format: t id size [extra]
        if (counter != 3+n_extra_fields) {
            cerr<<"error: input file column should be 3+n_extra_fields"<<endl;
            abort();
        }
        infile.clear();
        infile.seekg(0, ios::beg);
    }
    uint64_t byte_req = 0, byte_hit = 0, obj_req = 0, obj_hit = 0;
    //don't use real timestamp, use relative seq starting from 1
    uint64_t tmp, id, size;
    uint64_t seg_byte_req = 0, seg_byte_hit = 0, seg_obj_req = 0, seg_obj_hit = 0;
    string seg_bhr;
    string seg_ohr;

    uint64_t tmp2;

//    cerr << "simulating" << endl;
    ClassifiedRequest req(0, 0, 0);
    uint64_t train_seq = 0;
    uint64_t seq = 0;
    auto t_now = system_clock::now();
    while (infile >> tmp >> id >> size) {
        for (int i = 0; i < n_extra_fields; ++i)
            infile>>tmp2;

        if (uni_size) {
          size = 1;
        }

    if (train_seq && train_seq % windowSize == 0) {
      //train
//      auto timeBegin = chrono::system_clock::now();
//      auto timenow = chrono::system_clock::to_time_t(timeBegin);
      cerr << "Start processing window " << train_seq / windowSize <<endl; //<< ": " << ctime(&timenow);
      calculateOPT();
      vector<float> labels;
      vector<int32_t> indptr;
      vector<int32_t> indices;
      vector<double> data;
      deriveFeatures(labels, indptr, indices, data, sampling);
//      cerr << "Data size for training: " << labels.size() << endl;
      trainModel(labels, indptr, indices, data);

      windowByteSum = 0;
      windowLastSeen.clear();
      windowOpt.clear();
      windowTrace.clear();

//      auto timeEnd = chrono::system_clock::now();
//      timenow = chrono::system_clock::to_time_t(timeEnd);
//      cerr << "Finish processing window " << seq / windowSize << ": " << ctime(&timenow);
//      cerr << "Process window: " << chrono::duration_cast<chrono::milliseconds>(timeEnd - timeBegin).count()
//                 << " ms" << endl << endl;
    }

    train_seq++;

    annotate(train_seq, id, size, size);

    if (!init && train_seq % windowSize == 0) {
        //the end of a window
        //skip evaluation on first window
        vector<double> windowResult;
        evaluateModel(windowResult);

        // simulate cache
        //      auto begin = chrono::system_clock::now();
        auto rit = windowResult.begin();
        auto tit = windowTrace.begin();
        for (; rit != windowResult.end() && tit != windowTrace.end(); ++rit, ++tit) {
            //for each window request
            if (seq >= n_warmup)
                update_metric_req(byte_req, obj_req, tit->size);
            update_metric_req(seg_byte_req, seg_obj_req, tit->size);

            req.reinit(tit->id, tit->size, *rit);
            if (webcache->lookup(req)) {
                if (seq >= n_warmup)
                    update_metric_req(byte_hit, obj_hit, tit->size);
                update_metric_req(seg_byte_hit, seg_obj_hit, tit->size);
            } else {
              webcache->admit(req);
            }

            ++seq;
            if (!(seq%segment_window)) {
                auto _t_now = chrono::system_clock::now();
                cerr<<"delta t: "<<chrono::duration_cast<std::chrono::milliseconds>(_t_now - t_now).count()/1000.<<endl;
                cerr<<"seq: " << seq << endl;
                double _seg_bhr = double(seg_byte_hit) / seg_byte_req;
                double _seg_ohr = double(seg_obj_hit) / seg_obj_req;
                cerr<<"accu bhr: " << double(byte_hit) / byte_req << endl;
                cerr<<"seg bhr: " << _seg_bhr << endl;
                seg_bhr+=to_string(_seg_bhr)+"\t";
                seg_ohr+=to_string(_seg_ohr)+"\t";
                seg_byte_hit=seg_obj_hit=seg_byte_req=seg_obj_req=0;
                t_now = _t_now;
            }
        }
        cerr << "Window " << train_seq / windowSize << " byte hit rate: " << double(byte_hit) / byte_req << endl;
        //      cerr << "Simulate cache: "
        //                 << chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - begin).count()
        //                 << " ms"
        //                 << endl;
        //      cout << "Window " << seq / windowSize << " byte hit rate: " << double(byte_hit) / byte_req << endl;
        windowResult.clear();
        }
    }

    if (train_seq % windowSize != 0) {
        //the not mod part
        vector<double> windowResult;
        evaluateModel(windowResult);

        // simulate cache
    //    auto begin = chrono::system_clock::now();
        auto rit = windowResult.begin();
        auto tit = windowTrace.begin();
        for (; rit != windowResult.end() && tit != windowTrace.end(); ++rit, ++tit) {
            //for each window request
            if (seq >= n_warmup)
                update_metric_req(byte_req, obj_req, tit->size);
            update_metric_req(seg_byte_req, seg_obj_req, tit->size);

            req.reinit(tit->id, tit->size, *rit);
            if (webcache->lookup(req)) {
            if (seq >= n_warmup)
                update_metric_req(byte_hit, obj_hit, tit->size);
            update_metric_req(seg_byte_hit, seg_obj_hit, tit->size);
            } else {
            webcache->admit(req);
            }

            ++seq;
            if (!(seq%segment_window)) {
                auto _t_now = chrono::system_clock::now();
                cerr<<"delta t: "<<chrono::duration_cast<std::chrono::milliseconds>(_t_now - t_now).count()/1000.<<endl;
                cerr<<"seq: " << seq << endl;
                double _seg_bhr = double(seg_byte_hit) / seg_byte_req;
                double _seg_ohr = double(seg_obj_hit) / seg_obj_req;
                cerr<<"accu bhr: " << double(byte_hit) / byte_req << endl;
                cerr<<"seg bhr: " << _seg_bhr << endl;
                seg_bhr+=to_string(_seg_bhr)+"\t";
                seg_ohr+=to_string(_seg_ohr)+"\t";
                seg_byte_hit=seg_obj_hit=seg_byte_req=seg_obj_req=0;
                t_now = _t_now;
            }
        }
        cerr << "Window " << train_seq / windowSize << " byte hit rate: " << double(byte_hit) / byte_req << endl;
    //    cerr << "Simulate cache: "
    //               << chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - begin).count() << " ms"
    //               << endl;
    //    cout << "Window " << seq / windowSize << " byte hit rate: " << double(byte_hit) / byte_req << endl;
        windowResult.clear();
    }

    LGBM_BoosterFree(booster);
    infile.close();

        map<string, string> res = {
                {"byte_hit_rate", to_string(double(byte_hit) / byte_req)},
                {"object_hit_rate", to_string(double(obj_hit) / obj_req)},
                {"segment_byte_hit_rate", seg_bhr},
                {"segment_object_hit_rate", seg_ohr},
        };
    return res;
    }
}