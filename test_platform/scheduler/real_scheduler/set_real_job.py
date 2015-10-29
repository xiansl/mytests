#!/usr/bin/env python
# -*- coding:utf-8 -*-
#
#   Author  :   Hai Tao Yue
#   E-mail  :   bjhtyue@cn.ibm.com
#   Date    :   15/09/28 15:05:07
#   Desc    :
#
from __future__ import division
import sys
import numpy
import random
import os

class JobInitiator(object):
    def __init__(self, mean):
        self.acc_type_list = ['AES','DTW','EC','FFT','SHA']
        self.poisson_interval_mean = mean
        self.exponential_interval_mean = mean
        try:
            os.remove("job_param.txt")
        except OSError:
            pass
        self.target = open('job_param.txt','a')

    def poisson_generate_job(self, job_num):
        job_arrival_time_array = numpy.random.poisson(self.poisson_interval_mean, job_num)
        for i in range(job_num):#insert each job into fpga_jobs table
            acc_name = random.sample(self.acc_type_list,1)[0]
            in_buf_size = random.randint(1,1024)
            out_buf_size = in_buf_size
            arrival_time = job_arrival_time_array[i]/10.0
            self.target.write("%s " %str(acc_name))
            self.target.write("%s " %str(in_buf_size))
            self.target.write("%s " %str(arrival_time))
            self.target.write("\n")

        print "finally"

    def exponential_generate_job(self, job_num):
        for i in range(job_num):#insert each job into fpga_jobs table
            acc_name = random.sample(self.acc_type_list,1)[0]
            in_buf_size = random.randint(1,1024)
            out_buf_size = in_buf_size
            arrival_time = random.expovariate(self.exponential_interval_mean)
            self.target.write("%s " %str(acc_name))
            self.target.write("%s " %str(in_buf_size))
            self.target.write("%s " %str(arrival_time))
            self.target.write("\n")

        print "finally"

if __name__ == "__main__":
    if len(sys.argv) < 3:
        sys.exit("Usage: "+sys.argv[0]+" <job_num(integer)> <arrival_interval_mean(integer)")
        sys.exit(1)

    job_num = int(sys.argv[1])
    mean = int(sys.argv[2])

    job_initiator = JobInitiator(mean)

    #if time_pattern == "poisson":
    #    job_initiator.poisson_generate_job(job_num)

    #elif time_pattern == "exponential":
    #    job_initiator.exponential_generate_job(job_num)

    job_initiator.exponential_generate_job(job_num)

    #else:
    #    sys.exit("Usage: "+sys.argv[0]+" <time_pattern(poisson/exponential)> <job_num(integer)> <arrival_interval_mean(integer)")
    #    sys.exit(1)

