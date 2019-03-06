//
// Created by zhenyus on 1/16/19.
//

#include "gdbt.h"
#include <algorithm>
#include "utils.h"
#include <chrono>

using namespace chrono;
using namespace std;

void GDBTCache::try_train(uint64_t &t) {
    static uint64_t next_idx = 0;
    if (t < gradient_window)
        return;
    //look at previous window
    auto gradient_window_idx = t / gradient_window - 1;
    //already update
    if (gradient_window_idx != next_idx)
        return;
    ++next_idx;
    //perhaps no gradient at all
    if (gradient_window_idx >= pending_training_data.size())
        return;

    cerr<<"processing window idx: "<<gradient_window_idx<<endl;
    uint64_t pending_size = 0;
    for (auto i = gradient_window_idx; i < pending_training_data.size(); ++i)
        if (pending_training_data[i])
            pending_size += pending_training_data[i]->labels.size();
    cerr<<"pending data size: "<<pending_size<<endl;

    auto timeBegin = chrono::system_clock::now();
    auto &training_data = pending_training_data[gradient_window_idx];

    //training data not init
    if (!training_data)
        return;

    //no training data in the window
    if (training_data->labels.empty()) {
        delete pending_training_data[gradient_window_idx];
        return;
    }

    if (booster)
        LGBM_BoosterFree(booster);
    // create training dataset
    DatasetHandle trainData;
    LGBM_DatasetCreateFromCSR(
            static_cast<void *>(training_data->indptr.data()),
            C_API_DTYPE_INT32,
            training_data->indices.data(),
            static_cast<void *>(training_data->data.data()),
            C_API_DTYPE_FLOAT64,
            training_data->indptr.size(),
            training_data->data.size(),
            n_feature,  //remove future t
            GDBT_train_params,
            nullptr,
            &trainData);

    LGBM_DatasetSetField(trainData,
                         "label",
                         static_cast<void *>(training_data->labels.data()),
                         training_data->labels.size(),
                         C_API_DTYPE_FLOAT32);

    // init booster
    LGBM_BoosterCreate(trainData, GDBT_train_params, &booster);
    // train
    for (int i = 0; i < stoi(GDBT_train_params["num_iterations"]); i++) {
        int isFinished;
        LGBM_BoosterUpdateOneIter(booster, &isFinished);
        if (isFinished) {
            break;
        }
    }

    int64_t len;
    vector<double > result(training_data->indptr.size()-1);
    LGBM_BoosterPredictForCSR(booster,
                              static_cast<void *>(training_data->indptr.data()),
                              C_API_DTYPE_INT32,
                              training_data->indices.data(),
                              static_cast<void *>(training_data->data.data()),
                              C_API_DTYPE_FLOAT64,
                              training_data->indptr.size(),
                              training_data->data.size(),
                              n_feature,  //remove future t
                              C_API_PREDICT_NORMAL,
                              0,
                              GDBT_train_params,
                              &len,
                              result.data());
    double msr = 0;
    for (int i = 0; i < result.size(); ++i) {
        msr += pow((result[i] - training_data->labels[i]), 2);
    }
    msr /= result.size();
    training_error = training_error * 0.9 + msr *0.1;

//    vector<double > importance(n_feature);
//    LGBM_BoosterFeatureImportance(booster,
//            0,
//            0,
//            importance.data());
//    cout<<"i";
//    for (auto & it: importance)
//        cout<<" "<<it;
//    cout<<endl;

////    cerr<<"training l2: "<<msr<<endl;
//
////    vector<double > importance(max_n_past_timestamps+1);
////    int succeed = LGBM_BoosterFeatureImportance(booster,
////                                                0,
////                                                1,
////                                                importance.data());
////    cerr<<"\nimportance:\n";
////    for (auto & it: importance) {
////        cerr<<it<<endl;
////    }
////    char *json_model = new char[10000000];
////    int64_t out_len;
////    LGBM_BoosterSaveModelToString(booster,
////            0,
////            -1,
////            10000000,
////            &out_len,
////            json_model);
////    cout<<json_model<<endl;
////
////    LGBM_BoosterDumpModel(booster,
////                          0,
////                          -1,
////                          10000000,
////                          &out_len,
////                          json_model);
////    cout<<json_model<<endl;


    LGBM_DatasetFree(trainData);
    delete pending_training_data[gradient_window_idx];
    cerr << "Training time: "
               << chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - timeBegin).count()
               << " ms"
               << endl;

}

