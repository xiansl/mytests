#! /bin/bash
# run on fpga node
#
. ./fpga.cfg

job_num=$JOB_NUM
mean=$MEAN
node=`hostname`

export LD_LIBRARY_PATH=/home/900/lxs900/zzd/fpga_platform:/home/900/lxs900/zzd/fpga_platform/fpga-sim/driver:$LD_LIBRARY_PATH

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
cmd="../xmlrpclib_server.py -a $node -p 8000 &" 
exe "$cmd"
cmd="./execute_job.sh $node &" 
exe "$cmd"
