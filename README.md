# webcachesim: a simple C++ framework for simulating web caching policies

The webcachesimsource simulator replays a request trace, and you can configure several caching policies, the cache size, policy parameters, and a warm-up period before hit statistics are gathered.

# Request trace format

Request traces must be given in a space-separated format with three colums
- time should be a long int, but can be arbitrary (for future TTL feature, not currently in use)
- id should be a long int, used to uniquely identify objects
- size should be a long int, the obejct size

| time |  id | size |
| ---- | --- | ---- |
|   1  |  1  |  120 |
|   2  |  2  |   64 |
|   3  |  1  |  120 |
|   4  |  3  |  14  |
|   4  |  1 |  120 |

# Available caching policies
The available caching policies are:

1. LRU (least-recently used eviction)
2. Filter-LRU (LRU + admit after N requests)
3. Threshold-LRU (LRU + admit if object size is less than threshold)
4. ExpProb-LRU (LRU + admit with probability exponentially decreasing with object size)
2. FIFO (first-in first-out eviction)
3. GDS (greedy dual size eviction)
4. GDSF (greedy dual-size frequency eviction)
5. LRU-K (evict obejct which has oldest K-th reference in the past)
6. LFUDA (least-frequently used eviction with dynamic agin)
7. S2LRU (segmented LRU, two segments)
8. S4LRU (segmented LRU, four segments)

# Parameters of caching policies

no parameters:

1. LRU
2. FIFO
3. GDS
4. GDSF
6. LFUDA

one parameter:

2. Filter-LRU (N: - admit after N requests)
3. Threshold-LRU (t: - the size threshold)
4. ExpProb-LRU (c: the size which has a 50% chance of being admitted (used to determine the exponential family))
5. LRU-K (K: K-th reference in the past)
7. S2LRU (s: fraction of capacity assigned to first segment)
8. S4LRU (s: fraction of capacity assigned to first segment, rest is shared equally by other segments)