void GDBTCache::sample(uint64_t &t) {
    if (meta_holder[0].empty() || meta_holder[1].empty())
        return;
#ifdef LOG_SAMPLE_RATE
    bool log_flag = ((double) rand() / (RAND_MAX)) < LOG_SAMPLE_RATE;
#endif

    //sample list 0
    {
        uint32_t rand_idx = _distribution(_generator) % meta_holder[0].size();
        uint n_sample = min( (uint) ceil((double) training_sample_interval*meta_holder[0].size()/(meta_holder[0].size()+meta_holder[1].size())),
                             (uint) meta_holder[0].size());


        for (uint32_t i = 0; i < n_sample; i++) {
            uint32_t pos = (i + rand_idx) % meta_holder[0].size();

            auto &meta = meta_holder[0][pos];

            //don't train at the first threshold
            if (meta._future_timestamp <= threshold)
                continue;

            uint64_t known_future_interval;
            double obj;
            //known_future_interval < threshold
            if (meta._future_timestamp - t < threshold) {
                known_future_interval = meta._future_timestamp - t;
                obj = log1p(known_future_interval);
            }
            else {
                known_future_interval = threshold-1;
                obj = log1p_threshold;
            }

             //update gradient
            auto gradient_window_idx = (t + known_future_interval) / gradient_window;
            if (gradient_window_idx >= pending_training_data.size())
                pending_training_data.resize(gradient_window_idx + 1);
            if (!pending_training_data[gradient_window_idx])
                pending_training_data[gradient_window_idx] = new GDBTTrainingData;
            auto &training_data = pending_training_data[gradient_window_idx];
            uint64_t counter = training_data->indptr[training_data->indptr.size()-1];

            //fill in past_interval
            if ((t - meta._past_timestamp) < threshold) {
                //gdbt don't need to log
                uint64_t p_i = (t - meta._past_timestamp);
                training_data->indices.push_back(0);
                training_data->data.push_back(p_i);
                ++counter;
            }

            uint8_t j = 0;
            uint64_t this_past_timestamp = meta._past_timestamp;
            for (j = 0; j < meta._past_distance_idx && j < max_n_past_timestamps-1; ++j) {
                uint8_t past_distance_idx = (meta._past_distance_idx - 1 - j) % max_n_past_timestamps;
                uint64_t & past_distance = meta._past_distances[past_distance_idx];
                this_past_timestamp -= past_distance;
                if (this_past_timestamp > t - threshold) {
                    training_data->indices.push_back(j+1);
                    training_data->data.push_back(past_distance);
                    ++counter;
                }
            }

            training_data->indices.push_back(max_n_past_timestamps);
            training_data->data.push_back(meta._size);
            ++counter;

            for (uint k = 0; k < n_extra_fields; ++k) {
                training_data->indices.push_back(max_n_past_timestamps + k + 1);
                training_data->data.push_back(meta._extra_features[k]);
                ++counter;
            }

            training_data->indices.push_back(max_n_past_timestamps+n_extra_fields+1);
            training_data->data.push_back(j);
            ++counter;

            for (uint8_t k = 0; k < GDBTMeta::n_edwt_feature; ++k) {
                training_data->indices.push_back(max_n_past_timestamps + n_extra_fields + 2 + k);
                training_data->data.push_back(meta._edwt[k]*pow(0.5, (t - meta._past_timestamp) /
                GDBTMeta::edwt_windows[k]));
                ++counter;
            }

            training_data->labels.push_back(obj);
            training_data->indptr.push_back(counter);
        }
    }

//    cout<<n_out_window<<endl;

    //sample list 1
    if (meta_holder[1].size()){
        uint32_t rand_idx = _distribution(_generator) % meta_holder[1].size();
        //sample less from list 1 as there are gc
        uint n_sample = min( (uint) floor( (double) training_sample_interval*meta_holder[1].size()/(meta_holder[0].size()+meta_holder[1].size())),
                             (uint) meta_holder[1].size());
//        cout<<n_sample<<endl;

        for (uint32_t i = 0; i < n_sample; i++) {
            //garbage collection
            while (meta_holder[1].size()) {
                uint32_t pos = (i + rand_idx) % meta_holder[1].size();
                auto &meta = meta_holder[1][pos];
                uint64_t & past_timestamp = meta._past_timestamp;
                if (past_timestamp + threshold < t) {
                    uint64_t & ekey = meta._key;
                    key_map.erase(ekey);
                    //evict
                    uint32_t tail1_pos = meta_holder[1].size()-1;
                    if (pos !=  tail1_pos) {
                        //swap tail
                        meta_holder[1][pos] = meta_holder[1][tail1_pos];
                        key_map.find(meta_holder[1][tail1_pos]._key)->second.second = pos;
                    }
                    meta_holder[1].pop_back();
                } else
                    break;
            }
            if (!meta_holder[1].size())
                break;
            uint32_t pos = (i + rand_idx) % meta_holder[1].size();

            auto &meta = meta_holder[1][pos];

            //don't train at the first threshold
            if (meta._future_timestamp <= threshold)
                continue;

            uint64_t known_future_interval;
            double obj;
            //known_future_interval < threshold
            if (meta._future_timestamp - t < threshold) {
                known_future_interval = meta._future_timestamp - t;
                obj = log1p(known_future_interval);
            }
            else {
                known_future_interval = threshold-1;
                obj = log1p_threshold;
            }

             //update gradient
            auto gradient_window_idx = (t + known_future_interval) / gradient_window;
            if (gradient_window_idx >= pending_training_data.size())
                pending_training_data.resize(gradient_window_idx + 1);
            if (!pending_training_data[gradient_window_idx])
                pending_training_data[gradient_window_idx] = new GDBTTrainingData;
            auto &training_data = pending_training_data[gradient_window_idx];
            uint64_t counter = training_data->indptr[training_data->indptr.size()-1];

            //fill in past_interval
            if ((t - meta._past_timestamp) < threshold) {
                //gdbt don't need to log
                uint64_t p_i = (t - meta._past_timestamp);
                training_data->indices.push_back(0);
                training_data->data.push_back(p_i);
                ++counter;
            }

            uint8_t j = 0;
            uint64_t this_past_timestamp = meta._past_timestamp;
            for (j = 0; j < meta._past_distance_idx && j < max_n_past_timestamps-1; ++j) {
                uint8_t past_distance_idx = (meta._past_distance_idx - 1 - j) % max_n_past_timestamps;
                uint64_t & past_distance = meta._past_distances[past_distance_idx];
                this_past_timestamp -= past_distance;
                if (this_past_timestamp > t - threshold) {
                    training_data->indices.push_back(j+1);
                    training_data->data.push_back(past_distance);
                    ++counter;
                }
            }

            training_data->indices.push_back(max_n_past_timestamps);
            training_data->data.push_back(meta._size);
            ++counter;

            for (uint k = 0; k < n_extra_fields; ++k) {
                training_data->indices.push_back(max_n_past_timestamps + k + 1);
                training_data->data.push_back(meta._extra_features[k]);
                ++counter;
            }

            training_data->indices.push_back(max_n_past_timestamps+n_extra_fields+1);
            training_data->data.push_back(j);
            ++counter;

            for (uint8_t k = 0; k < GDBTMeta::n_edwt_feature; ++k) {
                training_data->indices.push_back(max_n_past_timestamps + n_extra_fields + 2 + k);
                training_data->data.push_back(meta._edwt[k]*pow(0.5, (t - meta._past_timestamp) /
                                                                     GDBTMeta::edwt_windows[k]));
                ++counter;
            }

            training_data->labels.push_back(obj);
            training_data->indptr.push_back(counter);
        }
    }
}


