# webcachesim2

## A simulator for CDN caching and web caching policies.

Simulate a variety of existing caching policies by replaying request traces, and use this framework as a basis to experiment with new ones. A 14-day long [Wikipedia trace](#trace) is released alongside the simulator.

The webcachesim2 simulator was built to evaluate the Learning relaxed Belady algorithm (LRB), a new machine-learning-based caching algorithm. The simulator build on top of [webcachesim](https://github.com/dasebe/webcachesim), see [References](#references) for more information.



Currently supported caching algorithms:
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
* Adaptive-TinyLFU (via Java library integration)
* LeCaR
* UCB
* LHD
* AdaptSize
* LFO
* Random (random eviction)

Almost configuration parameters of these algorithms can be tuned in a single config file [config/algorithm_params.yaml](config/algorithm_params.yaml)

## Trace
The Wikipedia trace [download link](http://lrb.cs.princeton.edu/wiki2018.tr.tar.gz). To uncomress:
```shell script
tar -xzvf wiki2018.tr.tar.gz
```

## Trace Format
Request traces are expected to be in a space-separated format with 3 columns and additionally columns for extra features.
- time should be a long long int, but can be arbitrary (for future TTL feature, not currently in use)
- id should be a long long int, used to uniquely identify objects
- size should be uint32, this is object's size in bytes
- extra features are optional uint16 features. LRB currently interprets them as categorical features (e.g., object type).

| time |  id | size | \[extra_feature(s)\] |
| ---- | --- | ---- |  ----               |
|   1  |  1  |  120 |
|   2  |  2  |   64 |
|   3  |  1  |  120 |
|   4  |  3  |  14  |
|   4  |  1 |  120 |

Simulator will run a sanity check on the trace when starting up.

## Installation

For ease of use, we also provide a docker image which contains the simulator. Our documentation assumes that you use this image. To run it:
```shell script
 docker run -it -v ${YOUR TRACE DIRECTORY}:/trace sunnyszy/webcachesim:v0.1 ${traceFile} ${cacheType} ${cacheSize} [--param=value]
```
Alternatively, you may follow the [instruction](INSTALL.md) to manually install the simulator.

## Using an exisiting policy

The basic interface is

    ./webcachesim_cli traceFile cacheType cacheSize [--param=value]

where

 - traceFile: a request trace (see [trace format](#trace-format))
 - cacheType: one of the caching policies
 - cacheSize: the cache capacity in bytes
 - param, value: optional cache parameter and value, can be used to tune cache policies

### Examples

#### Running single simulation

##### Running LRU on Wiki trace 1TB
```bash
docker run -it -v ${YOUR TRACE DIRECTORY}:/trace sunnyszy/webcachesim:v0.1 wiki2018.tr LRU 1099511627776

# running sanity check on trace: /trace/wiki2018.tr
# ...
# pass sanity check
# simulating
# segment id: 0
# ...
# results will be print in json string. Byte miss and byte req are aggregated in segment_byte_req, segment_byte_miss.
# The default segment size is 1 million request. This allows to calculate final byte miss ratio with your warmup length.
# LRB evaluation warmup length is in the NSDI paper.
# Alternatively, check no_warmup_byte_miss_ratio for byte miss ratio without considering warmup.
```

##### Running B-LRU on Wiki trace 1TB
```bash
docker run -it -v ${YOUR TRACE DIRECTORY}:/trace sunnyszy/webcachesim:v0.1 wiki2018.tr LRU 1099511627776 --bloom_filter=1
```

##### Running LRB on Wiki trace 1TB
```bash
docker run -it -v ${YOUR TRACE DIRECTORY}:/trace sunnyszy/webcachesim:v0.1 wiki2018.tr LRB 1099511627776 --memory_window=671088640
```
LRB memory window for Wikipedia trace different cache sizes in the paper (based on first 20% validation prefix):

| cache size (GB) |  memory window |
| ---- | --- | 
|   64  |  58720256  | 
|   128  |  100663296  |
|   256  |  167772160  |
|   512  |  335544320  |
|   1024  |  671088640 |

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

We ask academic works, which built on this code, to reference the LRB/AdaptSize papers:

    Learning Relaxed Belady for Content Distribution Network Caching
    Zhenyu Song, Daniel S. Berger, Kai Li, Wyatt Lloyd
    USENIX NSDI 2020.
    
    AdaptSize: Orchestrating the Hot Object Memory Cache in a CDN
    Daniel S. Berger, Ramesh K. Sitaraman, Mor Harchol-Balter
    USENIX NSDI 2017.

## License
TODO: third-part licenses
    
