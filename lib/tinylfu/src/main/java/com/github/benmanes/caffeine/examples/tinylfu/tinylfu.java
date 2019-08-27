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
        // cannot resize dynamically
        String trace_file = args[0];

        long cache_size = Long.parseUnsignedLong(args[2]);
        int segment_window = 10000000;
        for (int i = 3; i < args.length; i += 2) {
            if (args[i].equals("segment_window")) {
                segment_window = Integer.valueOf(args[i + 1]);
            }
        }

        HashMap<Long, Integer> size_map = new HashMap<>();

        LoadingCache<Long, Long> cache = Caffeine
                .newBuilder()
                .maximumWeight(cache_size)
                .weigher((k, v) -> size_map.get(k))
                .recordStats()
                .executor(Runnable::run)
                .build(k -> k);

        BufferedReader reader;

        long id;
        int size;
        long byte_req = 0, byte_miss = 0, obj_req = 0, obj_miss = 0;
        long seq = 0;
        try {
            reader = new BufferedReader(new FileReader(trace_file+".ant"));
            String line = reader.readLine();
            while (line != null) {
                String[] values = line.split(" ");
                id = Long.parseUnsignedLong(values[2]);
                size = Integer.valueOf(values[3]);

                if (!size_map.containsKey(id))
                    size_map.put(id, size);

                byte_req += size;
                obj_req += 1;

                Long v = cache.getIfPresent(id);
                if (v == null) {
                    byte_miss += size;
                    obj_miss += 1;
                    cache.put(id, id);
                }

                seq += 1;

                if (seq%segment_window == 0) {
                    System.err.println("segment id: " + seq/segment_window);
                    System.err.println("bmr: " + (double)(byte_miss)/byte_req + "\n");
                    //spliter
                    if (seq != segment_window)
                        System.out.print(" ");
                    System.out.print(byte_req+" "+byte_miss+" "+obj_req+" "+obj_miss);
                    byte_req=byte_miss=obj_req=obj_miss=0;
                }

                // read next line
                line = reader.readLine();
            }
            reader.close();
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

}
