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
    def __init__(self, node):
        self.acc_type_list = ['AES','DTW','EC','FFT','SHA']
        try:
            os.remove("job-"+node+".txt")
        except OSError:
            pass
        self.target = open("job-"+node+".txt",'a')

    def generate_job(self, job_num, small=2, middle=4, large=8):
        s_size = int(small*256000)
        m_size = int(middle*256000)
        l_size = int(large*256000)
        s_num = 0.2
        m_num = 0.6
        l_num = 0.2
        job_list = [i for i in range(job_num)]
        s_list = random.sample(job_list, int(job_num*s_num))
        job_list = list(set(job_list).difference(set(s_list)))
        m_list = random.sample(job_list, int(job_num*m_num))
        job_list = list(set(job_list).difference(set(m_list)))
        l_list = random.sample(job_list, int(job_num*l_num))
        job_param = dict()
        for i in s_list:
            acc_name = random.sample(self.acc_type_list,1)[0]
            in_buf_size = s_size
            arrival_time = 1
            job_param[i] = (acc_name, in_buf_size, arrival_time)

        for i in m_list:
            acc_name = random.sample(self.acc_type_list,1)[0]
            in_buf_size = m_size
            arrival_time = 1
            job_param[i] = (acc_name, in_buf_size, arrival_time)

        for i in l_list:
            acc_name = random.sample(self.acc_type_list,1)[0]
            in_buf_size = l_size
            arrival_time = 1
            job_param[i] = (acc_name, in_buf_size, arrival_time)

        for i in sorted(job_param.keys()):
            term = job_param[i]
            self.target.write("%s %s %s \n" %(str(term[0]), str(term[1]), str(term[2])))


        print "job paramters created successfully. job_num: %r. Please check 'job-%s.txt'" %(len(job_param), node)
        print "s_num:%r m_num:%r l_num:%r" %(int(s_num*job_num), int(m_num*job_num), int(l_num*job_num))






if __name__ == "__main__":
    if len(sys.argv) < 3:
        sys.exit("Usage: "+sys.argv[0]+" <job_num(integer)> <node>")
        sys.exit(1)
    job_num = int(sys.argv[1])
    node = sys.argv[2]
    job_initiator = JobInitiator(node)
    job_initiator.generate_job(job_num)

