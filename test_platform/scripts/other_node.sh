#! /bin/bash
. ./fpga.cfg
job_num=$JOB_NUM
mean=$MEAN
./set_real_job.py $job_num $mean 
./execute_job.sh & 
