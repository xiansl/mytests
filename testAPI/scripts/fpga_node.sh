#! /bin/bash
# run on fpga node
#
. ./fpga.cfg

job_num=$JOB_NUM
mean=$MEAN
node=`hostname`

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

cmd="./set_real_job.py $job_num $mean $node"
exe "$cmd"
cmd="/home/tian/testAPI/deamon.py 5000 &" 
exe "$cmd"
cmd="./execute_job.sh $node &" 
exe "$cmd"
