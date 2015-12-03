#!/bin/bash

usage(){
    echo "Usage: job-testbench.sh [acc_name job_size scheduler_host scheduler_port mode if_fpga_available]"
}

function exe {
	#echo $1
	eval $1
}


if [[ "$#" -eq "6" ]]; then
    acc_name=$1;
    job_size=$2;
    scheduler_host=$3
    scheduler_port=$4
    mode=$5
    if_fpga_available=$6
    bench_path="$PWD/job-implement"

    if [ $mode == "CPU" ]; then
        exe "$bench_path/${acc_name}-benchmark $job_size"

    elif [ $mode == "Local" ]; then
        if [ "$if_fpga_available" -eq "1" ]; then   
            echo $if_fpga_available
    	    exe "$bench_path/fpga-benchmark $acc_name $job_size $scheduler_host $scheduler_port"
        else 
    	    exe "$bench_path/${acc_name}-benchmark $job_size"
        fi

    elif [ $mode == "Hybrid" ]; then
    	if [ $job_size -le 10240 ]; then 	
            echo "$job_size"
    	    exe "$bench_path/${acc_name}-benchmark $job_size"
    	else
    		exe "$bench_path/fpga-benchmark $acc_name $job_size $scheduler_host $scheduler_port"
    	fi

    else  #TCP or RDMA
    	exe "$bench_path/fpga-benchmark $acc_name $job_size $scheduler_host $scheduler_port"

    fi

else
    usage
    echo "$#"
fi
exit

