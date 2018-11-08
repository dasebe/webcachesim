//
// Created by zhenyus on 11/8/18.
//

#include "simulation_lfo.h"
#include <fstream>
#include "request.h"
#include "lfo.h"
#include <chrono>
#include <unordered_map>
#include <list>
#include <regex>
#include <math.h>
#include <vector>
#include <algorithm>
#include <LightGBM/application.h>
#include <LightGBM/c_api.h>

#define HISTFEATURES 50

using namespace std;

// from boost hash combine: hashing of pairs for unordered_maps
template <class T>
inline void hash_combine(size_t & seed, const T & v) {
  hash<T> hasher;
  seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

namespace std {
  template<typename S, typename T>
  struct hash<pair<S, T>> {
    inline size_t operator()(const pair<S, T> &v) const {
      size_t seed = 0;
      hash_combine(seed, v.first);
      hash_combine(seed, v.second);
      return seed;
    }
  };
}

struct optEntry {
  uint64_t idx;
  uint64_t volume;
  bool hasNext;

  optEntry(uint64_t idx): idx(idx), volume(numeric_limits<uint64_t>::max()), hasNext(false) {};
};

struct trEntry {
  uint64_t id;
  uint64_t size;
  double cost;
  bool toCache;

  trEntry(uint64_t id, uint64_t size, double cost) : id(id), size(size), cost(cost), toCache(false) {};
};

uint64_t cacheSize;
uint64_t windowSize;
double cutoff;
bool init = true;
BoosterHandle booster;

// from (id, size) to idx
unordered_map<pair<uint64_t, uint64_t>, uint64_t> windowLastSeen;
vector<optEntry> windowOpt;
vector<trEntry> windowTrace;
vector<double> window_result;
uint64_t windowByteSum = 0;

ofstream resultFile;
void calculateOPT() {
  sort(windowOpt.begin(), windowOpt.end(), [](const optEntry& lhs, const optEntry& rhs) {
    return lhs.volume < rhs.volume;
  });

  uint64_t cacheVolume = cacheSize * windowSize;
  uint64_t currentVolume = 0;
  uint64_t hitc = 0;
  uint64_t bytehitc = 0;
  for (auto &it: windowOpt) {
    if (currentVolume > cacheVolume) {
      resultFile << "break" << endl;
      break;
    }
    if (it.hasNext) {
      windowTrace[it.idx].toCache = true;
      hitc++;
      bytehitc += windowTrace[it.idx].size;
      currentVolume += it.volume;
    }
  }
  resultFile << "PFOO-L ohr: " << double(hitc)/windowSize << " bhr: " << double(bytehitc)/windowByteSum << endl;
}

// purpose: derive features and count how many features are inconsistent
void deriveFeatures(vector<float> &labels, vector<int32_t> &indptr, vector<int32_t> &indices, vector<double> &data) {
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
    data.push_back(round(100.0*log2(it.size)));

    double currentSize;
    if (cacheAvailBytes <= 0) {
      if (cacheAvailBytes < 0) {
        negCacheSize++; // that's bad
      }
      currentSize = 0;
    } else {
      currentSize = round(100.0*log2(cacheAvailBytes));
    }
    indices.push_back(HISTFEATURES + 1);
    data.push_back(currentSize);
    indices.push_back(HISTFEATURES + 2);
    data.push_back(it.cost);

    indptr.push_back(indptr[i] + idx + 3);

    // update cache size
    if (cache.count(it.id) == 0) {
      // we have never seen this id
      if(it.toCache) {
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

    // update queue
    curQueue.push_front(i++);
  }

  resultFile << "neg. cache size: " << negCacheSize << endl;
}

void check(const vector<float> &labels, const vector<double> &result) {
  uint64_t fp = 0, fn = 0;

  for (size_t i = 0; i < labels.size(); i++) {
    if (labels[i] < cutoff && result[i] >= cutoff) {
      fp++;
    }
    if (labels[i] >= cutoff && result[i] < cutoff) {
      fn++;
    }
  }

  resultFile << cutoff << " " << labels.size() << " " << fp << " " << fn << " " << endl;
}

void trainModel(vector<float> &labels, vector<int32_t> &indptr, vector<int32_t> &indices, vector<double> &data) {
  unordered_map<string, string> trainParams = {
          {"boosting", "gbdt"},
          {"objective", "binary"},
          {"metric", "binary_logloss,auc"},
          {"metric_freq", "1"},
          {"is_provide_training_metric", "true"},
          {"max_bin", "255"},
          {"num_iterations", "50"},
          {"learning_rate", "0.1"},
          {"num_leaves", "31"},
          {"tree_learner", "serial"},
          {"num_threads", "64"},
          {"feature_fraction", "0.8"},
          {"bagging_freq", "5"},
          {"bagging_fraction", "0.8"},
          {"min_data_in_leaf", "50"},
          {"min_sum_hessian_in_leaf", "5.0"},
          {"is_enable_sparse", "true"},
          {"two_round", "false"},
          {"save_binary", "false"}
  };

  // create training dataset
  DatasetHandle trainData;
  LGBM_DatasetCreateFromCSR(static_cast<void*>(indptr.data()), C_API_DTYPE_INT32, indices.data(),
                            static_cast<void*>(data.data()), C_API_DTYPE_FLOAT64,
                            indptr.size(), data.size(), HISTFEATURES + 3,
                            trainParams, nullptr, &trainData);
  LGBM_DatasetSetField(trainData, "label", static_cast<void*>(labels.data()), labels.size(), C_API_DTYPE_FLOAT32);

  if (init) {
    // init booster
    resultFile << "init booster" << endl;
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
    // evaluate booster
    int64_t len;
    vector<double> result(indptr.size() - 1);
//    LGBM_BoosterPredictForCSR(booster, static_cast<void*>(indptr.data()), C_API_DTYPE_INT32, indices.data(),
//                              static_cast<void*>(data.data()), C_API_DTYPE_FLOAT64,
//                              indptr.size(), data.size(), HISTFEATURES + 3,
//                              C_API_PREDICT_RAW_SCORE, 0, trainParams, &len, result.data());
    LGBM_BoosterPredictForCSR(booster, static_cast<void*>(indptr.data()), C_API_DTYPE_INT32, indices.data(),
                              static_cast<void*>(data.data()), C_API_DTYPE_FLOAT64,
                              indptr.size(), data.size(), HISTFEATURES + 3,
                              C_API_PREDICT_NORMAL, 0, trainParams, &len, result.data());
    check(labels, result);
    for (auto &it: result) {
        window_result.emplace_back(it);
    }
    // refit existing booster
    resultFile << "refit booster" << endl;
    LGBM_BoosterCalcNumPredict(booster, indptr.size() - 1, C_API_PREDICT_LEAF_INDEX, 0, &len);
    result.resize(len);
    LGBM_BoosterPredictForCSR(booster, static_cast<void*>(indptr.data()), C_API_DTYPE_INT32, indices.data(),
                              static_cast<void*>(data.data()), C_API_DTYPE_FLOAT64,
                              indptr.size(), data.size(), HISTFEATURES + 3,
                              C_API_PREDICT_LEAF_INDEX, 0, trainParams, &len, result.data());
    BoosterHandle newBooster;
    LGBM_BoosterCreate(trainData, trainParams, &newBooster);
    LGBM_BoosterMerge(newBooster, booster);
    vector<int32_t> predLeaf(result.begin(), result.end());
    result.clear();
    LGBM_BoosterRefit(newBooster, predLeaf.data(), indptr.size() - 1, predLeaf.size() / (indptr.size() - 1));
    booster = newBooster;
  }

  auto timenow = chrono::system_clock::to_time_t(chrono::system_clock::now());
  resultFile << ctime(&timenow) << "finish training" << endl;
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


typedef std::vector<double >::const_iterator iter_double;
typedef std::vector<trEntry>::const_iterator iter_trEntry;


map<string, string> _simulation_lfo(string trace_file, string cache_type, uint64_t cache_size,
                                       map<string, string> params){
    // create cache
    unique_ptr<Cache> webcache = move(Cache::create_unique(cache_type));
    if(webcache == nullptr) {
        cerr<<"cache type not implemented"<<endl;
        return {};
    }

    // configure cache size
    webcache->setSize(cache_size);
    cacheSize = cache_size;

    bool uni_size = false;
    for (auto& kv: params) {
        webcache->setPar(kv.first, kv.second);
        if (kv.first == "window")
            windowSize = stoull(kv.second);
        if (kv.first == "cutoff")
            cutoff = stod(kv.second);
        if (kv.first == "uni_size")
            uni_size = static_cast<bool>(stoi(kv.second));
    }


    ifstream traceFile(trace_file);
    auto timenow = chrono::system_clock::to_time_t(chrono::system_clock::now());
    resultFile.open("/tmp/" + to_string(timenow));
    resultFile << ctime(&timenow) << trace_file << " " << cacheSize << " " << windowSize << " " << cutoff << endl;


    //suppose already annotated
    ifstream infile;
    uint64_t byte_req = 0, byte_hit = 0, obj_req = 0, obj_hit = 0;
    uint64_t t, id, size, next_t;

    infile.open(trace_file);
    if (!infile) {
        cerr << "exception opening/reading file"<<endl;
        return {};
    }


    cout<<"simulating"<<endl;
    ClassifiedRequest req(0, 0, 0);
    int i = 0;
    while (infile >> t >> id >> size) {
        if (uni_size)
            size = 1;
        annotate(t, id, size, size);
        //todo: make sure no  tail segment left at the trace
        if (t % windowSize == 0) { // the end of a window
            cout << "windowTrace size: " << windowOpt.size() << endl;
            calculateOPT();

            vector<float> labels;
            vector<int32_t> indptr;
            vector<int32_t> indices;
            vector<double> data;
            deriveFeatures(labels, indptr, indices, data);
            trainModel(labels, indptr, indices, data);

            //skip evaluation on first window
            if (t != windowSize) {
//                cout << "window result len: " << window_result.size() << " window trace len: " << windowTrace.size()
//                     << endl;
                auto rit = window_result.begin();
                auto tit = windowTrace.begin();
                for (; rit != window_result.end() && tit != windowTrace.end(); ++rit, ++tit) {
                    //for each window request
                    byte_req += tit->size;
                    obj_req++;

                    req.reinit(id, tit->size, *rit);
                    if (webcache->lookup(req)) {
                        byte_hit += tit->size;
                        obj_hit++;
                    } else {
                        webcache->admit(req);
                    }
                }
                window_result.clear();
            }
            //        cout << i << " " << t << " " << obj_hit << endl;

            windowByteSum = 0;
            windowLastSeen.clear();
            windowOpt.clear();
            windowTrace.clear();
        }
        if (!(++i % 1000000))
            cout << i << endl;
    }

    infile.close();

    map<string, string> res = {
            {"byte_hit_rate", to_string(double(byte_hit) / byte_req)},
            {"object_hit_rate", to_string(double(obj_hit) / obj_req)},
    };
    return res;
}