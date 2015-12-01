#!/bin/bash

bench_path="$PWD/job-implement"
mode=$1  # software, fpga, mix
job_size=$2; #
acc_name=$3; # aes, fft
scheduler_host=$4 #
scheduler_port=$5 #

function exe {
	echo $1
#	eval $1
}

if [ $mode == "software" ]; then
		 exe "$bench_path/$acc_name $job_size"
elif [ $mode == "fpga" ]; then
		exe "$bench_path/fpga-benchmark $acc_name $job_size $scheduler_host $scheduler_port"
elif [ $mode == "mixed" ]; then
	if [ $job_size -le 4194304 ]; then 	
		 exe "$bench_path/$acc_name $job_size"
	else
		exe "$bench_path/fpga-benchmark $acc_name $job_size $scheduler_host $scheduler_port"
	fi
fi

