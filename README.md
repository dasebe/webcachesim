# webcachesim:

## Webcachesim v2 is available, with many improvements. Please it check out [HERE](https://github.com/sunnyszy/lrb).

## A simulator for CDN caching and web caching policies.

Simulate a variety of existing caching policies by replaying request traces, and use this framework as a basis to experiment with new ones.

The webcachesim simulator was built for the [AdaptSize project](https://github.com/dasebe/AdaptSize), see [References](#references) for more information.


## Example simulation results

We replay production traffic from a CDN server operated by the [Wikimedia Foundation](https://wikimediafoundation.org/wiki/Home), and consider various modern caching policies.

<img src="https://cloud.githubusercontent.com/assets/9959772/22966302/de4de664-f361-11e6-9345-854bffa2005c.png" width=500px />

## Compiling webcachesim

You will need a compiler that supports C++11, e.g., GCC 4.8.1 upwards (with -std=c++11). Older compilers that partially support C++11, e.g., GCC 4.4, can compile (with -std=c++0x).

Then, get started with

    make


## Using an exisiting policy

The basic interface is

    ./webcachesim traceFile cacheType cacheSize [cacheParams]

where

 - traceFile: a request trace (see below)
 - cacheType: one of the caching policies (see below)
 - cacheSize: the cache capacity in bytes
 - cacheParams: optional cache parameters, can be used to tune cache policies (see below)

### Request trace format

Request traces must be given in a space-separated format with three colums
- time should be a long long int, but can be arbitrary (for future TTL feature, not currently in use)
- id should be a long long int, used to uniquely identify objects
- size should be a long long int, this is object's size in bytes

| time |  id | size |
| ---- | --- | ---- |
|   1  |  1  |  120 |
|   2  |  2  |   64 |
|   3  |  1  |  120 |
|   4  |  3  |  14  |
|   4  |  1 |  120 |

Example trace in file "test.tr".

### Available caching policies

There are currently ten caching policies. This section describes each one, in turn, its parameters, and how to run it on the "test.tr" example trace with cache size 1000 Bytes.

#### LRU

does: least-recently used eviction

params: none

example usage:

    ./webcachesim test.tr LRU 1000
     
#### FIFO

does: first-in first-out eviction

params: none

example usage:

    ./webcachesim test.tr FIFO 1000
    
#### GDS

does: greedy dual size eviction

params: none

example usage:

    ./webcachesim test.tr GDS 1000
    
#### GDSF

does: greedy dual-size frequency eviction

params: none

example usage:

    ./webcachesim test.tr GDSF 1000
    
#### LFU-DA

does: least-frequently used eviction with dynamic aging

params: none

example usage:

    ./webcachesim test.tr LFUDA 1000
    
    
#### Filter-LRU

does: LRU eviction + admit only after N requests

params: n - admit after n requests)

example usage (admit after 10 requests):

    ./webcachesim test.tr Filter 1000 n=10
    
#### Threshold-LRU

does: LRU eviction + admit only after N requests

params: t - the size threshold in log form (base 2)

example usage (admit only objects smaller than 512KB):

    ./webcachesim test.tr ThLRU 1000 t=19
    
#### ExpProb-LRU

does: LRU eviction + admit with probability exponentially decreasing with object size

params: c - the size which has a 50% chance of being admitted (used to determine the exponential family)

example usage (admit objects with size 256KB with about 50% probability):

    ./webcachesim test.tr ExpLRU 1000 c=18
  
#### LRU-K

does: evict object which has oldest K-th reference in the past

params: k - eviction based on k-th reference in the past

example usage (each segment gets half the capacity)

    ./webcachesim test.tr LRUK 1000 k=4

#### AdaptSize (version 0.1)

does: uses adaptive ExpLRU (ExpProb-LRU) policy that adapts with request traffic, [adapted from the official implementation](https://github.com/dasebe/AdaptSize)

params: t - reconfiguration interval (default 500K), i - numeric iteration (precision, default 15)

example usage

    ./webcachesim test.tr AdaptSize 1000 t=1000000 i=5


## How to get traces:


### Generate your own traces with a given distribution

One example is a Pareto (Zipf-like) popularity distribution and Bounded-Pareto object size distribution.
The "basic_trace" tool takes the following parameters:

 - how many unique objects
 - how many requests to generate for most popular object (total request length will be a multiple of that)
 - Pareto shape
 - min object size
 - max object size
 - output name for trace

Here's an example that recreates the "test.tr" trace for the examples above. This uses the "basic_trace" generator with 1000 objects, about 10000 requests overall, Pareto shape 1.8 and object sizes between 1 and 10000 bytes.

    g++ tracegenerator/basic_trace.cc -std=c++11 -o basic_trace
    ./basic_trace 1000 1000 1.8 1 10000 test.tr
    make
    ./webcachesim test.tr 0 LRU 1000


### Rewrite existing open-source traces

Example: download a public 1999 request trace ([trace description](http://www.cs.bu.edu/techreports/abstracts/1999-011)), rewrite it into our format, and run the simulator.

    wget http://www.cs.bu.edu/techreports/1999-011-usertrace-98.gz
    gunzip 1999-011-usertrace-98.gz
    g++ -o rewrite -std=c++11 ../traceparser/rewrite_trace_http.cc
    ./rewrite 1999-011-usertrace-98 test.tr
    make
    ./webcachesim test.tr 0 LRU 1073741824


## Implement a new policy

All cache implementations inherit from "Cache" (in policies/cache.h) which defines common features such as the cache capacity, statistics gathering, and the request interface. Defining a new policy needs little overhead

    class YourPolicy: public Cache {
    public:
      // interface to set arbitrary parameters request
      virtual void setPar(string parName, string parValue) {
        if(parName=="myPar") {
          myPar = stof(parValue);
        }
      }
    
       // requests call this function with their id and size
      bool request (const long cur_req, const long long size) {
       // your policy goes here
      }
    
    protected:
      double myPar;
    };
    // register your policy with the framework
    static Factory<YourPolicy> factoryYP("YourPolicy");
 
This allows the user interface side to conveniently configure and use your new policy.

    // create new cache
    unique_ptr<Cache> webcache = move(Cache::create_unique("YourPolicy"));
    // set cache capacity
    webcache->setSize(1000);
    // set an arbitrary param (parser implement by yourPolicy)
    webcache->setPar("myPar", "0.94");



## Contributors are welcome

Want to contribute? Great! We follow the [Github contribution work flow](https://help.github.com/articles/github-flow/).
This means that submissions should fork and use a Github pull requests to get merged into this code base.

There are a couple ways to help out.

### Documentation and use cases

Tell us how you use webcachesim or how you'd want to use webcachesim and what you're missing to implement your use case.
Feel free to [create an issue](https://github.com/dasebe/webcachesim/issues/new) for this purpose.

### Bug Reports

If you come across a bug in webcachesim, please file a bug report by [creating a new issue](https://github.com/dasebe/webcachesim/issues/new). This is an early-stage project, which depends on your input!

### Write test cases

This project has not be thoroughly tested, any test cases are likely to get a speedy merge.

### Contribute a new caching policy

If you want to add a new caching policy, please augment your code with a reference, a test case, and an example. Use pull requests as usual.

## References

We ask academic works, which built on this code, to reference the AdaptSize paper:

    AdaptSize: Orchestrating the Hot Object Memory Cache in a CDN
    Daniel S. Berger, Ramesh K. Sitaraman, Mor Harchol-Balter
    To appear in USENIX NSDI in March 2017.
    
You can find more information on [USENIX NSDI 2017 here.](https://www.usenix.org/conference/nsdi17/technical-sessions)
