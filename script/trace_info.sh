#!/bin/bash
# get byte, unique byte, unique count, average byte
 head -n 200000000 memc_200m.tr | awk '{size[$2]=$3; sum+=$3}END{for (i in size){unique_sum +=size[i]; count += 1}; print sum " " unique_sum " " count " " unique_sum/count}'
cat top_m_gbdt_128G_0.log |awk -F' ' '{print $5,$6,$9}'>top_m_gbdt_128G_0.log.awk