#!/bin/bash
# get byte, unique byte, unique count, average byte
 head -n 200000000 memc_200m.tr | awk '{size[$2]=$3; sum+=$3;frequency[$2]+=1}
 (!(NR%1000000)){print NR} ($3 > max){max=$3}
 END{for (i in size){unique_sum +=size[i]; count += 1; if (frequency[i]==1) {n_one_hit_wonder += 1}};
  print max, sum, unique_sum, count, unique_sum/count, n_one_hit_wonder}'
cat top_m_gbdt_128G_0.log |awk -F' ' '{print $5,$6,$9}'>top_m_gbdt_128G_0.log.awk