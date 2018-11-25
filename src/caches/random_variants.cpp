//
// Created by zhenyus on 11/8/18.
//

#include "random_variants.h"

using namespace std;

bool RandomCache::lookup(SimpleRequest &req) {
    return key_space.exist(req._id);
}

void RandomCache::admit(SimpleRequest &req) {
    const uint64_t & size = req._size;
    // object feasible to store?
    if (size > _cacheSize) {
        LOG("L", _cacheSize, req.get_id(), size);
        return;
    }
    // admit new object
    key_space.insert(req._id);
    object_size.insert({req._id, req._size});
    _currentSize += size;

    // check eviction needed
    while (_currentSize > _cacheSize) {
        evict();
    }
}

void RandomCache::evict() {
    auto key = key_space.pickRandom();
    key_space.erase(key);
    auto & size = object_size.find(key)->second;
    _currentSize -= size;
    object_size.erase(key);
}

uint8_t Meta::_n_past_intervals = max_n_past_intervals;

bool LRCache::lookup(SimpleRequest &_req) {
    static uint64_t i = 0;
    if (!(i%1000000)) {
        cout << "mean diff: " << mean_diff << endl;
        for (int j = 0; j < n_past_intervals; ++j)
            cout << "weight " << j << ": " << weights[j] << endl;
        cout << "bias: " << bias << endl;
    }
    ++i;

    auto & req = static_cast<AnnotatedRequest &>(_req);
    auto it = key_map.find(req._id);
    if (it != key_map.end()) {
        //add timestamp
        //update past timestamps
        bool & list_idx = it->second.first;
        uint32_t & pos_idx = it->second.second;
//        auto tmp = meta_holder[list_idx][pos_idx];
        meta_holder[list_idx][pos_idx].append_past_timestamp(req._t);

        uint64_t _future_timestamp;
        if (req._t + threshold > req._next_t)
            _future_timestamp = req._next_t;
        else
            _future_timestamp = req._t + threshold;
        meta_holder[list_idx][pos_idx]._future_timestamp = _future_timestamp;

        return !list_idx;
        //update future timestamp. Can look only threshold far
//        auto it = unordered_future_timestamp.find(key);
//        if (it == unordered_future_timestamp.end()) {
//            insert
//            unordered_future_timestamp.insert({{req._id, req._size}, _future_timestamp});
//            future_timestamp.insert({{req._id, req._size}, _future_timestamp});
//        } else {
//            if has pending training data, put it
//            auto gradients = pending_gradients.find(req._t);
//            if (gradients != pending_gradients.end()) {
//                try_train(gradients->second);
//                pending_gradients.erase(gradients);
//            }
//            replace
//            it->second = _future_timestamp;
//            future_timestamp.left.replace_data(future_timestamp.left.find(key), _future_timestamp);
//        }

    }

//    try_gc(req._t);

//    return RandomCache::lookup(req);
}

//void LRCache::try_gc(uint64_t t) {
//    /*
//     * objects that never request again
//     * */
//    auto it = future_timestamp.right.begin();
//    while (it != future_timestamp.right.end()) {
//        if (it->first < t) {
//            auto gradients = pending_gradients.find(it->first);
//            if (gradients != pending_gradients.end()) {
//                try_train(gradients->second);
//                pending_gradients.erase(gradients);
//            }
//            past_timestamps.erase(it->second);
//            it = future_timestamp.right.erase(it);
//            unordered_future_timestamp.erase(it->second);
//        }
//        else
//            break;
//    }
//}