bool GDBTCache::lookup(SimpleRequest &_req) {
    auto & req = dynamic_cast<AnnotatedRequest &>(_req);
    static uint64_t i = 0;
    if (!(i%1000000)) {
        cerr << "training error: " << training_error << " "<< "inference error: " << inference_error << endl;
        cerr << "list 0 size: "<<meta_holder[0].size()<<" "<<"list 1 size: "<<meta_holder[1].size()<<endl;
    }
    ++i;

    try_train(req._t);
    if (!(i % training_sample_interval))
        sample(req._t);

    auto it = key_map.find(req._id);
    if (it != key_map.end()) {
        //update past timestamps
        bool & list_idx = it->second.first;
        uint32_t & pos_idx = it->second.second;
        meta_holder[list_idx][pos_idx].update(req._t, req._next_seq);
        return !list_idx;
    }
    return false;
}

void GDBTCache::admit(SimpleRequest &_req) {
    AnnotatedRequest & req = static_cast<AnnotatedRequest &>(_req);
    const uint64_t & size = req._size;
    // object feasible to store?
    if (size > _cacheSize) {
        LOG("L", _cacheSize, req.get_id(), size);
        return;
    }

    auto it = key_map.find(req._id);
    if (it == key_map.end()) {
        //fresh insert
        key_map.insert({req._id, {0, (uint32_t) meta_holder[0].size()}});
        meta_holder[0].emplace_back(req._id, req._size, req._t, req._next_seq, req._extra_features);
        _currentSize += size;
        if (_currentSize <= _cacheSize)
            return;
    } else if (size + _currentSize <= _cacheSize){
        //bring list 1 to list 0
        //first move meta data, then modify hash table
        uint32_t tail0_pos = meta_holder[0].size();
        meta_holder[0].emplace_back(meta_holder[1][it->second.second]);
        uint32_t tail1_pos = meta_holder[1].size()-1;
        if (it->second.second !=  tail1_pos) {
            //swap tail
            meta_holder[1][it->second.second] = meta_holder[1][tail1_pos];
            key_map.find(meta_holder[1][tail1_pos]._key)->second.second = it->second.second;
        }
        meta_holder[1].pop_back();
        it->second = {0, tail0_pos};
        _currentSize += size;
        return;
    } else {
        //insert-evict
        auto epair = rank(req._t);
        auto & key0 = epair.first;
        auto & pos0 = epair.second;
        auto & pos1 = it->second.second;
        _currentSize = _currentSize - meta_holder[0][pos0]._size + req._size;
        swap(meta_holder[0][pos0], meta_holder[1][pos1]);
        swap(it->second, key_map.find(key0)->second);
    }
    // check more eviction needed?
    while (_currentSize > _cacheSize) {
        evict(req._t);
    }
}


