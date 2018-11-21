#define LOG_ENABLED (false)
#define LOG_WRITE(...) do { } while(false)
#define LOG_EXIT() do { } while(false)

template<typename P, typename U, typename K>
inline void PriorityQueue<P,U,K>::swap(const size_t a, const size_t b) {
    assert(a < queue.size());
    assert(b < queue.size());

    // if (LOG_ENABLED) {
    //     LOG_WRITE("swap %u %p %u %.2f and %u %p %u %.2f", 
    //               a, (void*)q[a].pidx, *q[a].pidx, (double)q[a].priority, 
    //               b, (void*)q[b].pidx, *q[b].pidx, (double)q[b].priority);
    //     if(*q[a].pidx != a || *q[b].pidx != b) {
    //         LOG_EXIT();
    //     }
    // }

    Entry buf = queue[a];
    queue[a] = queue[b];
    queue[b] = buf;

    idxUpdater.priorityQueueUpdate(queue[a].key, a);
    idxUpdater.priorityQueueUpdate(queue[b].key, b);
}

template<typename P, typename U, typename K>
inline void PriorityQueue<P,U,K>::shift_up(size_t idx) {
    size_t pidx = parent_idx(idx);

    // LOG_WRITE("shift %d %x up", id, queue[idx].pidx);

    while (idx > 0) {
        //     LOG_WRITE("parent %d priority %.2f", pidx, queue[pidx].priority);
        if (queue[pidx].priority > queue[idx].priority) {
            swap(pidx, idx);
        } else {
            break;
        }
        idx = pidx; 
        pidx = parent_idx(idx);
    }
    // LOG_WRITE("shift %x up until %d %d", queue[idx].pidx, idx, *queue[idx].pidx);
}

template<typename P, typename U, typename K>
inline void PriorityQueue<P,U,K>::shift_down(size_t idx) {
    size_t lcidx = left_child_idx(idx);
    size_t rcidx = right_child_idx(idx);
    size_t cidx;
    // LOG_WRITE("shift %d %x down", idx, queue[idx].pidx);
    while (lcidx < queue.size()) {
        if (rcidx == queue.size()) {
            cidx = lcidx;
        } else {
            cidx = (queue[lcidx].priority < queue[rcidx].priority)? lcidx : rcidx;
        }
        // LOG_WRITE("smaller child idx %d priority %.2f", cidx, queue[cidx].priority);

        if (queue[idx].priority > queue[cidx].priority) {
            swap(idx, cidx);
        } else {
            break;
        }

        idx = cidx;
        lcidx = left_child_idx(idx);
        rcidx = right_child_idx(idx);
    } 
    // LOG_WRITE("shift %x down until %d %d", queue[idx].pidx, idx, *queue[idx].pidx);
}

template<typename P, typename U, typename K>
inline void PriorityQueue<P,U,K>::push(key_t key, priority_t priority) {
    Entry entry { key, priority };
    idxUpdater.priorityQueueUpdate(key, queue.size());
    // LOG_WRITE("push item %p, pq_idx %d, priority %.2f", (void*)entry.pidx, *entry.pidx, entry.priority);
    queue.push_back(entry);
    shift_up(queue.size()-1);
}

template<typename P, typename U, typename K>
inline typename PriorityQueue<P,U,K>::Entry PriorityQueue<P,U,K>::peek() const {
    assert(queue.size() > 1);

    // if (LOG_ENABLED) {
    //     Entry entry = queue[0];
    //     if (*entry.pidx != 0) {
    //         LOG_WRITE("find min item %x, pq_idx %d, priority %.2f", entry.pidx, *entry.pidx, entry.priority);
    //         LOG_EXIT();
    //     }
    // }
  
    return queue.front();
}

template<typename P, typename U, typename K>
inline typename PriorityQueue<P,U,K>::Entry PriorityQueue<P,U,K>::pop() {
    assert(queue.size() > 0);
    Entry ret = queue[0];
    erase(0);
    return ret;
}

template<typename P, typename U, typename K>
inline void PriorityQueue<P,U,K>::update(size_t idx, priority_t priority) {
    int old_priority = queue[idx].priority;
    // LOG_WRITE("pq update item %p, pq_idx %d, priority %.2f to %.2f",
    //           (void*)queue[idx].pidx, *queue[idx].pidx, old_priority, priority);
    queue[idx].priority = priority;
    if (priority < old_priority) {
        shift_up(idx);
    } else if (priority > old_priority) {
        shift_down(idx);
    }
}

template<typename P, typename U, typename K>
inline void PriorityQueue<P,U,K>::erase(size_t idx) {
    if (queue.empty()) { return; }

    // if (LOG_ENABLED) {
    //     LOG_WRITE("pq erase item %x, pq_idx %d, priority %.1f",
    //               queue[idx], *queue[idx].pidx, queue[idx].priority);
    //     if (idx > queue.size()) {
    //         LOG_DUMP();
    //     }
    // }
  
    assert(idx < queue.size());
    swap(idx, queue.size() - 1);
    queue.pop_back();
    shift_down(idx);
}