void LRCache::admit(SimpleRequest &_req) {
    AnnotatedRequest & req = static_cast<AnnotatedRequest &>(_req);
    const uint64_t & size = req._size;
    // object feasible to store?
    if (size > _cacheSize) {
        LOG("L", _cacheSize, req.get_id(), size);
        return;
    }

    if (size + _currentSize <= _cacheSize) {
        //never evict
        auto it = key_map.find(req._id);
        if (it == key_map.end()) {
            //fresh insert
            key_map.insert({req._id, {0, (uint32_t) meta_holder[0].size()}});
            uint64_t _future_timestamp;
            if (req._t + threshold > req._next_t)
                _future_timestamp = req._next_t;
            else
                _future_timestamp = req._t + threshold;
            meta_holder[0].emplace_back(req._id, req._size, req._t, _future_timestamp);
            _currentSize += size;
            return;
        } else {
            //bring list 1 to list 0
            uint32_t new_pos = meta_holder[0].size();
            meta_holder[0].emplace_back(meta_holder[1][it->second.second]);
            uint32_t deactivate_tail_pos = meta_holder[1].size()-1;
            if (it->second.second !=  deactivate_tail_pos) {
                //swap tail
                meta_holder[1][it->second.second] = meta_holder[1][deactivate_tail_pos];
                key_map.find(meta_holder[1][deactivate_tail_pos]._key)->second.second = it->second.second;
            }
            meta_holder[1].pop_back();
            it->second.first = 0;
            it->second.second = new_pos;
            _currentSize += size;
            return;
        }
    }

    auto it = key_map.find(req._id);
    if (it == key_map.end()) {
        //fresh insert
        key_map.insert({req._id, {0, (uint32_t) meta_holder[0].size()}});
        uint64_t _future_timestamp;
        if (req._t + threshold > req._next_t)
            _future_timestamp = req._next_t;
        else
            _future_timestamp = req._t + threshold;
        meta_holder[0].emplace_back(req._id, req._size, req._t, _future_timestamp);
        _currentSize += size;
    } else {
        //insert-evict
        auto epair = rank(req._t);
        auto & key0 = epair.first;
        auto & pos0 = epair.second;
        auto & key1 = req._id;
        auto & pos1 = it->second.second;
        _currentSize = _currentSize - meta_holder[0][pos0]._size + req._size;
        swap(meta_holder[0][pos0], meta_holder[1][pos1]);
        it->second.first = 0;
        it->second.second = pos0;
        auto iit = key_map.find(key0);
        iit->second.first = 1;
        iit->second.second = pos1;
    }
    // check eviction needed
    while (_currentSize > _cacheSize) {
        evict(req._t);
    }

}


pair<uint64_t, uint32_t> LRCache::rank(const uint64_t & t) {
    double past_intervals[max_n_past_intervals];
    double future_interval;
    double max_future_interval;
    uint64_t max_key;
    uint32_t max_pos;
    uint32_t i;
    uint8_t j;
    uint8_t past_timestamp_idx;
    uint64_t past_interval;

//    static uint64_t tmp_i = 0;
    uint32_t rand_idx = _distribution(_generator) % meta_holder[0].size();
    uint8_t n_sample;
    if (sample_rate < meta_holder[0].size())
        n_sample = sample_rate;
    else
        n_sample = meta_holder[0].size();

    for (i = 0; i < n_sample; i++) {
        uint32_t pos = (i+rand_idx)%meta_holder[0].size();
        auto & meta = meta_holder[0][pos];
//        assert(meta._size == 1);
//        auto it = key_map.find(meta._key);
//        assert(it != key_map.end());
        //fill in past_interval
        for (j = 0; j < meta._past_timestamp_idx && j < n_past_intervals; ++j) {
            past_timestamp_idx = (meta._past_timestamp_idx - 1 - j) % n_past_intervals;
            past_interval = t - meta._past_timestamps[past_timestamp_idx];
            if (past_interval >= threshold)
                past_intervals[j] = log1p_threshold;
            else
                past_intervals[j] = log1p(past_interval);
        }
        for (; j < n_past_intervals; j++)
            past_intervals[j] = log1p_threshold;

        future_interval = 0;
        for (j = 0; j < n_past_intervals; j++)
            future_interval += weights[j] * past_intervals[j];

        if (!i || future_interval > max_future_interval) {
            max_future_interval = future_interval;
            max_key = meta._key;
            max_pos = pos;
        }

        //append training data
//        uint64_t training_idx = unordered_future_timestamp.find(key)->second;
//        double diff = future_interval+bias - log1p(training_idx-t);
//        mean_diff = 0.99*mean_diff + 0.01*abs(diff);
//        cout<<mean_diff<<endl;
//        if (!(tmp_i%100000)) {
//            cout<<past_timestamps.size()<<endl;
//            ++tmp_i;
//
//        }

//        auto _pending_gradients = pending_gradients.find(training_idx);
//        if (_pending_gradients == pending_gradients.end()) {
//            auto gradients = new double[n_past_intervals+2];
//            for (int j = 0; j < n_past_intervals; j++)
//                gradients[j] = diff * past_intervals[j];
//            gradients[n_past_intervals] = diff;
//            gradients[n_past_intervals+1] = 1;
//            pending_gradients.insert({training_idx, gradients});
//        } else {
//            auto gradients = _pending_gradients->second;
//            for (int j = 0; j < n_past_intervals; j++)
//                gradients[j] += diff * past_intervals[j];
//            gradients[n_past_intervals] += diff;
//            gradients[n_past_intervals+1] += 1;
//        }
    }
    return {max_key, max_pos};
//    key_space.erase(max_key);
//    _currentSize -= max_key.second;
}

