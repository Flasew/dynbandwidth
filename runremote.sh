#!/bin/bash
tbf=/root/dynbandwidth/tbf
rep=1
data=10000
TAB='\t'
pid=
rwnd=20000000
#declare -a bwarr=(51 64 81 102 128 161 203 255 321 405 509 641 807 1016 1279 1611 2028  2553 3214 4046 5093 6412 8072 10162 12794 16106 20277 25527 32137 40458 50933 64121      80724 101625 127938 161065 202768 255270 321366 404576 509331 641210 807235 1016249      1279381 1610646)
declare -a flarr=(1 2 5 10 15)
declare -a qlarr=(100 200 400 800 1000 2000)
declare -a dlarr=(0 500 1000)
declare -a bwarr=(64 102 161 255 405 641 1016 1611 2553 4046 6412 10162 16106 25527 40458 64121 101625 161065 255270)
#declare -a flarr=(5)
#declare -a qlarr=(2000)
#declare -a dlarr=(500)
#declare -a bwarr=(161065)
for b in "${bwarr[@]}"; do
  freq=$((b-20))
  for q in "${qlarr[@]}"; do
    /root/dynbandwidth/tbf/tbf $b $q &
    pid=`echo $!`
    for d in "${dlarr[@]}"; do
      ssh root@b09-04 "tc qdisc del dev enp1s0 root"
      ssh root@b09-06 "tc qdisc del dev enp1s0 root"
      if [ $d -ne 0 ]; then
        ssh root@b09-04 "tc qdisc add dev enp1s0 root netem delay ${d}us limit 5000"
        ssh root@b09-06 "tc qdisc add dev enp1s0 root netem delay ${d}us limit 5000"
      fi
      for j in "${flarr[@]}"; do
        n=0
        while [ $n -lt $rep ]; do
          n=$((n+1))
          fname=log_$(eval "date +%m_%d_%Y_%H_%M_%S").json
          ssh root@b09-04 "iperf3 -c 10.0.1.6 -n ${data}M -w ${rwnd}B -P $j -i 0 -J" > $fname
          sed -i "2i\	\"delay\":\	$d," $fname
          sed -i "2i\	\"qlen\":\	$q," $fname
          sed -i "2i\	\"bw_freq\":\	$b," $fname
        done
      done
    done
    kill $pid
  done
done
