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

using spp::sparse_hash_map;

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

        //try to use less this because it need to grub lock
        sparse_hash_map<uint64_t, uint64_t> size_map;
        shared_mutex size_map_mutex;


        bool lookup(SimpleRequest &req) override {
            //back pressure to prevent op_queue too long
            static int counter = 0;
            if ((++counter) % 10000) {
                while (true) {
                    op_queue_mutex.lock();
                    if (op_queue.size() > 1000) {
                        op_queue_mutex.unlock();
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    } else {
                        op_queue_mutex.unlock();
                        break;
                    }
                }
            }
            ++counter;
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
        void parallel_admit
                (const uint64_t &key, const int64_t &size, const uint16_t extra_features[max_n_extra_feature]) {
            if (size > _cacheSize)
                return;
            size_map_mutex.lock_shared();
            auto it = size_map.find(key);
            if (it == size_map.end() || !(it->second)) {
                size_map_mutex.unlock_shared();
                OpT op = {.key=key, .size=size};
                for (uint8_t i = 0; i < n_extra_fields; ++i)
                    op._extra_features[i] = extra_features[i];
                op_queue_mutex.lock();
                op_queue.push(op);
                op_queue_mutex.unlock();
            } else {
                //already inserted
                size_map_mutex.unlock_shared();
            }
        }

        uint64_t parallel_lookup(const uint64_t &key) {
            uint64_t ret = 0;
            size_map_mutex.lock_shared();
            auto it = size_map.find(key);
            if (it != size_map.end()) {
                ret = it->second;
                size_map_mutex.unlock_shared();
                op_queue_mutex.lock();
                op_queue.push(OpT{.key=key, .size=-1});
                op_queue_mutex.unlock();
            } else {
                size_map_mutex.unlock_shared();
            }
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
        std::queue<OpT> op_queue;
        std::mutex op_queue_mutex;
        std::thread print_status_thread;
    private:
        uint8_t n_extra_fields = 0;

        virtual void print_stats() {
            //no lock because read fail doesn't hurt much
            std::cerr << "\nop queue length: " << op_queue.size() << std::endl;
            std::cerr << "async size_map len: " << size_map.size() << std::endl;
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
                op_queue_mutex.lock();
                if (!op_queue.empty()) {
                    OpT op = op_queue.front();
                    op_queue.pop();
                    op_queue_mutex.unlock();
                    if (op.size < 0) {
                        async_lookup(op.key);
                    } else {
                        async_admit(op.key, op.size, op._extra_features);
                    }
                } else {
                    op_queue_mutex.unlock();
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            }
        }
    };

}

#endif //WEBCACHESIM_PARALLEL_CACHE_H
