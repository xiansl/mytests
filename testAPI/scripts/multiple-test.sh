#!/bin/bash
#
# FPGA test script
# by zzd and Lei
# for performance testing
#
# prerequisite:
# 1. modify fpga_node.txt with node information
# 2. modify paramters in this scipt for your environment
#

Mode="TCP"                    #one of the following: CPU, Local, TCP, RDMA, Hybrid
JobPattern="Small"              #one of the following: Small, Median, Large, Mixed
Algorithm="Local"               #one of the following: Local, FIFO, Priority
                                #(Local is only used for the mode Local, 
                                #don't set it when you use a different mode; 
                                #Local means the scheduler is running locally, 
                                #and only receives jobs from the same node)
SchedulerHost="p01"         #change it to fit your own setting 
SchedulerPort="9000"
DAEMON_PORT="5000"
FPGANodes="tian01"              #no space between
OtherNodes="tian02"             #no space between 
Path="/home/tian/mytests/testAPI/"

usage() {
    echo ""
    echo "**************"
    echo "mutiple-test.sh"
    echo "for performance testing"
    echo "Prerequisite: "
    echo "1. Modify fpga_node.txt with node information"
    echo "2. Please edit this script and set proper variables before you run it."
    echo "   Variables needing to be moddified are at the beginning of the codes"
    echo ""
    echo "After that, you can run this script with one of the following paramters:"
    echo ""
	echo "Usage: test_fpga.sh [start|status|stop]"
    echo ""
    echo "**************"
    echo ""
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
	if [[ $Mode = "CPU" ]]; then
        #execute_job
        node=
        cmd="pdsh -w $FPGANodes \"cd $Path/scripts; ./execute_job.sh `hostname` 1 $Mode $JobPattern $SchedulerHost $SchedulerPort &\"" 
        exe "$cmd"
        cmd="pdsh -w $OtherNodes \"cd $Path/scripts; ./execute_job.sh `hostname` 0 $Mode $JobPattern $SchedulerHost $SchedulerPort &\"" 
        exe "$cmd"

    else 
	    # run scheduler on current node 
        echo " Scheduler is using ${Algorithm}, Mode: ${Mode}, JobPattern: ${JobPattern} "
	    datetime=`date +"%Y%m%d-%H%M"`
        cmd="${Path}/scheduler.py $SchedulerPort $Algorithm $Mode $Path/fpga_node.txt >>logfile/Scheduler-${Mode}-${JobPattern}-${Algorithm}-${datetime}.log &"
	    exe "$cmd"

	    if [[ $Mode = "Local" ]]; then
            cmd="pdsh -w $AllNodes \"cd $Path/scripts; ./execute_job.sh `hostname` 1 $Mode $JobPattern $SchedulerHost $SchedulerPort &\"" 
            exe "$cmd"

        elif [[ $Mode = "Hybrid" ]]; then  #TCP, RDMA
            cmd="pdsh -w $FPGANodes \"cd $Path; ./deamon.py $DaemonPort $SchedulerHost $SchedulerPort &\""
            exe "$cmd"

            cmd="pdsh -w $FPGANodes \"cd $Path/scripts; ./execute_job.sh `hostname` 1 $Mode $JobPattern $SchedulerHost $SchedulerPort &\"" 
            exe "$cmd"

            cmd="pdsh -w $OtherNodes \"cd $Path/scripts; ./execute_job.sh `hostname` 0 $Mode $JobPattern $SchedulerHost $SchedulerPort &\"" 
            exe "$cmd"
        fi
    fi
}

# check scheduler and server status
test_status() {
	cmd="ps aux | grep \"$Path\" | grep \"scheduler\""
    exe "$cmd"
	echo "Scheduler Mode: $Mode"

	if [[ $Mode = "CPU" ]]; then
		cmd="pdsh -w $AllNodes \"ps aux | egrep \"[e]xecute_job\" \""
		cmd="pdsh -w $AllNodes \"ps aux | grep sh| egrep \"execute_job\" \""
        exe "$cmd"
        cmd="pdsh -w $AllNodes \"ps aux | egrep \"[j]ob-testbench\" \""
        cmd="pdsh -w $AllNodes \"ps aux | egrep \"job-testbench\" \""
        exe "$cmd"

	elif [[ $Mode = "Local" ]]; then
		cmd="pdsh -w $SchedulerHost \"ps aux | egrep \"[s]cheduler.py\"\""
		cmd="pdsh -w $AllNodes \"ps aux | grep python| egrep  \"scheduler.py\"\""
        exe "$cmd"
		cmd="pdsh -w $AllNodes \"ps aux | egrep \"[e]xecute_job\"\""
		cmd="pdsh -w $AllNodes \"ps aux | grep sh| egrep \"execute_job\"\""
        exe "$cmd"

    elif [[$Mode = "Hybrid"]]; then  #TCP
		cmd="pdsh -w $SchedulerHost \"ps aux | egrep \"[s]cheduler.py\"\""
		cmd="pdsh -w $SchedulerHost \"ps aux | egrep \"scheduler.py\"\""
        exe "$cmd"

		cmd="pdsh -w $FPGANodes \"ps aux | egrep \"[d]eamon.py\"\""
		cmd="pdsh -w $FPGANodes \"ps aux | egrep \"deamon.py\"\""
        exe "$cmd"

		cmd="pdsh -w $AllNodes \"ps aux | egrep \"[e]xecute_job\"\""
		cmd="pdsh -w $AllNodes \"ps aux | egrep \"execute_job\"\""
        exe "$cmd"

        cmd="pdsh -w $AllNodes \"ps aux | egrep \"[j]ob-testbench\"\""
        cmd="pdsh -w $AllNodes \"ps aux | egrep \"job-testbench\"\""
        exe "$cmd"

		cmd="pdsh -w $AllNodes \"ps aux | egrep \"[f]pga-benchmark\"\""
		cmd="pdsh -w $AllNodes \"ps aux | egrep \"fpga-benchmark\"\""
        exe "$cmd"

	else  #TCP, RDMA
		cmd="pdsh -w $SchedulerHost \"ps aux | egrep \"[s]cheduler.py\"\""
		cmd="pdsh -w $SchedulerHost \"ps aux | egrep \"scheduler.py\"\""
        exe "$cmd"

		cmd="pdsh -w $FPGANodes \"ps aux | egrep \"[d]eamon.py\"\""
		cmd="pdsh -w $FPGANodes \"ps aux | egrep \"deamon.py\"\""
        exe "$cmd"

		cmd="pdsh -w $AllNodes \"ps aux | egrep \"[e]xecute_job\"\""
		cmd="pdsh -w $AllNodes \"ps aux | egrep \"execute_job\"\""
        exe "$cmd"
	fi
}

# kill scheduler on SCHEDULE_NODE, 
# kill server and tests based on all nodes 
test_stop() {
    if [[ $Mode = "CPU" ]]; then
        cmd="pdsh -w $allnodes \"pkill -9 -f execute_job\"\""
        exe "$cmd"
    else
	    cmd="pkill -9 -f scheduler"
        exe "$cmd"

	    cmd="pdsh -w $FPGANodes \"pkill -9 -f deamon\""
        exe "$cmd"
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

#read_conf

AllNodes="$FPGANodes,$OtherNodes"

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
