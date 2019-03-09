#include <algorithm>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iostream>
#include <list>
#include <math.h>
#include <random>
#include <regex>
#include <string>
#include <unordered_map>
#include <vector>
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
        {"num_threads", "40"},
        {"feature_fraction", "0.8"},
        {"bagging_freq", "5"},
        {"bagging_fraction", "0.8"},
        {"min_data_in_leaf", "50"},
        {"min_sum_hessian_in_leaf", "5.0"},
        {"is_enable_sparse", "true"},
        {"two_round", "false"},
        {"save_binary", "false"}
};

// from (id, size) to idx
unordered_map<pair<uint64_t, uint64_t>, uint64_t> windowLastSeen;
vector<optEntry> windowOpt;
vector<trEntry> windowTrace;
uint64_t windowByteSum = 0;

ofstream resultFile;

void calculateOPT() {
  auto timeBegin = chrono::system_clock::now();

  sort(windowOpt.begin(), windowOpt.end(), [](const optEntry& lhs, const optEntry& rhs) {
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
  resultFile << cacheSize << " " << windowSize << " " << double(hitc) / windowSize << " " << double(bytehitc) / windowByteSum << endl;

  resultFile << "Calculate OPT: " << chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - timeBegin).count() << " ms" << endl;
}

// purpose: derive features and count how many features are inconsistent
void deriveFeatures(vector<float> &labels, vector<int32_t> &indptr, vector<int32_t> &indices, vector<double> &data, int sampling) {
  auto timeBegin = chrono::system_clock::now();

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
      flag = rand < (double)sampleSize/windowSize;
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

      double currentSize = cacheAvailBytes <= 0 ? 0 : round(100.0 * log2(cacheAvailBytes));
      indices.push_back(HISTFEATURES + 1);
      data.push_back(currentSize);
      indices.push_back(HISTFEATURES + 2);
      data.push_back(it.cost);

      indptr.push_back(indptr[indptr.size() - 1] + idx + 3);
    }

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

    if (cacheAvailBytes < 0) {
      negCacheSize++; // that's bad
    }

    // update queue
    curQueue.push_front(i++);
  }

  if (negCacheSize > 0) {
    resultFile << "Negative cache size: " << negCacheSize << endl;
  }

  resultFile << "Derive features: " << chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - timeBegin).count() << " ms" << endl;
}

void evaluateModel() {
  auto timeBegin = chrono::system_clock::now();

  // evaluate booster
  vector<float> labels;
  vector<int32_t> indptr;
  vector<int32_t> indices;
  vector<double> data;
  deriveFeatures(labels, indptr, indices, data, 0);
  resultFile << "Data size for evaluation: " << labels.size() << endl;
  int64_t len;
  vector<double> result(indptr.size() - 1);
  LGBM_BoosterPredictForCSR(booster, static_cast<void*>(indptr.data()), C_API_DTYPE_INT32, indices.data(),
                            static_cast<void*>(data.data()), C_API_DTYPE_FLOAT64,
                            indptr.size(), data.size(), HISTFEATURES + 3,
                            C_API_PREDICT_NORMAL, 0, trainParams, &len, result.data());

  uint64_t fp = 0, fn = 0;

  for (size_t i = 0; i < labels.size(); i++) {
    if (labels[i] < cutoff && result[i] >= cutoff) {
      fp++;
    }
    if (labels[i] >= cutoff && result[i] < cutoff) {
      fn++;
    }
  }

  resultFile << cacheSize << " " << windowSize << " " << sampleSize << " " << cutoff << " " << sampling << " " << (double) fp / labels.size() << " " << (double) fn / labels.size() << endl;
  resultFile << "Evaluate model: " << chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - timeBegin).count() << " ms" << endl;
}

