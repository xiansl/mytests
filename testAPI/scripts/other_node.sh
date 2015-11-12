#! /bin/bash
. ./fpga.cfg
job_num=$JOB_NUM
mean=$MEAN
node=`hostname`
./set_real_job.py $job_num $mean $node 
./execute_job.sh $node & 
