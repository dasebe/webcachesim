#include <chrono>
#include <iostream>
#include <vector>

std::chrono::steady_clock::time_point bench_ts_start;
std::chrono::steady_clock::time_point bench_ts_end;
uint64_t bench_req_count = 0;
uint64_t bench_req_limit;
long double bench_dur_sum = 0;
uint64_t bench_req_sum = 0;

inline void bench_start(uint64_t req_limit) {
    bench_ts_start = std::chrono::steady_clock::now();
    bench_req_limit = req_limit;
}

inline void bench_iterate() {
    if(++bench_req_count >= bench_req_limit) {
        bench_ts_end = std::chrono::steady_clock::now();
        double dur_milli = std::chrono::duration_cast<std::chrono::milliseconds>(bench_ts_end - bench_ts_start).count();
        bench_dur_sum += dur_milli;
        bench_req_sum += bench_req_count;
        if(dur_milli>0) {
            std::cerr << "brate " <<
                uint64_t(bench_req_count * 1000.0 / dur_milli)
                      << " " << uint64_t(bench_req_sum * 1000.0 / bench_dur_sum)
                      << "\n";
        } else {
            std::cerr << "brate NODURATION\n";
        }
        bench_req_count=0;
        bench_ts_start = bench_ts_end;
    }
}
