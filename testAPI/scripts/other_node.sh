#!/bin/bash
# run on fpga node
#

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

# print and execute command string
# comment out eval while debugging
exe() {
	if [[ "$#" -eq "1" ]]; then
		echo "  CMD: $1"
		eval $1
	else
		echo "  error in exe call"
		exit -1
	fi
}

read_conf

cmd="./execute_job.sh `hostname` 0 $Mode $JobPattern $SchedulerHost $SchedulerPort &" 
exe "$cmd"

