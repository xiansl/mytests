#!/bin/bash

usage() {
    echo "usage: execute_job.sh [local_node  if_fpga_available  schedule_mode  job_pattern scheduler_host scheduler_port]"
    echo "Example: execute_job.sh n1 1 TCP Small tian01 9000"
    echo "Example: execute_job.sh n0 0 RDMA Mixed tian01 9000"
}

processLine(){
	line="$@" # get all args
  	arg1=$(echo $line | awk '{ print $1 }')
  	arg2=$(echo $line | awk '{ print $2 }')
    arg3=$(echo $line | awk '{ print $3 }')

	sleep $arg3

    ./job-testbench.sh $arg1 $arg2 $SchedulerHost $SchedulerPort $Mode $ifFPGA &
}

if [[ "$#" -eq "6" ]]; then
    node=$1
    ifFPGA=$2
    Mode=$3
    JobPattern=$4
    SchedulerHost=$5
    SchedulerPort=$6

    write_log="logfile/${node}-${Mode}-${JobPattern}.log"
    rm -rf $write_log 
    touch $write_log

    FILE="job-pattern/${node}-${JobPattern}.txt"
    if [ ! -f $FILE ]; then
    	echo "$FILE: does not exists"
    	exit 1
    elif [ ! -r $FILE ]; then
    	echo "$FILE: can not be read"
    	exit 2
    fi

    exec 3<&0
    exec 0<"$FILE"
    #SHELLPID=$$
    trap "exit" INT
    while read -r line
    do
        #processLine $line -e >>$write_log
        processLine $line -e 
    done
    exec 0<&3
    #kill $SHELLPID
    exit 0
else
    usage
fi

exit