void trainModel(vector<float> &labels, vector<int32_t> &indptr, vector<int32_t> &indices, vector<double> &data) {
  auto timeBegin = chrono::system_clock::now();

  // create training dataset
  DatasetHandle trainData;
  LGBM_DatasetCreateFromCSR(static_cast<void*>(indptr.data()), C_API_DTYPE_INT32, indices.data(),
                            static_cast<void*>(data.data()), C_API_DTYPE_FLOAT64,
                            indptr.size(), data.size(), HISTFEATURES + 3,
                            trainParams, nullptr, &trainData);
  LGBM_DatasetSetField(trainData, "label", static_cast<void*>(labels.data()), labels.size(), C_API_DTYPE_FLOAT32);

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
    resultFile << "Train a new booster" << endl;
    for (int i = 0; i < stoi(trainParams["num_iterations"]); i++) {
      int isFinished;
      LGBM_BoosterUpdateOneIter(newBooster, &isFinished);
      if (isFinished) {
        break;
      }
    }

    booster = newBooster;
  }

  resultFile << "Train model: " << chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - timeBegin).count() << " ms" << endl;
}

void processRequest(uint64_t seq, uint64_t id, uint64_t size, double cost) {
  const uint64_t idx = (seq - 1) % windowSize;
  const auto idsize = make_pair(id, size);
  // why size would be <= 0?
  if (size > 0 && windowLastSeen.count(idsize) > 0) {
    windowOpt[windowLastSeen[idsize]].hasNext = true;
    windowOpt[windowLastSeen[idsize]].volume = (idx - windowLastSeen[idsize]) * size;
  }
  windowByteSum += size;
  windowLastSeen[idsize] = idx;
  windowOpt.emplace_back(idx);
  windowTrace.emplace_back(id, size, cost);
  if (seq % windowSize == 0) { // the end of a window
    auto timeBegin = chrono::system_clock::now();
    auto timenow = chrono::system_clock::to_time_t(timeBegin);
    resultFile << "Start processing window " << seq / windowSize << ": " << ctime(&timenow);

    calculateOPT();

    if (!init) {
      evaluateModel();
    }

    vector<float> labels;
    vector<int32_t> indptr;
    vector<int32_t> indices;
    vector<double> data;
    deriveFeatures(labels, indptr, indices, data, sampling);
    resultFile << "Data size for training: " << labels.size() << endl;
    trainModel(labels, indptr, indices, data);

    windowByteSum = 0;
    windowLastSeen.clear();
    windowOpt.clear();
    windowTrace.clear();

    auto timeEnd = chrono::system_clock::now();
    timenow = chrono::system_clock::to_time_t(timeEnd);
    resultFile << "Finish processing window " << seq / windowSize << ": " << ctime(&timenow);
    resultFile << "Process window: " << chrono::duration_cast<chrono::milliseconds>(timeEnd - timeBegin).count() << " ms" << endl << endl;
  }
}

int main(int argc, char* argv[]) {
  if (argc < 6) {
    cerr << "parameters: tracePath cacheSize windowSize sampleSize cutoff sampling\n";
    return 1;
  }

  // input path
  const string tracePath = argv[1];
  // trace name
  string traceName = tracePath;
  const size_t dirSlashIdx = tracePath.rfind('/');
  if (string::npos != dirSlashIdx) {
    traceName = tracePath.substr(dirSlashIdx + 1, tracePath.length());
  }
  // cache size
  cacheSize = stoull(argv[2]);
  // window size
  windowSize = stoull(argv[3]);
  // sample size for training
  sampleSize = stoull(argv[4]);
  // cutoff
  cutoff = stod(argv[5]);
  // sampling: 0 means no sampling, 1 means most recent, 2 means random
  sampling = stoi(argv[6]);

  ifstream traceFile(tracePath);
  auto timenow = chrono::system_clock::to_time_t(chrono::system_clock::now());
  resultFile.open(tracePath + ".result." + to_string(timenow));
  resultFile << "Start: " << ctime(&timenow) << traceName << " " << cacheSize << " " << windowSize << " " << sampleSize << " " << cutoff << " " << sampling << endl << endl;

  uint64_t seq, id, size;
  double cost;
  // seq starts from 1
  while (traceFile >> seq >> id >> size >> cost) {
    processRequest(seq, id, size, cost);
  }

  return 0;
}
