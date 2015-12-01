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
import os, errno
from mysql.connector import MySQLConnection,Error
from python_mysql_dbconfig import read_db_config
global CHUNK_SIZE
CHUNK_SIZE = 1024



class JobInitiator(object):
    def __init__(self, mean):
        self.dbconfig = read_db_config()
        self.conn = MySQLConnection(**self.dbconfig)
        self.acc_type_list = ['acc0','acc1','acc2','acc3','acc4']
        self.job_size_list = ['1','1','0.3','0.5','2']
        self.job_node_list = list()
        self.fpga_available_node_list = list()
        self.fpga_non_available_node_list = list()
        self.acc_bw_list = dict()
        self.poisson_interval_mean = mean
        self.exponential_interval_mean = mean
        for acc_id in self.acc_type_list:
            self.acc_bw_list[acc_id] = '1.2'
            if acc_id == 'acc4':
                self.acc_bw_list[acc_id] = '0.15'


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



    def simulate_real_job(self, path, pattern):
        acc_list = dict()
        acc_list["AES"] = ('acc0', 1.2)
        acc_list["DTW"] = ('acc1', 1.2)
        acc_list["EC"] = ('acc2', 1.2)
        acc_list["FFT"] = ('acc3', 1.2)
        acc_list["SHA"] = ('acc4', 0.15)
        query = "INSERT INTO fpga_jobs(job_id, in_buf_size, out_buf_size, acc_id, arrival_time, execution_time, from_node)"\
                "VALUES(%s, %s, %s, %s, %s, %s, %s)"
        cursor = self.conn.cursor()
        job_index = 0
        node_index = 0
        for root, dirs, files in os.walk(path):
            for file_name in files:
                node_index += 1
                if pattern == "local":
                    if node_index > len(self.fpga_available_node_list):
                        break

                if pattern == "remote":
                    if node_index <= len(self.fpga_available_node_list):
                        continue

                print file_name
                file_path = os.path.join(root, file_name)
                f = open(file_path, "r")
                lines=f.readlines()
                f.close()

                f_arrival_time = 0
                from_node = "192.168.3."+str(node_index)
                for line in lines:
                    job_param = line.split()

                    real_in_buf_size = int(job_param[1])*4/1024 #M Bytes

                    acc_id = acc_list[job_param[0]][0]
                    job_id = str(job_index)

                    f_arrival_time +=float(job_param[2])
                    arrival_time = str(f_arrival_time) #seconds

                    acc_bw = acc_list[job_param[0]][1]*1024 #M bytes

                    chunk = min(CHUNK_SIZE*4/1024, real_in_buf_size)
                    chunk_execution_time = str(chunk/(acc_bw)) #seconds
                    real_in_buf_size = str(real_in_buf_size)
                    real_out_buf_size = real_in_buf_size
                    job_index += 1

                    query_args=(job_id, real_in_buf_size, real_out_buf_size, acc_id, arrival_time, chunk_execution_time, from_node)
                    cursor.execute(query,query_args)
                    self.conn.commit()
            cursor.close()






if __name__ == "__main__":
    if len(sys.argv) < 2:
        sys.exit("Usage: "+sys.argv[0]+" <job_pattern(global/local/remote)> ")
        sys.exit(1)
    job_initiator = JobInitiator(20)
    job_initiator.create_job_table()
    job_initiator.simulate_real_job("job-info", sys.argv[1])




