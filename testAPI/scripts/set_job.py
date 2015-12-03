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
        default_size = 256      #1MB
        s_size = 256*8          #8MB
        m_size = 512*1024       #2GB
        l_size = 2*1024*1024    #8GB

        mixed = {"small":0.3, "median":0.3, "large":0.4}
        job_list = [i for i in range(job_num)]
        job_param = dict()

        if self.pattern == "Small":
            for i in job_list:
                acc_name = random.sample(self.acc_type_list,1)[0]
                in_buf_size = random.randint(default_size, s_size)
                arrival_time = self.interval
                job_param[i] = (acc_name, in_buf_size, arrival_time)

        elif self.pattern == "Median":
            for i in job_list:
                acc_name = random.sample(self.acc_type_list,1)[0]
                in_buf_size = random.randint(s_size+1, m_size)
                arrival_time = self.interval
                job_param[i] = (acc_name, in_buf_size, arrival_time)

        elif self.pattern == "Large":
            for i in job_list:
                acc_name = random.sample(self.acc_type_list,1)[0]
                in_buf_size = random.randint(m_size+1, l_size)
                arrival_time = self.interval
                job_param[i] = (acc_name, in_buf_size, arrival_time)

        else:
            s_list = random.sample(job_list, int(job_num*mixed["small"]))
            job_list = list(set(job_list).difference(set(s_list)))
            m_list = random.sample(job_list, int(job_num*mixed["median"]))
            job_list = list(set(job_list).difference(set(m_list)))
            l_list = random.sample(job_list, int(job_num*mixed["large"]))
            job_list = list(set(job_list).difference(set(l_list)))
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


if __name__ == "__main__":
    if len(sys.argv) < 4:
        print "Usage: "+sys.argv[0]+" <Job_num> <Job_interval_time/second> <Pattern> <Node>"
        print "Example:"+sys.argv[0]+" 100 1 Large n1"
        print "Example:"+sys.argv[0]+" 100 3 Mixed n1"
        sys.exit(1)
    job_num = int(sys.argv[1])
    interval= int(sys.argv[2])
    pattern = sys.argv[3]
    node = sys.argv[4]
    job_initiator = JobInitiator(interval, pattern, node)
    job_initiator.generate_job(job_num)

