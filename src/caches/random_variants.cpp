//
// Created by zhenyus on 11/8/18.
//

#include "random_variants.h"

using namespace std;





bool RandomCache::lookup(SimpleRequest &req) {
    return key_space.exist({req.get_id(), req.get_size()});
}

void RandomCache::admit(SimpleRequest &req) {
    const uint64_t size = req.get_size();
    // object feasible to store?
    if (size > _cacheSize) {
        LOG("L", _cacheSize, req.get_id(), size);
        return;
    }
    // admit new object
    key_space.insert({req.get_id(), req.get_size()});
    _currentSize += size;

    // check eviction needed
    while (_currentSize > _cacheSize) {
        evict();
    }
}

void RandomCache::evict() {
    auto key = key_space.pickRandom();
    key_space.erase(key);
    _currentSize -= key.second;
}

bool LRCache::lookup(SimpleRequest &_req) {
    AnnotatedRequest & req = static_cast<AnnotatedRequest &>(_req);
    //add timestamp
    auto &_past_timestamps = past_timestamps[{req._id, req._size}];
    const auto n_past_timestamps = _past_timestamps.size();

    //update past timestamps
    if (n_past_timestamps >= n_past_intervals)
        _past_timestamps.pop_back();
    _past_timestamps.push_front(req._t);

    //update future timestamp. Can look only threshold far
    if (req._t + threshold > req._next_t)
        future_timestamp[{req._id, req._size}] = req._next_t;
    else
        future_timestamp[{req._id, req._size}] = req._t + threshold;

    try_train(req._t);

    return RandomCache::lookup(req);
}

void LRCache::admit(SimpleRequest &_req) {
    AnnotatedRequest & req = static_cast<AnnotatedRequest &>(_req);
    const uint64_t size = req.get_size();
    // object feasible to store?
    if (size > _cacheSize) {
        LOG("L", _cacheSize, req.get_id(), size);
        return;
    }
    // admit new object
    key_space.insert({req.get_id(), req.get_size()});
    _currentSize += size;

    // check eviction needed
    while (_currentSize > _cacheSize) {
        evict(req._t);
    }
}

void LRCache::evict(uint64_t t) {
    static double * past_intervals;
    static double future_interval;
    static double max_future_interval;
    static pair<uint64_t, uint64_t> max_key;
//    static uint64_t tmp_i = 0;

    if (past_intervals == nullptr)
        past_intervals = new double[n_past_intervals];

    for (int i = 0; i < sample_rate; i++) {
        const auto & key = key_space.pickRandom();
        //fill in past_interval
        const auto & _past_timestamps = past_timestamps[{key.first, key.second}];
        int j = 0;
        for (auto & it: _past_timestamps) {
            past_intervals[j] = log1p(t - it);
            j++;
        }
        for (; j < n_past_intervals; j++)
            past_intervals[j] = log1p_threshold;
        future_interval = 0;
        for (j = 0; j < n_past_intervals; j++)
            future_interval += weights[j] * past_intervals[j];
        if (!i || future_interval > max_future_interval) {
            max_future_interval = future_interval;
            max_key = key;
        }
        //append training data
        uint64_t training_idx = future_timestamp[key];
        auto & _pending_gradients = pending_gradients[training_idx];
        double diff = future_interval+bias - log1p(training_idx-t);
        mean_diff = 0.5*mean_diff + 0.5*abs(diff);
//        cout<<mean_diff<<endl;
//        if (!(tmp_i%100000)) {
//            cout<<past_timestamps.size()<<endl;
//            ++tmp_i;
//
//        }

        for (j = 0; j < n_past_intervals; j++)
            _pending_gradients.push_back(diff * past_intervals[j]);
        _pending_gradients.push_back(diff);
    }

    key_space.erase(max_key);
    _currentSize -= max_key.second;
}

void LRCache::try_train(uint64_t t) {
    //train
    static double * weight_update;
    static double bias_update;
    static uint64_t n_update;

    if (weight_update == nullptr) {
        weight_update = new double[n_past_intervals];
        for (int i = 0; i < n_past_intervals; i++)
            weight_update[i] = 0;
        bias_update = 0;
        n_update = 0;
    }



    auto it = pending_gradients.cbegin();
    while (it != pending_gradients.cend()) {
//        double tmp = it->first;
//        auto tmp1 = it->second;
        if (it->first <= t) {
            int i = 0;
            for (const auto & iit: it->second) {
                if (i%(n_past_intervals+1) == n_past_intervals)
                    bias_update += iit;
                else {
                    weight_update[i%n_past_intervals] += iit;
                    ++n_update;
                }
                i++;
            }
            it = pending_gradients.erase(it);
        }
        else
        {
            ++it;
            break;
        }
    }
    if (n_update >= batch_size) {
        for (int i = 0; i < n_past_intervals; ++i)
            weights[i] = weights[i] - learning_rate / n_update * weight_update[i];
        bias = bias - learning_rate / n_update * bias_update;

        for (int i = 0; i < n_past_intervals; ++i)
            weight_update[i] = 0;
        bias_update = 0;
        n_update = 0;

    }

}

bool BeladySampleCache::lookup(SimpleRequest &_req) {
    auto & req = static_cast<AnnotatedRequest &>(_req);
    //add timestamp

    //update future timestamp. Can look only threshold far
    if (req._t + threshold > req._next_t)
        future_timestamp[{req._id, req._size}] = req._next_t;
    else
        future_timestamp[{req._id, req._size}] = req._t + threshold;

//    try_train(req._t);

    return RandomCache::lookup(req);
}

void BeladySampleCache::admit(SimpleRequest &_req) {
    auto & req = static_cast<AnnotatedRequest &>(_req);
    const uint64_t size = req.get_size();
    // object feasible to store?
    if (size > _cacheSize) {
        LOG("L", _cacheSize, req.get_id(), size);
        return;
    }
    // admit new object
    key_space.insert({req.get_id(), req.get_size()});
    _currentSize += size;

    // check eviction needed
    while (_currentSize > _cacheSize) {
        evict(req._t);
    }
}

void BeladySampleCache::evict(uint64_t t) {
    static double future_interval;
    static double max_future_interval;
    static pair<uint64_t, uint64_t> max_key;

    for (int i = 0; i < sample_rate; i++) {
        const auto & key = key_space.pickRandom();
        future_interval = 0;

        //todo: use ground truth
        future_interval = future_timestamp[key];

        if (!i || future_interval > max_future_interval) {
            max_future_interval = future_interval;
            max_key = key;
        }
    }

    key_space.erase(max_key);
    _currentSize -= max_key.second;
}
