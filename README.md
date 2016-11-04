# webcachesim
An open-source simulator for a variety of web caching policies.

The simulator runs on a request trace, you can configure several caching policies, the cache size, policy parameters, and a warm-up period before hit statistics are gathered.

The available caching policies are:
1. LRU
2. FIFO
3. GDS
4. GDSF
5. LRUK
6. LFUDA
7. S2LRU
8. S4LRU

Request traces must be given in a space-separated format:
| time |  id | size |
| ---- | --- | ---- |
|   1  |  1  |  120 |
|   2  |  2  |   64 |
|   3  |  1  |  120 |
|   4  |  3  |  14  |
|   4  |  1 |  120 |
