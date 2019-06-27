#!/bin/bash
tbf=/root/dynbandwidth/tbf
rep=4
data=10000
TAB='\t'
pid=
rwnd=524288
#declare -a bwarr=(51 64 81 102 128 161 203 255 321 405 509 641 807 1016 1279 1611 2028  2553 3214 4046 5093 6412 8072 10162 12794 16106 20277 25527 32137 40458 50933 64121      80724 101625 127938 161065 202768 255270 321366 404576 509331 641210 807235 1016249      1279381 1610646)
declare -a flarr=(1 2 5 10 15)
declare -a bwarr=(64 102 161 255 405 641 1016 1611 2553 4046 6412 10162 16106 25527 40458 64121 101625)
for i in "${bwarr[@]}"; do
  freq=$((i-15))
  /root/dynbandwidth/tbf/tbf $i &
  pid=`echo $!`
  for j in "${flarr[@]}"; do
    n=0
    while [ $n -lt $rep ]; do
      n=$((n+1))
      fname=log_$(eval "date +%m_%d_%Y_%H_%M_%S").json
      iperf3 -c 10.0.0.6 -n ${data}M -w ${rwnd}B -P $j -i 0 -J > $fname
      sed -i "2i\	\"bw_freq\":\	$i," $fname
      sleep 1
    done
  done
  kill $pid
done
