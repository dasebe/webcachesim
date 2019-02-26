package com.github.benmanes.caffeine.examples.tinylfu;

import java.io.BufferedReader;
import java.io.FileReader;
import java.io.IOException;

import com.github.benmanes.caffeine.cache.Caffeine;
import com.github.benmanes.caffeine.cache.LoadingCache;

import java.util.HashMap;

public class tinylfu {
    public static void main(String[] args) {
        // trace cache_type size k0 v0 k1 v1 ...
        // byte request = byte hit + byte miss
        // byte miss = byte eviction + byte in cache
        // interest_byte_hit = (whole_byte_request - warmup_byte_request) - (whole_byte_evict - warmup_byte_evict) \
        // - (whole_byte_cache - warmup_byte_cache)
        // whole_byte_cache ~= warmup_byte_cache
        String trace_file = args[0];
        String trace_path = System.getenv("WEBCACHESIM_TRACE_DIR");

        long cache_size = Long.parseLong(args[2]);
        long n_warmup = 0;

        for (int i = 3; i < args.length; i += 2) {
            if (args[i].equals("n_warmup")) {
                n_warmup = Long.parseLong(args[i+1]);
            }
        }


        HashMap<Long, Integer> size_map = new HashMap<>();

//        MetricRegistry registry = new MetricRegistry();
        LoadingCache<Long, Long> cache = Caffeine
                .newBuilder()
//                .initialCapacity(cache_size)
//                .maximumSize(100)
                .maximumWeight(cache_size)
                .weigher((k, v) -> size_map.get(k))
                .recordStats()
                .executor(Runnable::run)
                .build(k -> k);

        BufferedReader reader;
        long counter = 0;
        long warmup_n_hit = 0;
        long warmup_n_request = 0;
        long whole_byte_request = 0;
        long warmup_byte_request = 0;
        long warmup_byte_evict = 0;
        try {
            reader = new BufferedReader(new FileReader(trace_path+"/"+trace_file));
            String line = reader.readLine();
            while (line != null) {
//                System.out.println(line);
                if (counter == n_warmup) {
                    warmup_n_hit = cache.stats().hitCount();
                    warmup_n_request = cache.stats().requestCount();
                    warmup_byte_evict = cache.stats().evictionWeight();
                }
                String[] values = line.split(" ");
                long key = Long.parseLong(values[1]);
                Integer size = Integer.valueOf(values[2]);

                if (counter < n_warmup)
                    warmup_byte_request += size;
                whole_byte_request += size;

                if (!size_map.containsKey(key)) {
                    size_map.put(key, size);
                }
                cache.get(key);

                // read next line
                line = reader.readLine();
                counter += 1;
                if ((counter % 1000000) == 0) {
                    System.err.println(counter);
                }
            }
            reader.close();
        } catch (IOException e) {
            e.printStackTrace();
        }


        long whole_n_hit = cache.stats().hitCount();
        long whole_n_request = cache.stats().requestCount();
        long whole_byte_evict = cache.stats().evictionWeight();
        double object_hit_rate = ((double) (whole_n_hit-warmup_n_hit))/(whole_n_request-warmup_n_request);
        double byte_hit_rate = ((double) (whole_byte_request - warmup_byte_request -
                (whole_byte_evict - warmup_byte_evict)) )/(whole_byte_request - warmup_byte_request);
        System.out.println(object_hit_rate);
        System.out.println(byte_hit_rate);
//        System.out.println(whole_miss - warmup_miss);
    }

}
