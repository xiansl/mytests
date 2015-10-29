#!/bin/bash

dir=$PWD
interval=1 # every 10 secs send a request
range=10


for i in {1..10}
do
	number=$RANDOM
	interval[$i]=$((number %= $range))
	bench[$i]=$((number %= 5))
	workload[$i]=$((number %= ))
done

#for i in {1..10}
#do 
#	echo ${interval[$i]}
#done

for i in {1..10}
do
        for par1 in AES EC FFT DTW SHA
        do
                for((par2=128; par2<=1024; par2=par2*2))
                do
                        $PWD/test_bench $par1 $par2
                        sleep ${interval[$i]}
                done
        done
done

