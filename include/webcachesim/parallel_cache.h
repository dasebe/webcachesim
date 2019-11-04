//
// Created by zhenyus on 11/2/19.
//

#ifndef WEBCACHESIM_PARALLEL_CACHE_H
#define WEBCACHESIM_PARALLEL_CACHE_H

#include "cache.h"
#include <queue>
#include <mutex>
#include <thread>
#include "sparsepp/spp.h"
#include <shared_mutex>

using spp::sparse_hash_map;

namespace webcachesim {

    struct OpT {
        uint64_t key;
        //-1 means get command
        int64_t size;
    };


    class ParallelCache : public Cache {
    public:
        ~ParallelCache() {
            keep_running.clear();
            if (lookup_get_thread.joinable())
                lookup_get_thread.join();
        }

        //try to use less this because it need to grub lock
        sparse_hash_map<uint64_t, uint64_t> size_map;
        shared_mutex size_map_mutex;

        //the cache receive these msgs to update the states, but not need to reply
        virtual void async_lookup(const uint64_t &key) = 0;

        virtual void async_admit(const uint64_t &key, const int64_t &size) = 0;

        //the client call this, expecting fast return
        void parallel_admit(uint64_t &key, int64_t &size) {
            if (size > _cacheSize)
                return;
            size_map_mutex.lock_shared();
            auto it = size_map.find(key);
            if (it == size_map.end() || !(it->second)) {
                size_map_mutex.unlock_shared();
                op_queue_mutex.lock();
                op_queue.push(OpT{.key=key, .size=size});
                op_queue_mutex.unlock();
            } else {
                //already inserted
                size_map_mutex.unlock_shared();
            }
        }

        uint64_t parallel_lookup(uint64_t &key) {
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
            keep_running.test_and_set();
            lookup_get_thread = std::thread(&ParallelCache::async_lookup_get, this);
        }

    protected:
        std::thread lookup_get_thread;
        std::atomic_flag keep_running = ATOMIC_FLAG_INIT;
    private:
        //op queue
        std::queue<OpT> op_queue;
        std::mutex op_queue_mutex;

        void async_lookup_get() {
            while (keep_running.test_and_set()) {
                op_queue_mutex.lock();
                if (!op_queue.empty()) {
                    OpT op = op_queue.front();
                    op_queue.pop();
                    op_queue_mutex.unlock();
                    if (op.size < 0) {
                        async_lookup(op.key);
                    } else {
                        async_admit(op.key, op.size);
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
