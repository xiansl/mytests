#!/usr/bin/env python
# -*- coding:utf-8 -*-
#
#   Author  :   Zhuang Di ZHU
#   E-mail  :   zhuangdizhu@yahoo.com
#   Date    :   15/09/28 15:05:07
#   Desc    :
#
from __future__ import division
import sys
import numpy
import random
import os

class JobInitiator(object):
    def __init__(self, mean, node):
        self.acc_type_list = ['AES','DTW','EC','FFT','SHA']
        self.poisson_interval_mean = mean
        self.exponential_interval_mean = mean
        try:
            os.remove("job-"+node+"-f"+str(mean)+".txt")
        except OSError:
            pass
        self.target = open("job-"+node+"-f"+str(mean)+".txt",'a')

    def generate_job(self, job_num):
        s_size = 2048000
        m_size = 4096000
        l_size = 8192000
        s_num = 0.2
        m_num = 0.6
        l_num = 0.2
        job_list = [i for i in range(job_num)]
        s_list = random.sample(job_list, int(job_num*s_num))
        m_list = random.sample(job_list, int(job_num*m_num))
        l_list = random.sample(job_list, int(job_num*l_num))
        job_param = dict()
        for i in s_list:
            acc_name = random.sample(self.acc_type_list,1)[0]
            in_buf_size = s_size
            arrival_time = 2
            job_param[i] = (acc_name, in_buf_size, arrival_time)

        for i in m_list:
            acc_name = random.sample(self.acc_type_list,1)[0]
            in_buf_size = m_size
            arrival_time = 2
            job_param[i] = (acc_name, in_buf_size, arrival_time)

        for i in l_list:
            acc_name = random.sample(self.acc_type_list,1)[0]
            in_buf_size = l_size
            arrival_time = 2
            job_param[i] = (acc_name, in_buf_size, arrival_time)

        for i in sorted(job_param.keys()):
            term = job_param[i]
            self.target.write("%s %s %s \n" %(str(term[0]), str(term[1]), str(term[2])))

        print "job paramters created successfully. Please check 'job-%s-f%s.txt'" %(node, str(mean))





    def poisson_generate_job(self, job_num):
        job_arrival_time_array = numpy.random.poisson(self.poisson_interval_mean, job_num)
        for i in range(job_num):#insert each job into fpga_jobs table
            acc_name = random.sample(self.acc_type_list,1)[0]
            in_buf_size = random.randint(256,1024)
            out_buf_size = in_buf_size
            arrival_time = job_arrival_time_array[i]/10.0
            self.target.write("%s " %str(acc_name))
            self.target.write("%s " %str(in_buf_size))
            self.target.write("%s " %str(arrival_time))
            self.target.write("\n")

        print "job paramters created successfully. Please check 'job-%s-f%s.txt'" %(node, str(mean))

    def exponential_generate_job(self, job_num):
        for i in range(job_num):#insert each job into fpga_jobs table
            acc_name = random.sample(self.acc_type_list,1)[0]
            in_buf_size = random.randint(256,1024)
            out_buf_size = in_buf_size
            arrival_time = random.expovariate(self.exponential_interval_mean)
            self.target.write("%s " %str(acc_name))
            self.target.write("%s " %str(in_buf_size))
            self.target.write("%s " %str(arrival_time))
            self.target.write("\n")

        print "job paramters created successfully. Please check 'job_%s.txt'" %node

if __name__ == "__main__":
    if len(sys.argv) < 4:
        sys.exit("Usage: "+sys.argv[0]+" <job_num(integer)> <arrival_interval_mean(integer) <node>")
        sys.exit(1)
    job_num = int(sys.argv[1])
    mean = int(sys.argv[2])
    node = sys.argv[3]
    job_initiator = JobInitiator(mean, node)
    job_initiator.generate_job(job_num)