void LRCache::evict(const uint64_t & t) {
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

//void LRCache::try_train(double * gradients) {
//    //train
//    static double * weight_update;
//    static double bias_update;
//    static uint64_t n_update;
//
//    if (weight_update == nullptr) {
//        weight_update = new double[n_past_intervals];
//        for (int i = 0; i < n_past_intervals; i++)
//            weight_update[i] = 0;
//        bias_update = 0;
//        n_update = 0;
//    }
//
//    for (int i = 0; i < n_past_intervals; ++i)
//        weight_update[i] += gradients[i];
//    bias_update += gradients[n_past_intervals];
//    n_update += gradients[n_past_intervals+1];
//    delete [] gradients;
//
//    if (n_update >= batch_size) {
//        for (int i = 0; i < n_past_intervals; ++i)
//            weights[i] = weights[i] - learning_rate / n_update * weight_update[i];
//        bias = bias - learning_rate / n_update * bias_update;
//
//        for (int i = 0; i < n_past_intervals; ++i)
//            weight_update[i] = 0;
//        bias_update = 0;
//        n_update = 0;
//
//    }
//
//}

bool BeladySampleCache::lookup(SimpleRequest &_req) {
    auto & req = static_cast<AnnotatedRequest &>(_req);
    //add timestamp

    //update future timestamp. Can look only threshold far
    uint64_t next_t;
    if (req._t + threshold > req._next_t)
        next_t = req._next_t;
    else
        next_t = req._t + threshold;
    future_timestamp.find(req._id)->second = next_t;

//    try_train(req._t);

    return RandomCache::lookup(req);
}

void BeladySampleCache::admit(SimpleRequest &_req) {
    auto & req = static_cast<AnnotatedRequest &>(_req);
    const uint64_t size = req._size;
    // object feasible to store?
    if (size > _cacheSize) {
        LOG("L", _cacheSize, req.get_id(), size);
        return;
    }
    // admit new object
    key_space.insert(req._id);
    object_size.insert({req._id, req._size});
    _currentSize += size;

    // check eviction needed
    while (_currentSize > _cacheSize) {
        evict();
    }
}

void BeladySampleCache::evict() {
    static uint64_t future_interval;
    static uint64_t max_future_interval;
    static uint64_t max_key;

    for (int i = 0; i < sample_rate; i++) {
        const auto & key = key_space.pickRandom();
        future_interval = 0;

        //todo: use ground truth
        future_interval = future_timestamp.find(key)->second;

        if (!i || future_interval > max_future_interval) {
            max_future_interval = future_interval;
            max_key = key;
        }
    }

    key_space.erase(max_key);
    _currentSize -= object_size.find(max_key)->second;
}
