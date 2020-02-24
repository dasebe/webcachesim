# webcachesim2:

## A simulator for CDN caching and web caching policies.

Simulate a variety of existing caching policies by replaying request traces, and use this framework as a basis to experiment with new ones.

The webcachesim2 simulator was built for Learning relaxed Belady (LRB), a learning-based caching algorithm. And it was built on top of [webcachesim](https://github.com/dasebe/webcachesim), see [References](#references) for more information.

Current support algorithms:
* Learning Relaxed Belady (LRB)
* LR (linear-regression based ML caching)
* Belady (heap-based)
* Belady (a sample-based approximate version)
* Relaxed Belady
* Inf (infinite-size cache)
* LRU
* B-LRU (Bloom Filter LRU)
* ThLRU (LRU with threshold admission control)
* LRUK
* LFUDA
* S4LRU
* ThS4LRU (S4LRU with threshold admission control)
* FIFO
* Hyperbolic
* GDSF
* GDWheel
* Adaptive-TinyLFU (java library integration)
* LeCaR
* UCB
* LHD
* AdaptSize
* Random (random eviction)


## Installation

We build a [docker image]() for you. Alternatively, you may follow the [instruction](INSTALL.md) to install the simulator.

## Using an exisiting policy

The basic interface is

    ./webcachesim_cli_db traceFile cacheType cacheSize [param_1 value_1] [param_2 value_2] ... [param_n value_n]

where

 - traceFile: a request trace (see below)
 - cacheType: one of the caching policies (see below)
 - cacheSize: the cache capacity in bytes
 - param_i, value_i: optional cache parameter and value, can be used to tune cache policies

### Examples

#### Running single simulation

##### Running LRU on Wiki trace
```bash
webcachesim_cli_db wc2800m_ts.tr LRU 1099511627776 segment_window 1000000 real_time_segment_window 600 dburl ${YOUR mongodb uri} dbcollection ${YOUR prefer mongodb collection} 
# Results will be sent to database
```

##### Running LRB on Wiki trace
```bash
webcachesim_cli_db wc2800m_ts.tr LRB 1099511627776 segment_window 1000000 real_time_segment_window 600 memory_window 536870912 dburl ${YOUR mongodb uri} dbcollection ${YOUR prefer mongodb collection} 
# Results will be sent to database
```

##### Running multiple configurations

Before run this, fill in your database information in the config file

```bash
python3 ${WEBCACHESIM_ROOT}/pywebcachesim/runner/runner.py --config_file ${WEBCACHESIM_ROOT}/config_example/job_dev.yaml  --algorithm_param_file ${WEBCACHESIM_ROOT}/config_example/algorithm_params.yaml  --trace_param_file ${WEBCACHESIM_ROOT}/config_example/trace_params.yaml --nodefile ${WEBCACHESIM_ROOT}/config_example/nodefile
```


### Request trace format

Request traces must be given in a space-separated format with 3+extra colums
- time should be a long long int, but can be arbitrary (for future TTL feature, not currently in use)
- id should be a long long int, used to uniquely identify objects
- size should be a long long int, this is object's size in bytes
- extra features are optional features. They can be viewed as categorical feature, which is an unsigned integer < 64K

| time |  id | size | \[extra_feature_1\] | \[extra_feature_2\] | ---- | \[extra_feature_n\] |
| ---- | --- | ---- |  ----               | ----                | ---- | ----                |
|   1  |  1  |  120 |
|   2  |  2  |   64 |
|   3  |  1  |  120 |
|   4  |  3  |  14  |
|   4  |  1 |  120 |

Example trace in file "test.tr".

<!--### Available caching policies-->

<!--There are currently ten caching policies. This section describes each one, in turn, its parameters, and how to run it on the "test.tr" example trace with cache size 1000 Bytes.-->

<!--#### LRU-->

<!--does: least-recently used eviction-->

<!--params: none-->

<!--example usage:-->

<!--    ./webcachesim test.tr LRU 1000-->
<!--     -->
<!--#### FIFO-->

<!--does: first-in first-out eviction-->

<!--params: none-->

<!--example usage:-->

<!--    ./webcachesim test.tr FIFO 1000-->
<!--    -->
<!--#### GDS-->

<!--does: greedy dual size eviction-->

<!--params: none-->

<!--example usage:-->

<!--    ./webcachesim test.tr GDS 1000-->
<!--    -->
<!--#### GDSF-->

<!--does: greedy dual-size frequency eviction-->

<!--params: none-->

<!--example usage:-->

<!--    ./webcachesim test.tr GDSF 1000-->
<!--    -->
<!--#### LFU-DA-->

<!--does: least-frequently used eviction with dynamic aging-->

<!--params: none-->

<!--example usage:-->

<!--    ./webcachesim test.tr LFUDA 1000-->
<!--    -->
<!--    -->
<!--#### Filter-LRU-->

<!--does: LRU eviction + admit only after N requests-->

<!--params: n - admit after n requests)-->

