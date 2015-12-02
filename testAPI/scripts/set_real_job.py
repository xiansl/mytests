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
    def __init__(self, interval, pattern, node):
        self.acc_type_list = ['AES','DTW','EC','FFT','SHA']
        self.interval = int(interval)
        self.pattern = pattern
        try:
            os.remove("job-pattern/"+node+"-"+pattern+".txt")
        except OSError:
            pass
        self.target = open("job-pattern/"+node+"-"+pattern+".txt",'a')

    def generate_job(self, job_num):
        default_size = 256      #1M
        s_size = 1024*10        #40MB
        m_size = 1024*1024      #4GB
        l_size = 256*1024*10    #10GB
        mixed = {"small":0.3, "median":0.3, "large":0.4}
        job_list = [i for i in range(job_num)]

        if self.pattern="Small":
            pass
        elif self.pattern="Median":
            pass
        elif self.pattern="Large":
            pass
        else:
            s_list = random.sample(job_list, int(job_num*mixed["small"]))
            job_list = list(set(job_list).difference(set(s_list)))
            m_list = random.sample(job_list, int(job_num*mixed["median"]))
            job_list = list(set(job_list).difference(set(m_list)))
            l_list = random.sample(job_list, int(job_num*mixed["large"]))
            job_list = list(set(job_list).difference(set(l_list)))
            job_param = dict()
            for i in s_list:
                acc_name = random.sample(self.acc_type_list,1)[0]
                in_buf_size = random.randint(default_size, s_size)
                arrival_time = self.interval
                job_param[i] = (acc_name, in_buf_size, arrival_time)

            for i in m_list:
                acc_name = random.sample(self.acc_type_list,1)[0]
                in_buf_size = random.randint(s_size+1, m_size)
                arrival_time = self.interval
                job_param[i] = (acc_name, in_buf_size, arrival_time)

            for i in l_list:
                acc_name = random.sample(self.acc_type_list,1)[0]
                in_buf_size = random.randint(m_size+1, l_size)
                arrival_time = self.interval
                job_param[i] = (acc_name, in_buf_size, arrival_time)

        for i in sorted(job_param.keys()):
            term = job_param[i]
            self.target.write("%s %s %s \n" %(str(term[0]), str(term[1]), str(term[2])))

        print "job paramters created successfully. Please check 'job-pattern/%s-%s.txt'" %(node, pattern)





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

        print "job paramters created successfully. Please check 'job-%s.txt'" %node

if __name__ == "__main__":
    if len(sys.argv) < 4:
        print "Usage: "+sys.argv[0]+" <Job_num> <Pattern> <Node>"
        print "Example:"+sys.argv[0]+" 100 Large n1"
        print "Example:"+sys.argv[0]+" 100 Mixed n1"
        sys.exit(1)
    job_num = int(sys.argv[1])
    pattern = int(sys.argv[2])
    node = sys.argv[3]
    job_initiator = JobInitiator(pattern, node)
    job_initiator.generate_job(job_num)

