#!/usr/bin/env python
# -*- coding:utf-8 -*-
#
#   Author  :   Hai Tao Yue
#   E-mail  :   bjhtyue@cn.ibm.com
#   Date    :   15/08/14 16:11:52
#   Desc    :
#
from __future__ import division
import sys
import numpy
import random

from mysql.connector import MySQLConnection,Error
from python_mysql_dbconfig import read_db_config



class JobInitiator(object):
    def __init__(self):
        self.dbconfig = read_db_config()
        self.conn = MySQLConnection(**self.dbconfig)
        self.acc_type_list = ['acc0','acc1','acc2','acc3','acc4']
        self.job_size_list = ['1','1','0.3','0.5','2']
        self.job_node_list = list()
        self.fpga_available_node_list = list()
        self.fpga_non_available_node_list = list()
        self.acc_bw_list = dict()
        self.poisson_interval_mean = 1
        self.exponential_interval_mean = 200
        for acc_id in self.acc_type_list:
            self.acc_bw_list[acc_id] = '1.2'

    def create_job_table(self):

        cursor = self.conn.cursor()
        try:
            query = "SELECT node_ip FROM fpga_nodes"
            cursor.execute(query)
            node_tuple = cursor.fetchall()
            for node in node_tuple:
                self.job_node_list.append(node[0])
            #print node[0]

            query = "SELECT node_ip FROM fpga_nodes WHERE if_fpga_available = 1"
            cursor.execute(query)
            node_tuple = cursor.fetchall()
            for node in node_tuple:
                self.fpga_available_node_list.append(node[0])
            #print 'len = %r' %len(self.fpga_available_node_list)

            query = "SELECT node_ip FROM fpga_nodes WHERE if_fpga_available = 0"
            cursor.execute(query)
            node_tuple = cursor.fetchall()
            for node in node_tuple:
                self.fpga_non_available_node_list.append(node[0])

            query1 = "DROP TABLE IF EXISTS `fpga_jobs`"

            query2 = "CREATE TABLE `fpga_jobs`("\
                    "`job_id` int(11) NOT NULL,"\
                    "`in_buf_size` varchar(40) NOT NULL,"\
                    "`out_buf_size` varchar(40) NOT NULL,"\
                    "`acc_id` varchar(40) NOT NULL,"\
                    "`arrival_time` varchar(40) NOT NULL,"\
                    "`execution_time` varchar(40) NOT NULL,"\
                    "`from_node` varchar(40) NOT NULL,"\
                    "PRIMARY KEY (`job_id`))ENGINE=InnoDB  DEFAULT CHARSET=latin1"
            cursor.execute(query1)
            cursor.execute(query2)
        except Error as e:
            print e
        finally:
            cursor.close()


    def poisson_generate_job(self, job_pattern, job_num):
        cursor = self.conn.cursor()
        job_arrival_time_array = numpy.random.poisson(self.poisson_interval_mean, job_num)
        #acc_id_list = numpy.random.randint(0,acc_num,job_num)#generate an array of random acc_id, array length = job_num
        try:
            f_arrival_time = 0.0
            query = "INSERT INTO fpga_jobs(job_id, in_buf_size, out_buf_size, acc_id, arrival_time, execution_time, from_node)"\
                     "VALUES(%s, %s, %s, %s, %s, %s, %s)"
            for i in range(job_num):#insert each job into fpga_jobs table
                job_id = str(i);
                acc_id = random.sample(self.acc_type_list,1)[0]
                acc_bw = self.acc_bw_list[acc_id]
                in_buf_size = random.sample(self.job_size_list,1)[0]
                out_buf_size = in_buf_size
                f_arrival_time += job_arrival_time_array[i]/50.0
                str_arrival_time = str(f_arrival_time)
                f_execution_time = float(in_buf_size)/float(acc_bw)
                str_execution_time = str(f_execution_time)
                if job_pattern == 'global':
                    from_node = random.sample(self.job_node_list,1)[0]
                elif job_pattern == 'local':
                    from_node = random.sample(self.fpga_available_node_list,1)[0]
                query_args=(job_id, in_buf_size, out_buf_size, acc_id, str_arrival_time, str_execution_time, from_node)
                cursor.execute(query,query_args)
                #print query_args
                print "job_id =%r, acc_id = %r, from_node %r arrival_time = %r execution_time = %r" %(job_id, acc_id, from_node, str_arrival_time, str_execution_time)
                self.conn.commit()
        except Error as e:
            print(e)

        finally:
            #print "finally"
            cursor.close()


    def exponential_generate_job(self, job_pattern, job_num):
        cursor = self.conn.cursor()
        try:
            query = "INSERT INTO fpga_jobs(job_id, in_buf_size, out_buf_size, acc_id, arrival_time, execution_time, from_node)"\
                     "VALUES(%s, %s, %s, %s, %s, %s, %s)"
            f_arrival_time = 0.0
            for i in range(job_num):#insert each job into fpga_jobs table
                job_id = str(i);
                acc_id = random.sample(self.acc_type_list,1)[0]
                acc_bw = self.acc_bw_list[acc_id]
                #in_buf_size = random.sample(self.job_size_list,1)[0]
                in_buf_size = str(random.uniform(0.5,1.5))
                out_buf_size = in_buf_size
                arrival_interval = random.expovariate(self.exponential_interval_mean)
                f_arrival_time += arrival_interval
                str_arrival_time = str(f_arrival_time)
                f_execution_time = float(in_buf_size)/float(acc_bw)
                str_execution_time = str(f_execution_time)
                if job_pattern == 'global':
                    from_node = random.sample(self.job_node_list,1)[0]
                elif job_pattern == 'local':
                    from_node = random.sample(self.fpga_available_node_list,1)[0]
                query_args=(job_id, in_buf_size, out_buf_size, acc_id, str_arrival_time, str_execution_time, from_node)
                cursor.execute(query,query_args)
                print "job_id =%r, acc_id = %r, from_node %r arrival_time = %r execution_time = %r" %(job_id, acc_id, from_node, str_arrival_time, str_execution_time)
                #print query_args
                self.conn.commit()
        except Error as e:
            print(e)

        finally:
            #print "finally"
            cursor.close()

if __name__ == "__main__":
    if len(sys.argv) < 4:
        sys.exit("Usage: "+sys.argv[0]+" <job_pattern(global/local)> <time_pattern(poisson/exponential/normal)> <job_num(integer)>")
        sys.exit(1)

    job_initiator = JobInitiator()
    job_initiator.create_job_table()
    job_pattern = sys.argv[1]
    time_pattern = sys.argv[2]
    job_num = int(sys.argv[3])

    if job_pattern != "global" and job_pattern != "local":
        sys.exit("Usage: "+sys.argv[0]+" <job_pattern(global/local)> <time_pattern(poisson/exponential/normal)> <job_num(integer)>")
        sys.exit(1)

    if time_pattern == "poisson":
        job_initiator.poisson_generate_job(job_pattern, job_num)

    elif time_pattern == "exponential":
        job_initiator.exponential_generate_job(job_pattern, job_num)



