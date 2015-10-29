#!/usr/bin/env python
# -*- coding:utf-8 -*-
#
#   Author  :   Hai Tao Yue
#   E-mail  :   bjhtyue@cn.ibm.com
#   Date    :   15/08/15 21:34:33
#   Desc    :
#
from __future__ import division
import sys
import numpy
import random
from mysql.connector import MySQLConnection,Error
from python_mysql_dbconfig import read_db_config

class SystemInitiator(object):
    def __init__(self):
        self.dbconfig = read_db_config()
        self.conn = MySQLConnection(**self.dbconfig)
        self.acc_type_list = dict()
        self.total_node_num = 100
        self.fpga_node_num = 50
        self.each_section_num = 4
        self.acc_type_list[0] = ('acc0','Erasurecode','1.2','1')
        self.acc_type_list[1] = ('acc1','AES','1.2','1')
        self.acc_type_list[2] = ('acc2','FFT','1.2','1')
        self.acc_type_list[3] = ('acc3','DTW','1.2','1')
        self.acc_type_list[4] = ('acc4','SHA','0.15','0')
        self.acc_type_num = len(self.acc_type_list)


    def create_acc_table(self):
        cursor = self.conn.cursor()
        try:
            query1 = "DROP TABLE IF EXISTS `acc_type_list`"
            query2 = "CREATE TABLE `acc_type_list` ("\
                    "`id` int(11) NOT NULL,"\
                    "`acc_id` varchar(40) NOT NULL,"\
                    "`acc_name` varchar(40) NOT NULL,"\
                    "`acc_peak_bw` varchar(40) NOT NULL,"\
                    "`if_flow_intensive` int(1) NOT NULL,"\
                    "PRIMARY KEY (`id`)"\
                    ") ENGINE=InnoDB  DEFAULT CHARSET=latin1"
            cursor.execute(query1)
            cursor.execute(query2)
        except Error as e:
            print e

        finally:
            cursor.close()


    def insert_into_acc_type_list(self):
        cursor = self.conn.cursor()
        query = "INSERT  INTO `acc_type_list`("\
                "`id`,`acc_id`,`acc_name`,"\
                "`acc_peak_bw`,`if_flow_intensive`) "\
                "VALUES(%s,%s,%s,%s,%s)"
        try:

            args = dict()
            args[0] = ('1','acc0','Erasurecode','1.2','1')
            args[1] = ('2','acc1','AES','1.2','1')
            args[2] = ('3','acc2','FFT','1.2','1')
            args[3] = ('4','acc3','DTW','1.2','1')
            args[4] = ('5','acc4','SHA','1.2','0')

            for i in range(5):
                cursor.execute(query, args[i])
                self.conn.commit()

        except Error as e:
            print e
        finally:
            cursor.close()


    def create_node_table(self):
        cursor = self.conn.cursor()
        try:
            query1 = "DROP TABLE IF EXISTS `fpga_nodes`"
            query2 = "CREATE TABLE `fpga_nodes`("\
                    "`id` int(11) NOT NULL,"\
                    "`node_ip` varchar(40) NOT NULL,"\
                    "`pcie_bw` varchar(40) NOT NULL,"\
                   "`if_fpga_available` int(1) NOT NULL,"\
                    "`section_num` int(2) NOT NULL,"\
                    "`roce_bw` varchar(40) NOT NULL,"\
                    "`roce_latency` varchar(40) NOT NULL,"\
                    "PRIMARY KEY(`id`)"\
                    ")ENGINE=InnoDB DEFAULT CHARSET=latin1"

            cursor.execute(query1)
            cursor.execute(query2)


        except Error as e:
            print e

        finally:
            cursor.close()

    def insert_into_fpga_node(self):
        cursor = self.conn.cursor()
        query = "INSERT INTO `fpga_nodes`"\
                "(`id`,`node_ip`,`pcie_bw`,"\
                "`if_fpga_available`,`section_num`,"\
                "`roce_bw`, `roce_latency`) "\
                "VALUES(%s, %s, %s, %s, %s, %s, %s)"
        try:
            for i in range(self.total_node_num):
                id = str(i)
                node_index = 1+i
                node_ip = "192.168.3."+str(node_index)
                #pcie_bw = str(2.8)
                pcie_bw = str(4.8)
                if i%2:
                    if_fpga_available = 0
                    section_num = 0
                else:
                    if_fpga_available = 1
                    section_num = self.each_section_num
                roce_bw = str(40/8)
                roce_latency = str(12)
                query_args = (id, node_ip,
                              pcie_bw, if_fpga_available,
                              section_num,roce_bw, roce_latency)
                cursor.execute(query, query_args)
                self.conn.commit()
        except Error as e:
            print e
        finally:
            cursor.close()



    def create_section_table(self):
        cursor = self.conn.cursor()
        try:
            query1 = "DROP TABLE IF EXISTS `fpga_resources`"
            query2 = "CREATE TABLE `fpga_resources` ("\
                    "`id` int(11) NOT NULL,"\
                    "`node_ip` varchar(40) NOT NULL,"\
                    "`section_id` varchar(40) NOT NULL,"\
                    "`acc_id` varchar(40),"\
                    "`acc_name` varchar(40),"\
                    "`acc_peak_bw` varchar(11),"\
                    "`if_flow_intensive` int(1),"\
                    "PRIMARY KEY(`id`))ENGINE=InnoDB DEFAULT CHARSET=latin1"
            cursor.execute(query1)
            cursor.execute(query2)
        except Error as e:
            print e

        finally:
            cursor.close()


    def insert_into_fpga_section(self):
        cursor = self.conn.cursor()
        query = "insert  into `fpga_resources`("\
                "`id`,`node_ip`,"\
                "`section_id`,`acc_id`,"\
                "`acc_name`,`acc_peak_bw`,"\
                "`if_flow_intensive`) "\
                "VALUES (%s,%s,%s,%s,%s,%s,%s)"
        try:
            i = 0
            for node_index in range(self.total_node_num):
                if node_index%2 == 0:
                    for sec_index in range(self.each_section_num):
                        for acc_index in range(self.acc_type_num):
                            id = str(i+1)
                            i += 1
                            node_ip = "192.168.3."+str(node_index+1)
                            section_id = node_ip+":section"+str(sec_index)
                            acc_id = self.acc_type_list[acc_index][0]
                            acc_name = self.acc_type_list[acc_index][1]
                            acc_peak_bw = self.acc_type_list[acc_index][2]
                            if_flow_intensive = self.acc_type_list[acc_index][3]
                            query_args = (id, node_ip,
                                          section_id, acc_id,
                                          acc_name, acc_peak_bw,
                                          if_flow_intensive)
                            cursor.execute(query,query_args)

                            self.conn.commit()

        except Error as e:
            print e
        finally:
            cursor.close()

if __name__ == "__main__":
    system_initiator = SystemInitiator()
    system_initiator.create_acc_table()
    system_initiator.insert_into_acc_type_list()
    system_initiator.create_node_table()
    system_initiator.insert_into_fpga_node()
    system_initiator.create_section_table()
    system_initiator.insert_into_fpga_section()