<!--example usage (admit after 10 requests):-->

<!--    ./webcachesim test.tr Filter 1000 n=10-->
<!--    -->
<!--#### Threshold-LRU-->

<!--does: LRU eviction + admit only after N requests-->

<!--params: t - the size threshold in log form (base 2)-->

<!--example usage (admit only objects smaller than 512KB):-->

<!--    ./webcachesim test.tr ThLRU 1000 t=19-->
<!--    -->
<!--#### ExpProb-LRU-->

<!--does: LRU eviction + admit with probability exponentially decreasing with object size-->

<!--params: c - the size which has a 50% chance of being admitted (used to determine the exponential family)-->

<!--example usage (admit objects with size 256KB with about 50% probability):-->

<!--    ./webcachesim test.tr ExpLRU 1000 c=18-->
<!--  -->
<!--#### LRU-K-->

<!--does: evict object which has oldest K-th reference in the past-->

<!--params: k - eviction based on k-th reference in the past-->

<!--example usage (each segment gets half the capacity)-->

<!--    ./webcachesim test.tr LRUK 1000 k=4-->


<!--## How to get traces:-->


<!--### Generate your own traces with a given distribution-->

<!--One example is a Pareto (Zipf-like) popularity distribution and Bounded-Pareto object size distribution.-->
<!--The "basic_trace" tool takes the following parameters:-->

<!-- - how many unique objects-->
<!-- - how many requests to generate for most popular object (total request length will be a multiple of that)-->
<!-- - Pareto shape-->
<!-- - min object size-->
<!-- - max object size-->
<!-- - output name for trace-->

<!--Here's an example that recreates the "test.tr" trace for the examples above. This uses the "basic_trace" generator with 1000 objects, about 10000 requests overall, Pareto shape 1.8 and object sizes between 1 and 10000 bytes.-->

<!--    g++ tracegenerator/basic_trace.cc -std=c++11 -o basic_trace-->
<!--    ./basic_trace 1000 1000 1.8 1 10000 test.tr-->
<!--    make-->
<!--    ./webcachesim test.tr 0 LRU 1000-->


<!--### Rewrite existing open-source traces-->

<!--Example: download a public 1999 request trace ([trace description](http://www.cs.bu.edu/techreports/abstracts/1999-011)), rewrite it into our format, and run the simulator.-->

<!--    wget http://www.cs.bu.edu/techreports/1999-011-usertrace-98.gz-->
<!--    gunzip 1999-011-usertrace-98.gz-->
<!--    g++ -o rewrite -std=c++11 ../traceparser/rewrite_trace_http.cc-->
<!--    ./rewrite 1999-011-usertrace-98 test.tr-->
<!--    make-->
<!--    ./webcachesim test.tr 0 LRU 1073741824-->


<!--## Implement a new policy-->

<!--All cache implementations inherit from "Cache" (in policies/cache.h) which defines common features such as the cache capacity, statistics gathering, and the request interface. Defining a new policy needs little overhead-->

<!--    class YourPolicy: public Cache {-->
<!--    public:-->
<!--      // interface to set arbitrary parameters request-->
<!--      virtual void setPar(string parName, string parValue) {-->
<!--        if(parName=="myPar") {-->
<!--          myPar = stof(parValue);-->
<!--        }-->
<!--      }-->
<!--    -->
<!--       // requests call this function with their id and size-->
<!--      bool request (const long cur_req, const long long size) {-->
<!--       // your policy goes here-->
<!--      }-->
<!--    -->
<!--    protected:-->
<!--      double myPar;-->
<!--    };-->
<!--    // register your policy with the framework-->
<!--    static Factory<YourPolicy> factoryYP("YourPolicy");-->
<!-- -->
<!--This allows the user interface side to conveniently configure and use your new policy.-->

<!--    // create new cache-->
<!--    unique_ptr<Cache> webcache = move(Cache::create_unique("YourPolicy"));-->
<!--    // set cache capacity-->
<!--    webcache->setSize(1000);-->
<!--    // set an arbitrary param (parser implement by yourPolicy)-->
<!--    webcache->setPar("myPar", "0.94");-->



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
