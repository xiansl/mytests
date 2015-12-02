#!/bin/bash
usage() {
    echo "usage: execute_job.sh [node scheduler_host scheduler_port mode pattern]"
    echo "mode: CPU, Local, TCP, RDMA, Hybrid"
    echo "pattern: Large Median Small Mixed"
}

processLine(){
	line="$@" # get all args
  	arg1=$(echo $line | awk '{ print $1 }')
  	arg2=$(echo $line | awk '{ print $2 }')
    arg3=$(echo $line | awk '{ print $3 }')

	sleep $arg3
	./job-testbench.sh $arg1 $arg2 $scheduler_host $scheduler_port $mode & 
}

if [[ "$#" -eq "4" ]]; then
    node=$1
    scheduler_host=$2
    scheduler_port=$3
    mode=$4         #CPU, Local, TCP, FPGA, Hybrid
    pattern=$5      #Large, Median, Small, Mixed
    
    write_log="logfile/${node}-${mode}-${pattern}.log"
    rm -rf $write_log 
    touch $write_log
    
    FILE="job-pattern/$node-$pattern.txt"
    
    if [ ! -f $FILE ]; then
    	echo "$FILE: does not exists"
    	exit 1
    elif [ ! -r $FILE ]; then
    	echo "$FILE: can not be read"
    	exit 2
    fi

    BAKIFS=$IFS
    IFS=$(echo -en "\n\b")
    exec 3<&0
    exec 0<"$FILE"
    SHELLPID=$$
    while read -r line
    do
    	processLine $line >>$write_log
    	#processline $line
    done
    exec 0<&3
    IFS=$BAKIFS
    kill $SHELLPID
else
    usage
fi

exit
