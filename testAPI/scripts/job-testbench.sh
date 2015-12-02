#!/bin/bash

usage(){
    echo "Usage: job-testbench.sh [acc_name job_size scheduler_host scheduler_port mode]"
    echo "mode: CPU Local TCP RDMA Hybrid"
}

function exe {
	echo $1
#	eval $1
}


if [[ "$#" -eq "5" ]]; then
    acc_name=$1;
    job_size=$2;
    scheduler_host=$3
    scheduler_port=$4
    mode=$5
    bench_path="$PWD/job-implement"

    if [ $mode == "CPU" ]; then
    		 exe "$bench_path/$acc_name $job_size"

    elif [ $mode == "Local" ]; then
    		exe "$bench_path/fpga-benchmark $acc_name $job_size $scheduler_host $scheduler_port"
    elif [ $mode == "Hybrid" ]; then
    	if [ $job_size -le 4194304 ]; then 	
    		 exe "$bench_path/$acc_name $job_size"
    	else
    		exe "$bench_path/fpga-benchmark $acc_name $job_size $scheduler_host $scheduler_port"
    	fi
    else
        ls
    fi

else
    usage
fi
exit

