#!/bin/bash
#
# FPGA test script
# by zzd and Lei
# for performance testing
#
# prerequisite:
# 1. modify fpga_node.txt with node information
# 2. modify fpga.cfg for your environment
#
# usage: ./multiple-test.sh [start|stop|status]
#

# read node list from fpga_node.txt

# show usage info


usage() {
	echo "Usage: test_fpga.sh [start|status|stop]"
}

read_conf() {
	echo "Reading config ..." >&2
	. ./fpga.cfg
	if [ -z "$MODE" ]; then
		Mode="CPU"
	else 
		Mode=$MODE
	fi
	if [ -z "$JOBPATTERN" ]; then
        JobPattern="Small"
	else 
        JobPattern=$JOBPATTERN
	fi
	if [ -z "$ALGORITHM" ]; then
        Algorithm="Local"
	else 
        Algorithm=$ALGORITHM
	fi
	if [ -z "$SCHEDULER_HOST" ]; then
        SchedulerHost="n1"
	else 
        SchedulerHost=$SCHEDULER_HOST
	fi
	if [ -z "$SCHEDULER_PORT" ]; then
        SchedulerPort="9000"
	else 
        SchedulerPort=$SCHEDULER_PORT
	fi
	if [ -z "$DAEMON_PORT" ]; then
        DaemonPort="5000"
	else 
        DaemonPort=$DAEMON_PORT
	fi
	if [ -z "$FPGA_NODES" ]; then
        FPGANodes="node1,node2"
	else 
        FPGANodes=$FPGA_NODES
	fi
	if [ -z "$OTHER_NODES" ]; then
        OtherNodes="node1,node2"
	else 
        OtherNodes=$OTHER_NODES
	fi
    if [ -z "$PATH" ]; then
        Path="$PWD"
    else
        Path=$PATH
    fi
    
    echo "Config:   $Mode, $JobPattern, $Algorithm, $SchedulerHost, $SchedulerPort, $DaemonPort, $FPGANodes, $OtherNodes $Path "
}

# start scheduler on SchedulerHost, 
# start server and tests based on MODE

test_start() {
	# run scheduler on current node 
	run_scheduler $pattern $algorithm $mode $mean 
}

# check scheduler and server status
test_status() {
	ps aux | grep "/home/tian/testAPI/" | grep "scheduler"
	echo "Scheduler Mode: $Mode"

	if [[ $Mode = "CPU" ]]; then
		pdsh -w $AllNodes 'ps aux | egrep "[e]xecute_job"'
		pdsh -w $AllNodes 'ps aux | egrep "[A]ES-benchmark"'
		pdsh -w $AllNodes 'ps aux | egrep "[F]FT-benchmark"'

	elif [[ $Mode = "Local" ]]; then
		pdsh -w $FPGANodes 'ps aux | egrep "[s]cheduler.py"'

		pdsh -w $AllNodes 'ps aux | egrep "[e]xecute_job"'
		pdsh -w $FPGANodes 'ps aux | egrep "[f]pga-benchmark"'
		pdsh -w $OtherNodes 'ps aux | egrep "[A]ES-benchmark"'
		pdsh -w $OtherNodes 'ps aux | egrep "[F]FT-benchmark"'

    elif [[$Mode = "Hybrid"]]; then  #TCP, RDMA
		pdsh -w $SchedulerHost 'ps aux | egrep "[s]cheduler.py"'
		pdsh -w $FPGANodes 'ps aux | egrep "[d]eamon.py"'

		pdsh -w $AllNodes 'ps aux | egrep "[e]xecute_job"'
		pdsh -w $AllNodes 'ps aux | egrep "[f]pga-benchmark"'
		pdsh -w $AllNodes 'ps aux | egrep "[A]ES-benchmark"'
		pdsh -w $AllNodes 'ps aux | egrep "[F]FT-benchmark"'

	else  #TCP, RDMA
		pdsh -w $SchedulerHost 'ps aux | egrep "[s]cheduler.py"'
		pdsh -w $FPGANodes 'ps aux | egrep "[d]eamon.py"'

		pdsh -w $AllNodes 'ps aux | egrep "[e]xecute_job"'
		pdsh -w $AllNodes 'ps aux | egrep "[f]pga-benchmark"'
	fi
}

# kill scheduler on SCHEDULE_NODE, 
# kill server and tests based on all nodes 
test_stop() {
	pkill -9 -f scheduler
	pdsh -w $fpga_nodes 'pkill -9 -f deamon'
	if [[ $pattern = "Local" ]]; then
		pdsh -w $fpganodes 'pkill -9 -f execute_job'

	elif [[ $pattern = "Remote" ]]; then
		pdsh -w $other_nodes 'pkill -9 -f execute_job'
		
	else
		pdsh -w $allnodes 'pkill -9 -f execute_job'

	fi
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
    mode=$3
	mean=$4
	
	echo "  scheduler is using ${algorithm}, mode = ${mode}, pattern = ${pattern}"
	datetime=`date +"%Y%m%d-%H%M"`
	cmd="/home/tian/mytests/testAPI/scheduler.py 9000 ${algorithm} ${mode} fpga_node.txt >> $pattern-f$mean-${algorithm}-$datetime.log &"
	exe "$cmd"
	echo "  pattern=$pattern"
	
	if [[ $pattern = "Local" ]]; then
		cmd="pdsh -w $fpga_nodes \"cd $path; ./fpga_node.sh &\""
        exe "$cmd"

	elif [[ $pattern = "Remote" ]]; then
		cmd="pdsh -w $fpga_nodes \"cd /home/tian/mytests/testAPI; ./deamon.py 5000 tian01 9000 &\""
        exe "$cmd"
		cmd="pdsh -w $other_nodes \"cd $path; ./other_node.sh &\""
        exe "$cmd"
		
	else
		cmd="pdsh -w $fpga_nodes \"cd $path; ./fpga_node.sh &\""
        exe "$cmd"
		cmd="pdsh -w $other_nodes \"cd $path; ./other_node.sh &\""
        exe "$cmd"
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
