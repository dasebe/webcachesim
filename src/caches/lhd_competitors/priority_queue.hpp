#ifndef PRIORITY_QUEUE_H
#define PRIORITY_QUEUE_H

#include <limits.h>

template<typename PriorityType, typename IndexUpdater, typename KeyType>
class PriorityQueue {
  public:
    typedef PriorityType priority_t;
    typedef KeyType key_t;

    PriorityQueue(IndexUpdater& _idxUpdater)
        : queue(0)
        , idxUpdater(_idxUpdater) {
    }
    
    ~PriorityQueue() {
        
    }

    struct Entry {
        key_t key;
        priority_t priority;
    };

    void push(key_t key, priority_t priority);
    Entry pop();
    Entry peek() const;
    void update(size_t idx, priority_t priority);
    void erase(size_t idx);

  private:

    inline void shift_up(size_t idx);
    inline void shift_down(size_t idx);

    static inline size_t parent_idx(size_t idx) { return (idx-1) / 2; }
    static inline size_t left_child_idx(size_t idx) { return idx * 2 + 1; }
    static inline size_t right_child_idx(size_t idx) { return left_child_idx(idx) + 1; }

    inline void swap(const size_t a, const size_t b);

    std::vector<Entry> queue;
    IndexUpdater &idxUpdater;
};

#include "priority_queue_impl.hpp"

#endif