pair<uint64_t, uint32_t> GDBTCache::rank(const uint64_t & t) {
    uint32_t rand_idx = _distribution(_generator) % meta_holder[0].size();
    //if not trained yet, use random
    if (booster == nullptr) {
        return {meta_holder[0][rand_idx]._key, rand_idx};
    }

    uint n_sample;
    if (sample_rate < meta_holder[0].size())
        n_sample = sample_rate;
    else
        n_sample = meta_holder[0].size();

    vector<double> label;
    vector<int32_t> indptr = {0};
    vector<int32_t> indices;
    vector<double> data;
    vector<double> sizes;

    uint64_t counter = 0;
    for (uint32_t i = 0; i < n_sample; i++) {
        uint32_t pos = (i+rand_idx)%meta_holder[0].size();
        auto & meta = meta_holder[0][pos];
        //fill in past_interval
        if ((t - meta._past_timestamp) < threshold) {
            //gdbt don't need to log
            uint64_t p_i = (t - meta._past_timestamp);
            indices.push_back(0);
            data.push_back(p_i);
            ++counter;
        }

        uint8_t j = 0;
        uint64_t this_past_timestamp = meta._past_timestamp;
        for (j = 0; j < meta._past_distance_idx && j < max_n_past_timestamps-1; ++j) {
            uint8_t past_distance_idx = (meta._past_distance_idx - 1 - j) % max_n_past_timestamps;
            uint64_t & past_distance = meta._past_distances[past_distance_idx];
            this_past_timestamp -= past_distance;
            if (this_past_timestamp > t - threshold) {
                indices.push_back(j+1);
                data.push_back(past_distance);
                ++counter;
            }
        }

        indices.push_back(max_n_past_timestamps);
        data.push_back(meta._size);
        sizes.push_back(meta._size);
        ++counter;


        for (uint k = 0; k < n_extra_fields; ++k) {
            indices.push_back(max_n_past_timestamps + k + 1);
            data.push_back(meta._extra_features[k]);
            ++counter;
        }

        indices.push_back(max_n_past_timestamps+n_extra_fields+1);
        data.push_back(j);
        ++counter;

        for (uint8_t k = 0; k < GDBTMeta::n_edwt_feature; ++k) {
            indices.push_back(max_n_past_timestamps + n_extra_fields + 2 + k);
            data.push_back(meta._edwt[k]*pow(0.5, (t - meta._past_timestamp) /
                                                                 GDBTMeta::edwt_windows[k]));
            ++counter;
        }

        if (meta._future_timestamp - t < threshold) {
            //gdbt don't need to log
            uint64_t p_f = (meta._future_timestamp - t);
            //known_future_interval < threshold
            label.push_back(log1p(p_f));
        } else {
            label.push_back(log1p_threshold);
        }

        //remove future t
        indptr.push_back(counter);

        //add future t
//        indices.push_back(MAX_N_INTERVAL+2);
//        data.push_back(meta._future_timestamp-t);
//        indptr.push_back(indptr[indptr.size() - 1] + j + 3);
    }
    int64_t len;
    uint32_t sample_pos;
    vector<double> result(n_sample);
    LGBM_BoosterPredictForCSR(booster,
                              static_cast<void *>(indptr.data()),
                              C_API_DTYPE_INT32,
                              indices.data(),
                              static_cast<void *>(data.data()),
                              C_API_DTYPE_FLOAT64,
                              indptr.size(),
                              data.size(),
                              n_feature,  //remove future t
                              C_API_PREDICT_NORMAL,
                              0,
                              GDBT_train_params,
                              &len,
                              result.data());
    double msr = 0;
    for (int i = 0; i < result.size(); ++i) {
        msr += pow((result[i] - label[i]), 2);
    }
    msr /= result.size();
//        cerr<<"inference l2: "<<msr<<endl;
    inference_error = inference_error * 0.9 + msr *0.1;
    if (objective == object_hit_rate)
        for (uint32_t i = 0; i < n_sample; ++i)
            result[i] += log1p(sizes[i]);
    auto max_it = max_element(result.begin(), result.end());
    sample_pos = (uint32_t) distance(result.begin(), max_it);

    uint32_t max_pos = (sample_pos+rand_idx)%meta_holder[0].size();
    auto & meta = meta_holder[0][max_pos];
    auto & max_key = meta._key;

    return {max_key, max_pos};
}

void GDBTCache::evict(const uint64_t & t) {
    auto epair = rank(t);
    uint64_t & key = epair.first;
    uint32_t & old_pos = epair.second;

    //bring list 0 to list 1
    uint32_t new_pos = meta_holder[1].size();

    meta_holder[1].emplace_back(meta_holder[0][old_pos]);
    uint32_t activate_tail_idx = meta_holder[0].size()-1;
    if (old_pos !=  activate_tail_idx) {
        //move tail
        meta_holder[0][old_pos] = meta_holder[0][activate_tail_idx];
        key_map.find(meta_holder[0][activate_tail_idx]._key)->second.second = old_pos;
    }
    meta_holder[0].pop_back();

    auto it = key_map.find(key);
    it->second.first = 1;
    it->second.second = new_pos;
    _currentSize -= meta_holder[1][new_pos]._size;
}


