//
// Created by zhenyus on 11/2/19.
//

#ifndef WEBCACHESIM_PARALLEL_CACHE_H
#define WEBCACHESIM_PARALLEL_CACHE_H

#include <api.h>
#include "cache.h"
#include <queue>
#include <mutex>
#include <thread>
#include "sparsepp/spp.h"
#include <shared_mutex>
#include <atomic>
#include <chrono>
#include <boost/lockfree/queue.hpp>

using spp::sparse_hash_map;
using namespace chrono;
using namespace std;

namespace webcachesim {
    struct OpT {
        uint64_t key;
        //-1 means get command
        int64_t size;
        uint16_t _extra_features[max_n_extra_feature];
    };

    class ParallelCache : public Cache {
    public:
        ~ParallelCache() {
            keep_running = false;
            if (lookup_get_thread.joinable())
                lookup_get_thread.join();
            if (print_status_thread.joinable())
                print_status_thread.join();
        }

        //sharded by 32. Assuming 32 threads
        static const int n_shard = 64;
        sparse_hash_map<uint64_t, uint64_t> size_map[n_shard];
        shared_mutex size_map_mutex[n_shard];


        bool lookup(SimpleRequest &req) override {
            //fixed size freelock queue will block on push when queue is full
            return parallel_lookup(req._id);
        }

        void admit(SimpleRequest &req) override {
            int64_t size = req._size;
            parallel_admit(req._id, size, req._extra_features.data());
        }

        //the cache receive these msgs to update the states, but not need to reply
        virtual void async_lookup(const uint64_t &key) = 0;

        virtual void async_admit(
                const uint64_t &key, const int64_t &size, const uint16_t extra_features[max_n_extra_feature]) = 0;

        //the client call this, expecting fast return
        virtual void parallel_admit
                (const uint64_t &key, const int64_t &size, const uint16_t extra_features[max_n_extra_feature]) {
            if (size > _cacheSize)
                return;
            auto shard_id = key%n_shard;
            size_map_mutex[shard_id].lock_shared();
            auto it = size_map[shard_id].find(key);
            if (it == size_map[shard_id].end() || !(it->second)) {
                size_map_mutex[shard_id].unlock_shared();
                OpT op = {.key=key, .size=size};
                for (uint8_t i = 0; i < n_extra_fields; ++i)
                    op._extra_features[i] = extra_features[i];
                while (! op_queue.push(op));
            } else {
                //already inserted
                size_map_mutex[shard_id].unlock_shared();
            }
        }

        virtual uint64_t parallel_lookup(const uint64_t &key) {
            uint64_t ret = 0;
//            system_clock::time_point timeBegin;
            auto shard_id = key%n_shard;
            size_map_mutex[shard_id].lock_shared();
//            timeBegin = chrono::system_clock::now();
            auto it = size_map[shard_id].find(key);
            if (it != size_map[shard_id].end()) {
                ret = it->second;
                size_map_mutex[shard_id].unlock_shared();
                while(!op_queue.push(OpT{.key=key, .size=-1}));
            } else {
                size_map_mutex[shard_id].unlock_shared();
            }
//            auto duration = chrono::duration_cast<chrono::microseconds>(chrono::system_clock::now() - timeBegin).count();
//            cout<<"d: "<<duration<<endl;
            return ret;
        }

        void init_with_params(const map<string, string> &params) override {
            for (auto &it: params) {
                if (it.first == "n_extra_fields") {
                    n_extra_fields = stoul(it.second);
                }
            }
            lookup_get_thread = std::thread(&ParallelCache::async_lookup_get, this);
            print_status_thread = std::thread(&ParallelCache::async_print_status, this);
        }

    protected:
        std::thread lookup_get_thread;
        std::atomic<bool> keep_running = true;
        //op queue
        boost::lockfree::queue<OpT, boost::lockfree::capacity<4096>> op_queue;
//        std::queue<OpT> op_queue;
        std::thread print_status_thread;
    private:
        uint8_t n_extra_fields = 0;

        virtual void print_stats() {
            //no lock because read fail doesn't hurt much
//            std::cerr << "\nop queue length: " << op_queue.size() << std::endl;
//            std::cerr << "async size_map len: " << size_map.size() << std::endl;
            std::cerr << "cache size: " << _currentSize << "/" << _cacheSize << " ("
                      << ((double) _currentSize) / _cacheSize
                      << ")" << std::endl;
//                      << "in/out metadata " << in_cache_metas.size() << " / " << out_cache_metas.size() << std::endl;
//            std::cerr << "n_training: "<<training_data->labels.size()<<std::endl;

//        std::cerr << "training loss: " << training_loss << std::endl;
//        std::cerr << "n_force_eviction: " << n_force_eviction <<std::endl;
//        std::cerr << "training time: " << training_time <<std::endl;
//        std::cerr << "inference time: " << inference_time <<std::endl;
        }

        void async_print_status() {
            while (keep_running) {
                std::this_thread::sleep_for(std::chrono::seconds(10));
                print_stats();
            }
        }

        void async_lookup_get() {
            while (keep_running) {
                OpT op;
                if (op_queue.pop(op)) {
                    if (op.size < 0) {
                        async_lookup(op.key);
                    } else {
                        async_admit(op.key, op.size, op._extra_features);
                    }
                }
            }
        }
    };

}

#endif //WEBCACHESIM_PARALLEL_CACHE_H
