# webcachesim:
## a C++11 framework for simulating web caching policies

This simulator replays a request trace, and allows to experiment with various caching policies. The goal of this framework is flexibility to allow experimenting with and implementation of a wide variety of caching policies.

### Basic programming interface

The basic interface is simple:

    // create new cache
    unique_ptr<Cache> webcache = move(Cache::create_unique("yourPolicy"));
    // set cache capacity
    webcache->setSize(1000);
    // set an arbitrary param (parser implement by yourPolicy)
    webcache->setPar("param", ".5");

### Basic command line interface

There is also a simple CLI, with a general call format like this

    ./webcachesim traceFile warmUp cacheType log2CacheSize cacheParams

where

 - traceFile: a request trace (see below)
 - warmUp: the number of requests to skip before gathering hit/miss statistics
 - cacheType: one of the caching policies (see below)
 - log2CacheSize: the maximum cache capacity in bytes in logarithmic form (base 2)
 - cacheParams: optional cache parameters, can be used to tune cache policies (see below)

## Usage and current support for policies

### Request trace format

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

### Available caching policies

There are currently seven available caching policies.

#### LRU

does: least-recently used eviction

params: none

example (1GB capacity):

    ./webcachesim trace.txt 0 LRU 30
    
#### FIFO

does: first-in first-out eviction

params: none

example (1GB capacity):

    ./webcachesim trace.txt 0 FIFO 30
    
#### GDS

does: greedy dual size eviction

params: none

example (1GB capacity):

    ./webcachesim trace.txt 0 GDS 30
    
#### GDSF

does: greedy dual-size frequency eviction

params: none

example (1GB capacity):

    ./webcachesim trace.txt 0 GDSF 30
    
#### Filter-LRU

does: LRU eviction + admit only after N requests

params: n - admit after n requests)

example (1GB capacity, admit after 10 requests):

    ./webcachesim trace.txt 0 Filter 30 n=10
    
#### Threshold-LRU

does: LRU eviction + admit only after N requests

params: t - the size threshold in log form (base 2)

example (1GB capacity, admit only objects smaller than 512KB):

    ./webcachesim trace.txt 0 ThLRU 30 t=19
    
#### ExpProb-LRU

does: LRU eviction + admit with probability exponentially decreasing with object size

params: c - the size which has a 50% chance of being admitted (used to determine the exponential family)

example (1GB capacity, admit objects with size 256KB with about 50% probability):

    ./webcachesim trace.txt 0 ThLRU 30 c=18
    
#### ExpProb-LRU

does: LRU eviction + admit with probability exponentially decreasing with object size

params: c - the size which has a 50% chance of being admitted (used to determine the exponential family)

example (1GB capacity, admit objects with size 256KB with about 50% probability):

    ./webcachesim trace.txt 0 ThLRU 30 c=18
    
#### Segmented LRU (two segments)

does: segments cache capacity into two areas and does LRU eviction in each, a hit moves an object up one area to the next

params: either seg1 or seg2 = the fraction of the capacity assigned to the first or second segment, respectively (the rest goes to the other)

example (1GB capacity, each segment gets half the capacity)

    ./webcachesim trace.txt 0 S2LRU 30 seg1=.5

#### LRU-K

does: evict obejct which has oldest K-th reference in the past

params: k - eviction based on k-th reference in the past

example (1GB capacity, each segment gets half the capacity)

    ./webcachesim trace.txt 0 LRUK 30 k=4

## Example:

Download a public 1999 request trace ([trace description](http://www.cs.bu.edu/techreports/abstracts/1999-011)), rewrite it into our format, and run the simulator.

    wget http://www.cs.bu.edu/techreports/1999-011-usertrace-98.gz
    gunzip 1999-011-usertrace-98.gz
    g++ -o rewrite -std=c++11 ../helpers/rewrite_trace_http.cc
    ./rewrite 1999-011-usertrace-98 trace.txt
    make
    ./webcachesim trace.txt LRU 30 0 10
