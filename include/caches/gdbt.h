    //
    // Created by zhenyus on 1/16/19.
    //
    
    #ifndef WEBCACHESIM_GDBT_H
    #define WEBCACHESIM_GDBT_H
    
    #include "cache.h"
    #include "cache_object.h"
    #include <unordered_map>
    #include <vector>
    #include <random>
    #include <cmath>
    #include <LightGBM/c_api.h>
    
    using namespace std;
    
    
    class GDBTMeta {
    public:
        static uint8_t _max_n_past_timestamps;
        static uint8_t base_edwt_window;
        static uint8_t n_edwt_feature;
        static vector<double > edwt_windows;
        uint64_t _key;
        uint64_t _size;
        uint8_t _past_distance_idx;
        uint64_t _past_timestamp;
        vector<uint64_t> _past_distances;
        uint64_t _future_timestamp;
        vector<uint64_t> _extra_features;
        vector<double > _edwt;
    
        GDBTMeta(const uint64_t & key, const uint64_t & size, const uint64_t & past_timestamp,
                const uint64_t & future_timestamp, const vector<uint64_t> & extra_features) {
            _key = key;
            _size = size;
            _past_timestamp = past_timestamp;
            _past_distances = vector<uint64_t >(_max_n_past_timestamps);
            _past_distance_idx = (uint8_t) 0;
            _future_timestamp = future_timestamp;
            _extra_features = extra_features;
            _edwt = vector<double >(n_edwt_feature, 1);
        }
    
        inline void update(const uint64_t &past_timestamp, const uint64_t &future_timestamp) {
            //distance
            uint64_t _distance = past_timestamp - _past_timestamp;
            _past_distances[_past_distance_idx%_max_n_past_timestamps] = _distance;
            _past_distance_idx = _past_distance_idx + (uint8_t) 1;
            if (_past_distance_idx >= _max_n_past_timestamps * 2)
                _past_distance_idx -= _max_n_past_timestamps;
            //timestamp
            _past_timestamp = past_timestamp;
            _future_timestamp = future_timestamp;
            for (uint8_t i = 0; i < n_edwt_feature; ++i) {
                _edwt[i] = _edwt[i] * pow(0.5, _distance/edwt_windows[i]) + 1;
            }
        }
    };


    //init with a wrong value
    uint8_t GDBTMeta::_max_n_past_timestamps= 0;
    uint8_t GDBTMeta::base_edwt_window = 1;
    uint8_t GDBTMeta::n_edwt_feature = 1;
    vector<double > GDBTMeta::edwt_windows = vector<double >(GDBTMeta::n_edwt_feature);


    class GDBTTrainingData {
    public:
        // training info
        vector<float> labels;
        vector<int32_t> indptr = {0};
        vector<int32_t> indices;
        vector<double> data;
    };
    
    
    class GDBTCache : public Cache
    {
    public:
        //key -> (0/1 list, idx)
        unordered_map<uint64_t, pair<bool, uint32_t>> key_map;
        vector<GDBTMeta> meta_holder[2];
    
        // sample_size
        uint sample_rate = 32;
        // threshold
        uint64_t threshold = 10000000;
        double log1p_threshold = log1p(threshold);
        // n_past_interval
        uint8_t max_n_past_timestamps = 64;
    
        vector<GDBTTrainingData*> pending_training_data;
        uint64_t gradient_window = 100000;  //todo: does this large enough
    
        uint64_t n_extra_fields = 0;
        uint64_t n_feature;
        uint64_t training_sample_interval = 1;
        uint64_t num_threads = 1;
    
        BoosterHandle booster = nullptr;
    
        unordered_map<string, string> GDBT_train_params = {
                {"boosting",                   "gbdt"},
                {"objective",                  "regression"},
                {"num_iterations",             "1"},
                {"num_leaves",                  "32"},
                {"num_threads",                "1"},
                {"shrinkage_rate",           "0.1"},
                {"feature_fraction",           "0.8"},
                {"bagging_freq",               "5"},
                {"bagging_fraction",           "0.8"},
                {"learning_rate",              "0.1"},
        };
    
        double training_error = 0;
        double inference_error = 0;

        enum ObjectiveT: uint8_t {byte_hit_rate=0, object_hit_rate=1};
        ObjectiveT objective = byte_hit_rate;
    
        default_random_engine _generator = default_random_engine();
        uniform_int_distribution<std::size_t> _distribution = uniform_int_distribution<std::size_t>();
    
        GDBTCache()
            : Cache()
        {
        }
    
        void init_with_params(map<string, string> params) override {
            //set params
            for (auto& it: params) {
                if (it.first == "sample_rate") {
                    sample_rate = stoul(it.second);
                } else if (it.first == "threshold") {
                    threshold = stoull(it.second);
                    log1p_threshold = std::log1p(threshold);
                } else if (it.first == "max_n_past_timestamps") {
                    max_n_past_timestamps = (uint8_t) stoi(it.second);
                } else if (it.first == "gradient_window") {
                    gradient_window = stoull(it.second);
                } else if (it.first == "n_extra_fields") {
                    n_extra_fields = stoull(it.second);
                } else if (it.first == "num_iterations") {
                    GDBT_train_params["num_iterations"] = it.second;
                } else if (it.first == "learning_rate") {
                    GDBT_train_params["learning_rate"] = it.second;
                } else if (it.first == "num_threads") {
                    GDBT_train_params["num_threads"] = it.second;
                } else if (it.first == "training_sample_interval") {
                    training_sample_interval = stoull(it.second);
                } else if (it.first == "objective") {
                    if (it.second == "byte_hit_rate")
                        objective = byte_hit_rate;
                    else if (it.second == "object_hit_rate")
                        objective = object_hit_rate;
                    else {
                        cerr<<"error: unknown objective"<<endl;
                        exit(-1);
                    }
                } else {
                    cerr << "unrecognized parameter: " << it.first << endl;
                }
            }
    
            //init
            GDBTMeta::_max_n_past_timestamps = max_n_past_timestamps;
            GDBTMeta::base_edwt_window = 10;
            GDBTMeta::n_edwt_feature = 10;
            GDBTMeta::edwt_windows = vector<double >(GDBTMeta::n_edwt_feature);
            for (uint8_t i = 0; i < GDBTMeta::n_edwt_feature; ++i) {
                GDBTMeta::edwt_windows[i] = pow(2, GDBTMeta::base_edwt_window+i);
            }
            //interval, distances, size, extra_features, n_past_intervals, ewdt
            n_feature = max_n_past_timestamps + n_extra_fields + 2 + GDBTMeta::n_edwt_feature;
            if (n_extra_fields) {
                string categorical_feature = to_string(max_n_past_timestamps+1);
                for (uint i = 0; i < n_extra_fields-1; ++i) {
                    categorical_feature += ","+to_string(max_n_past_timestamps+2+i);
                }
                GDBT_train_params["categorical_feature"] = categorical_feature;
            }
        }
    
        virtual bool lookup(SimpleRequest& req);
        virtual void admit(SimpleRequest& req);
        virtual void evict(const uint64_t & t);
        void evict(SimpleRequest & req) {};
        void evict() {};
        //sample, rank the 1st and return
        pair<uint64_t, uint32_t > rank(const uint64_t & t);
        void try_train(uint64_t & t);
        void sample(uint64_t &t);
    };
    
    static Factory<GDBTCache> factoryGBDT("GDBT");
    #endif //WEBCACHESIM_GDBT_H
