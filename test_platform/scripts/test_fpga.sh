#!/bin/bash
#
# FPGA test script
# by zzd 
# for performance testing
#
# prerequisite: 
# 1. modify fpga_node.txt with node information
# 2. modify fpga.cfg for your environment
#
# usage: ./test_fpga.sh [start|stop]
#

# read node list from fpga_node.txt

# show usage info

path=$PWD

usage() {
	echo "Usage: test_fpga.sh [start|status|stop]"
}

read_conf() {
	echo "Reading config ..." >&2
	. ./fpga.cfg
	if [ -z "$PATTERN" ]; then
		pattern="Local"
	else 
		pattern=$PATTERN
	fi
	if [ -z "$ALGORITHM" ]; then
		algorithm="LOCALITY2"
	else 
		algorithm=$ALGORITHM
	fi
	if [ -z "$MEAN" ]; then
		mean="500"
	else 
		mean=$MEAN
	fi
	if [ -z "$JOB_NUM" ]; then
		job_num="10"
	else 
		job_num=$JOB_NUM
	fi
	if [ -z "$FPGA_NODES" ]; then
		fpga_nodes="r2,r3"
	else 
		fpga_nodes=$FPGA_NODES
	fi
	if [ -z "$OTHER_NODES" ]; then
		other_nodes="r4,r5"
	else 
		other_nodes=$OTHER_NODES
	fi
	echo "  Config: $pattern, $algorithm, $mean, $job_num, $fpga_nodes, $other_nodes"
}

# start scheduler on SCHEDULE_NODE, 
# start server and tests based on PATTERN
test_start() {
	# run scheduler on current node 
	run_scheduler $pattern $algorithm $mean $HOSTNAME
}

# check scheduler and server status
test_status() {
	ps aux | grep lxs900 | grep "[f]inal_scheduler"
	pdsh -w $allnodes 'ps aux | egrep "[x]mlrpclib_server|[e]xecute_job"'
}

# kill scheduler on SCHEDULE_NODE, 
# kill server and tests based on all nodes 
test_stop() {
	pkill -9 -f final_scheduler
	pdsh -w $allnodes 'pkill -9 -f execute_job'
	pdsh -w $allnodes 'pkill -9 -f xmlrpclib_server'
}

# print and execute command string
# comment out eval while debugging
exe() {
	if [[ "$#" -eq "1" ]]; then
		echo "    CMD: $1"
		eval $1
	else
		echo "  error in exe call"
		exit -1
	fi
}

run_scheduler() {
	pattern=$1
	algorithm=$2
	mean=$3
	node=$4
	
	echo "  scheduler is using ${algorithm}, mean = ${mean}, pattern = ${pattern}"
	datetime=`date +"%Y%m%d-%H%M"`
	cmd="./final_scheduler.py $node 9000 ${algorithm} >> $pattern-f$mean-${algorithm}-$datetime.log &"
	exe "$cmd"
	echo "  pattern=$pattern"
	
	if [[ $pattern = "Local" ]]; then
		cmd='pdsh -w $fpga_nodes "cd $path; ./fpga_node.sh &"'
		echo "$cmd"; eval "$cmd"
		
	elif [[ $pattern = "Remote" ]]; then
		cmd='pdsh -w $fpga_nodes "cd $path; ../xmlrpclib_server.py -a $node -p 8000 &"'
		echo "$cmd"; eval "$cmd"
		cmd='pdsh -w $other_nodes "cd $path; ./other_node.sh &"'
		echo "$cmd"; eval "$cmd"
	else
		cmd='pdsh -w $fpga_nodes "cd $path; ./fpga_node.sh &"'
		echo "$cmd"; eval "$cmd"
		cmd='pdsh -w $other_nodes "cd $path; ./other_node.sh &"'
		echo "$cmd"; eval "$cmd"
	fi
}

read_conf
allnodes="$fpga_nodes,$other_nodes"

if [[ "$#" -eq "1" ]]; then
	if  [[ "$1" = "start" ]]; then
		echo "Starting test_fpga:"
		test_start
		echo "  test_fpga started"
	elif [[ "$1" = "status" ]]; then
		echo "test_fpga status:"
		test_status
	elif [[ "$1" = "stop" ]]; then
		echo "Stopping test_fpga:"
		test_stop
		echo "  test_fpga stopped"
	else 
		usage
	fi
else
	usage
fi

exit
