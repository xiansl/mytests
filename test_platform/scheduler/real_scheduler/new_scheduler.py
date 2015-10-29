#!/usr/bin/env python

from __future__ import division
import  SimpleXMLRPCServer,SocketServer
from xmlrpclib import Binary
import time, thread
import xmlrpclib
import datetime
import argparse
import sys
import random
import math
import uuid
print sys.argv
from mysql.connector import MySQLConnection, Error
from python_mysql_dbconfig import read_db_config

global mutex
mutex = thread.allocate_lock()
dbconfig = read_db_config()
conn = MySQLConnection(**dbconfig)

class RPCThreading(SocketServer.ThreadingMixIn, SimpleXMLRPCServer.SimpleXMLRPCServer):
        pass

class PowerNode(object):
    def __init__(self, node_id, node_ip, node_url, pcie_bw, if_fpga_available, section_num, roce_bw, roce_latency):
        self.node_id = node_id
        self.node_ip = node_ip
        self.node_url = node_url
        self.pcie_bw = pcie_bw
        self.if_fpga_available = if_fpga_available
        self.section_num = section_num
        self.roce_bw = roce_bw
        self.roce_latency = roce_latency
        self.section_list = list()
        self.job_waiting_list = dict()

class FpgaSection(object):
    def __init__(self, section_id, port_id, node_ip, compatible_acc_list,
                 current_acc_id=''):
        self.section_id = section_id
        self.port_id = port_id
        self.node_ip = node_ip
        self.if_idle = True
        self.compatible_acc_list = list()
        for acc in compatible_acc_list:
            self.compatible_acc_list.append(acc)

        self.current_acc_name = None
        self.current_acc_bw = None
        self.current_job_id = None


class FpgaJob(object):
    def __init__(self,
                 job_id,
                 job_node_ip,
                 job_in_buf_size,
                 job_out_buf_size,
                 job_acc_name,
                 job_arrival_time):

        self.job_id = 0
        self.job_node_ip = job_node_ip
        self.job_in_buf_size = job_in_buf_size
        self.job_out_buf_size = job_out_buf_size
        self.job_acc_name = job_acc_name
        self.job_arrival_time = job_arrival_time
	self.job_open_time = 0
        self.job_execution_time = 0

        self.job_start_time = 0

        self.job_real_execution_time = 0
        self.job_total_time = 0
        self.job_efficiency = 0
        self.job_complete_time = 0

        self.job_if_triggered = 0

        self.job_if_local = ""
        self.job_status = ""
        self.job_fpga_port =""
        self.job_server_ip =""
        self.job_current_section_id = ""

class FpgaScheduler(object):
    def __init__(self, scheduling_algorithm):
        self.scheduling_algorithm = scheduling_algorithm
        self.current_job_count = 0
        self.job_list = dict()
        self.node_list = dict()
        self.section_list = dict()
        self.acc_type_list = dict() # <acc_name:acc_bw>
        self.node_list = dict() # <node_ip: PowerNode>
        self.job_list = dict()
        self.job_waiting_list = dict()#<job_id: weight> could be arrival time, execution time, etc;

        self.dbconfig = read_db_config()
        self.conn = MySQLConnection(**self.dbconfig)

    def initiate_acc_type_list(self):
        cursor = self.conn.cursor()
        query = "SELECT acc_name, acc_peak_bw FROM acc_type_list"
        try:
            cursor.execute(query)
            accs = cursor.fetchall()

            for acc in accs:
                acc_name = acc[0]
                acc_peak_bw = float(acc[1])*1024
                self.acc_type_list[acc_name] = acc_peak_bw

        except Error as e:
            print(e)

        finally:
            cursor.close()
    def initiate_node_status(self):
        cursor = self.conn.cursor()
        query = "select id, node_ip, node_url, pcie_bw, if_fpga_available, section_num, roce_bw, roce_latency FROM fpga_nodes"
        try:
            cursor.execute(query)
            nodes = cursor.fetchall()
            #print('Total fpga_node(s):', cursor.rowcount)
            for node in nodes:
                node_id = int(node[0])
                node_ip = node[1]
                node_url = node[2]
                pcie_bw = float(node[3])*1024
                if_fpga_available = int(node[4])
                section_num = int(node[5])
                roce_bw = float(node[6])*1024
                roce_latency = float(node[7])/(10 ** 6)
                self.node_list[node_ip] = PowerNode(node_id, node_ip, node_url, pcie_bw, if_fpga_available, section_num, roce_bw, roce_latency)
                if int(node[4]) == 1:
                    for i in range(section_num):
                        section_id = node_ip+":section"+str(i)
                        #print section_id
                        self.node_list[node_ip].section_list.append(section_id)

        except Error as e:
            print(e)

        finally:
            cursor.close()

    def initiate_section_status(self):
        cursor = self.conn.cursor()
        query = "SELECT acc_name FROM fpga_resources WHERE node_ip = %s AND port_id = %s"
        try:
            for node_ip, node in self.node_list.items():
                #print "node_ip = %r" %node_ip
                #print "section_num = %r" %node.section_num
                for i in range(node.section_num):
                    #print "i=%r" %i
                    compatible_acc_list = list()
                    port_id = str(i)
                    #print "section_id = %r" %section_id
                    query_args = (node_ip, port_id)
                    cursor.execute(query,query_args)
                    acc_list = cursor.fetchall()
                    #print "acc_list =%r" %len(acc_list)
                    for acc in acc_list:# acc=(acc_name,)
                        #print "acc=%r,%r" %(acc,type(acc))
                        compatible_acc_list.append(acc[0])
                    #print "%r,%r" %(section_id, acc_list)
                    section_id = node_ip+":section"+port_id
                    self.section_list[section_id] = FpgaSection(section_id, port_id, node_ip, compatible_acc_list)

        except Error as e:
            print(e)

        finally:
            cursor.close()


    def initiate_system_status(self):
        print "[0]Begin to load system infomation ..."
        self.initiate_acc_type_list()
        self.initiate_node_status()
        self.initiate_section_status()
        print "[1] Information oaded. Begin running simulator ..."



    def execute_scheduling(self, job_id, event_type):
        if self.scheduling_algorithm == "SJF" or self.scheduling_algorithm == "FIFO":
            self.conduct_sjf_fifo_scheduling(job_id, event_type)
        elif self.scheduling_algorithm == "RANDOM":
            self.conduct_random_scheduling(job_id, event_type)
        elif self.scheduling_algorithm == "LOCALITY1":
            self.conduct_locality_scheduling(job_id, event_type)
        elif self.scheduling_algorithm == "LOCALITY2":
            self.conduct_absolute_locality_scheduling(job_id, event_type)

    def conduct_absolute_locality_scheduling(job_id, event_type):
        if event_type == "JOB_ARRIVAL":
            job_node_ip = self.job_list[job_id].job_node_ip
            if self.node_list[job_node_ip].if_fpga_available == True:
                for section_id in self.node_list[job_node_ip].section_list:
                    if self.section_list[section_id].if_idle == True:
                        self.trigger_new_job(job_id, section_id)
                        return

            idle_section_list = list()
            for section_id, section in self.section_list.items():
                if section.if_idle == True:
                    idle_section_list.append(section_id)
            if len(idle_section_list):
                id_index = random.randint(0, len(idle_section_list)-1)
                section_id = idle_section_list[id_index]
                self.trigger_new_job(job_id, section_id)
                return

            self.add_job_to_wait_queue(job_id)


        elif event_type == "JOB_COMPLETE":
            section_id = self.job_list[job_id].job_current_section_id
            section = self.section_list[section_id]
            section_node_ip = section.node_ip
            node = self.node_list[section_node_ip]
            self.section_list[section_id].if_idle = True

            if len(node.job_waiting_list):
                sorted_list = sorted(node.job_waiting_list.items(), lambda x,y: cmp(x[1], y[1]))
                new_job_id = sorted_list[0][0]
                self.trigger_new_job(new_job_id, section_id)
                return

            elif len(self.job_waiting_list):
                sorted_list = sorted(self.job_waiting_list.items(), lambda x,y: cmp(x[1], y[1]))
                new_job_id = sorted_list[0][0]
                for i in range(len(sorted_list)):
                    job_id = sorted_list[i][0]
                    job_node_ip = self.job_list[job_id].job_node_ip
                    if self.node_list[job_node_ip].if_fpga_available == False:
                       new_job_id = sorted_list[i][0]
                       break
                self.trigger_new_job(new_job_id, section_id)
                return

            return


    def conduct_locality_scheduling(job_id, event_type):
        if event_type == "JOB_ARRIVAL":
            job_node_ip = self.job_list[job_id].job_node_ip
            if self.node_list[job_node_ip].if_fpga_available == True:
                for section_id in self.node_list[job_node_ip].section_list:
                    if self.section_list[section_id].if_idle == True:
                        self.trigger_new_job(job_id, section_id)
                        return

            idle_section_list = list()
            for section_id, section in self.section_list.items():
                if section.if_idle == True:
                    idle_section_list.append(section_id)
            if len(idle_section_list):
                id_index = random.randint(0, len(idle_section_list)-1)
                section_id = idle_section_list[id_index]
                self.trigger_new_job(job_id, section_id)
                return
            self.add_job_to_wait_queue(job_id)


        elif event_type == "JOB_COMPLETE":
            section_id = self.job_list[job_id].job_current_section_id
            section = self.section_list[section_id]
            section_node_ip = section.node_ip
            node = self.node_list[section_node_ip]
            self.section_list[section_id].if_idle = True

            if len(node.job_waiting_list):
                sorted_list = sorted(node.job_waiting_list.items(), lambda x,y: cmp(x[1], y[1]))
                new_job_id = sorted_list[0][0]
                self.trigger_new_job(new_job_id, section_id)
                return

            if len(self.job_waiting_list):
                sorted_list = sorted(self.job_waiting_list.items(), lambda x,y: cmp(x[1], y[1]))
                new_job_id = sorted_list[0][0]
                self.trigger_new_job(new_job_id, section_id)
                return

            return

    def conduct_random_scheduling(self, job_id, event_type):
        if event_type == "JOB_ARRIVAL":
            idle_section_list = list()
            for section_id, section in self.section_list.items():
                if section.if_idle == True:
                    idle_section_list.append(section_id)
            if len(idle_section_list):
                id_index = random.randint(0, len(idle_section_list)-1)
                section_id = idle_section_list[id_index]

                self.trigger_new_job(job_id, section_id)
                return

            self.add_job_to_wait_queue(job_id)


        elif event_type == "JOB_COMPLETE":
            section_id = self.job_list[job_id].job_current_section_id
            self.section_list[section_id].if_idle = True

            if len(self.job_waiting_list):
                sorted_list = sorted(self.job_waiting_list.items(), lambda x,y: cmp(x[1], y[1]))
                new_job_id = sorted_list[0][0]
                self.trigger_new_job(new_job_id, section_id)
                return
            return

    def conduct_sjf_fifo_scheduling(self, job_id, event_type):
        if event_type == "JOB_ARRIVAL":
            job_node_ip = self.job_list[job_id].job_node_ip
            if self.node_list[job_node_ip].if_fpga_available == True:
                for section_id in self.node_list[job_node_ip].section_list:
                    if self.section_list[section_id].if_idle == True:
                        self.trigger_new_job(job_id, section_id)
                        return
            else:
                for node_ip, node in self.node_list.items():
                    for section_id in node.section_list:
                        if self.section_list[section_id].if_idle == True:
                            self.trigger_new_job(job_id, section_id)
                            return


        elif event_type == "JOB_COMPLETE":
            section_id = self.job_list[job_id].job_current_section_id
            section = self.section_list[section_id]
            section_node_ip = section.node_ip
            node = self.node_list[section_node_ip]
            self.section_list[section_id].if_idle = True

            if len(node.job_waiting_list):
                for job_id in node.job_waiting_list:
                    sorted_list = sorted(node.job_waiting_list.items(), lambda x,y: cmp(x[1], y[1]))
                    new_job_id = sorted_list[0][0]
                    self.trigger_new_job(new_job_id, section_id)

            elif len(self.job_waiting_list):
                for job_id in self.job_waiting_list:
                    sorted_list = sorted(self.job_waiting_list.items(), lambda x,y: cmp(x[1], y[1]))
                    new_job_id = sorted_list[0][0]
                    self.trigger_new_job(new_job_id, section_id)

            return

    def conduct_network_aware_scheduling(self, job_id, event_type):
        if event_type == "JOB_ARRIVAL":
            job_node_ip = self.job_list[job_id].job_node_ip
            if self.node_list[job_node_ip].if_fpga_available == True:
                for section_id in self.node_list[job_node_ip].section_list:
                    if self.section_list[section_id].if_idle == True:
                        self.trigger_new_job(job_id, section_id)
                        return
            else:
                for node_ip, node in self.node_list.items():
                    idle_section_list = list()
                    network_heavy_section_list = list()
                    for section_id in node.section_list:
                        if self.section_list[section_id].if_idle == False:
                            job_id= self.section_list[section_id].current_job_id
                            if self.job_list[job_id].job_if_local == False:
                                network_heavy_section_list.append(section_id)
                        else:
                            idle_section_list.append(section_id)
                    if len(network_heavy_section_list) == 0 and len(idle_section_list):
                        section_id = idle_section_list[0]
                        self.trigger_new_job(job_id, section_id)
                        return
            self.add_job_to_wait_queue(job_id)


        elif event_type == "JOB_COMPLETE":
            section_id = self.job_list[job_id].job_current_section_id
            section = self.section_list[section_id]
            section_node_ip = section.node_ip
            node = self.node_list[section_node_ip]
            self.section_list[section_id].if_idle = True

            for section_id in node.section_list:
                if self.section_list[section_id].if_idle == False:
                    job_id= self.section_list[section_id].current_job_id
                    if self.job_list[job_id].job_if_local == False:
                        network_heavy_section_list.append(section_id)
                else:
                    idle_section_list.append(section_id)

            if len(network_heavy_section_list) == 0 and len(idle_section_list):
                if len(self.job_waiting_list):
                    for job_id in self.job_waiting_list:
                        sorted_list = sorted(self.job_waiting_list.items(), lambda x,y: cmp(x[1], y[1]))
                        new_job_id = sorted_list[0][0]
                        section_id = idle_section_list[0]
                        self.trigger_new_job(new_job_id, section_id)
                        return

            if len(idle_section_list):
                if len(node.job_waiting_list):
                    for job_id in node.job_waiting_list:
                        sorted_list = sorted(node.job_waiting_list.items(), lambda x,y: cmp(x[1], y[1]))
                        new_job_id = sorted_list[0][0]
                        section_id = idle_section_list[0]
                        self.trigger_new_job(new_job_id, section_id)
                        return

        return

    def conduct_pcie_sensitive_scheduling(self, job_id, event_type):
        if event_type == "JOB_ARRIVAL":
            job_node_ip = self.job_list[job_id].job_node_ip
            if self.node_list[job_node_ip].if_fpga_available == True:
                current_bw = 0
                idle_section_list = list()
                for section_id in self.node_list[job_node_ip].section_list:
                    if self.section_list[section_id].if_idle == False:
                        acc_bw = self.section_list[section_id].current_acc_bw
                        current_bw += acc_bw
                    else:
                        idle_section_list.append(section_id)

                my_acc_name = self.job_list[job_id].job_acc_name
                my_acc_bw = self.acc_type_list[my_acc_name]

                if current_bw + my_acc_bw <= self.node_list[job_node_ip].pcie_bw:
                    section_id = idle_section_list[0]
                    self.trigger_new_job(job_id, section_id)
                    return
            else:
                for node_ip, node in self.node_list.items():
                    if node.if_fpga_available == True:
                        current_bw = 0
                        idle_section_list = list()
                        for section_id in node.section_list:
                            if self.section_list[section_id].if_idle == False:
                                acc_bw = self.section_list[section_id].current_acc_bw
                                current_bw += acc_bw
                            else:
                                idle_section_list.append(section_id)

                        my_acc_name = self.job_list[job_id].job_acc_name
                        my_acc_bw = self.acc_type_list[my_acc_name]

                        if current_bw + my_acc_bw <= self.node_list[node_ip].pcie_bw:
                            section_id = idle_section_list[0]
                            self.trigger_new_job(job_id, section_id)
                            return
                return




        elif event_type == "JOB_COMPLETE":
            section_id = self.job_list[job_id].job_current_section_id
            section = self.section_list[section_id]
            section_node_ip = section.node_ip

            node = self.node_list[section_node_ip]
            self.section_list[section_id].if_idle = True

            current_bw = 0
            idle_section_list = list()

            for section_id in node.section_id_list:
                if self.section_list[section_id].if_idle == False:
                    acc_bw = self.section_list[section_id].current_acc_bw
                    current_bw += acc_bw
                else:
                    idle_section_list.append(section_id)

            if current_bw < node.pcie_bw:

                if len(node.job_waiting_list):
                    sorted_list = sorted(node.job_waiting_list.items(), lambda x,y: cmp(x[1], y[1]))
                    for job in sorted_list:
                        my_acc_name = self.job_list[job[0]].job_acc_name
                        my_acc_bw = self.acc_type_list[my_acc_name]

                        if current_bw + my_acc_bw <= node.pcie_bw:
                            section_id = idle_section_list[0]
                            new_job_id = job[0]
                            self.trigger_new_job(new_job_id, section_id)
                            return

                elif len(self.job_waiting_list):
                    sorted_list = sorted(self.job_waiting_list.items(), lambda x,y: cmp(x[1], y[1]))
                    for job in sorted_list:
                        my_acc_name = self.job_list[job[0]].job_acc_name
                        my_acc_bw = self.acc_type_list[my_acc_name]
                        if current_bw + my_acc_bw <= node.pcie_bw:
                            section_id = idle_section_list[0]
                            new_job_id = job[0]
                            self.trigger_new_job(new_job_id, section_id)
                            return
            return

    def trigger_new_job(self, job_id, section_id):
        job_node_ip = self.job_list[job_id].job_node_ip
        job_acc_name = self.job_list[job_id].job_acc_name
        section = self.section_list[section_id]
        mutex.acquire()
        self.job_list[job_id].job_server_ip = section.node_ip
        self.job_list[job_id].job_fpga_port = section.port_id
        self.job_list[job_id].job_current_section_id = section.section_id
        self.job_list[job_id].job_if_triggered = 1
        self.section_list[section_id].if_idle = False
        self.section_list[section_id].current_acc_name = job_acc_name
        self.section_list[section_id].current_acc_bw = self.acc_type_list[job_acc_name]
        self.section_list[section_id].current_job_id = job_id
        mutex.release()

        if job_node_ip == section.node_ip:
            self.job_list[job_id].job_status = "3"
            self.job_list[job_id].job_if_local = True
        else:
            self.job_list[job_id].job_status = "0"
            self.job_list[job_id].job_if_local = False


    def launch_new_job(self, current_job_id):
        ret = dict()
        ret["ifuse"] = "1"
        ret["ip"] =""
        ret["port"] = ""
        ret["section_id"] = ""
        ret["job_id"] = ""

        print "begin to launch job %r" %current_job_id
        section_id = self.job_list[current_job_id].job_current_section_id
        fpga_node_ip = self.job_list[current_job_id].job_server_ip
        fpga_port = self.job_list[current_job_id].job_fpga_port
        node_url = self.node_list[fpga_node_ip].node_url

        if self.job_list[current_job_id].job_status == "3":
            print "open a local fpga accelerator"
            ret["ifuse"] = "3"
    	    ret["ip"] = fpga_node_ip
            ret["section_id"] = fpga_port
            ret["job_id"] = current_job_id


        elif self.job_list[current_job_id].job_status == "0":
            print "ask the corresponding node server to open remote FPGA"
            node_server = xmlrpclib.ServerProxy(node_url)
            section_id = fpga_node_ip+":section"+fpga_port
            ret = node_server.rpc_service(fpga_port, in_buffer_size, out_buffer_size, acc_name)
            ret["job_id"] = current_job_id
            ret["section_id"] = fpga_port


        if ret["ifuse"] == "3" or ret["ifuse"] == "0":
            mutex.acquire()
            self.section_list[section_id].current_job_id = current_job_id
            self.remove_job_from_waiting_queue(current_job_id)
            mutex.release()

            for i,j in ret.items():
                print i,j,type(j)
		print ""

        return ret

    def remove_job_from_waiting_queue(self, current_job_id):
        job_node_ip = self.job_list[current_job_id].job_node_ip
        if self.node_list[job_node_ip].if_fpga_available == True:
            if current_job_id in self.node_list[job_node_ip].job_waiting_list:
            	del(self.node_list[job_node_ip].job_waiting_list[current_job_id])

        if current_job_id in self.job_waiting_list:
            del(self.job_waiting_list[current_job_id])

    def on_receive_request(self, job_node_ip, in_buffer_size, out_buffer_size, acc_name):

        mutex.acquire()
        self.current_job_count+=1
        mutex.release()
        current_job_id = str(self.current_job_count)
        job_arrival_time = time.time()
        print "job_arrival_time %r" %job_arrival_time
        job_acc_bw = self.acc_type_list[acc_name]
        job_execution_time = float(in_buffer_size)/float(job_acc_bw)
        readable_time=time.strftime("job arrives at %i:%m:%s id is ")+str(self.current_job_count)
        print readable_time

        mutex.acquire()
        self.job_list[current_job_id] = FpgaJob(current_job_id, job_node_ip, in_buffer_size, out_buffer_size, acc_name, job_arrival_time)
        self.job_list[current_job_id].job_execution_time = job_execution_time
        mutex.release()

        self.execute_scheduling(current_job_id, "JOB_ARRIVAL")

        if self.job_list[current_job_id].job_if_triggered == 0:
            self.add_job_to_wait_queue(current_job_id)

        while (self.job_list[current_job_id].job_if_triggered == 0):
            pass

        ret = self.launch_new_job(current_job_id)

        return ret


    def add_job_to_wait_queue(self, current_job_id):
        job_node_ip = self.job_list[current_job_id].job_node_ip
        time = ""
        if self.scheduling_algorithm == "SJF":
            time = float(self.job_list[current_job_id].job_execution_time)
        else:
            time = float(self.job_list[current_job_id].job_arrival_time)

        if self.node_list[job_node_ip].if_fpga_available == True:
            self.node_list[job_node_ip].job_waiting_list[current_job_id] = time

        self.job_waiting_list[current_job_id] = time


    def update_job_info(self, current_job_id, job_open_time, job_real_execution_time, job_total_time):

        self.job_list[current_job_id].job_open_time = float(job_open_time)
        self.job_list[current_job_id].job_real_execution_time = float(job_real_execution_time)
        self.job_list[current_job_id].job_total_time = float(job_total_time)
        self.job_list[current_job_id].job_complete_time = float(job_total_time) + self.job_list[current_job_id].job_arrival_time

        self.job_list[current_job_id].job_efficiency = self.job_list[current_job_id].job_real_execution_time/self.job_list[current_job_id].job_total_time
        return

    def on_receive_complete(self, job_id, job_open_time, job_real_execution_time, job_total_time):
	print "open: %r, execute: %r, total: %r" %(job_open_time, job_real_execution_time, job_total_time)
	print ""
    	current_job_id = job_id
        mutex.acquire()
        section_id = self.job_list[current_job_id].job_current_section_id
        self.section_list[section_id].if_idle = True
        mutex.release()

        readable_time=time.strftime("job completes at %i:%m:%s id is")+current_job_id
        print readable_time

        self.update_job_info(current_job_id, job_open_time, job_real_execution_time, job_total_time)

        self.execute_scheduling(job_id, "JOB_COMPLETE")

        return_dict = dict();
        return_dict["status"] ="0"
        return return_dict




def run_scheduler(addr, port, scheduling_algorithm):
    scheduler_object = FpgaScheduler(scheduling_algorithm)
    scheduler_object.initiate_system_status()
    server = RPCThreading((addr, int(port)))
    server.register_instance(scheduler_object)
    print 'Scheudler started. Use Control-C to exit'
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print 'Exiting'
        server.server_close()



if __name__ == "__main__":
    #parser = argparse.ArgumentParser(description="This is used to start a RPC server.")
    #parser.add_argument("-a","--addr", help = "The interface ip_address to start RPC server", required = True)
    #parser.add_argument("-p", "--port", help = "The port number to bind", required = True)
    #parser.add_argument("-s", "--scheduler", help = "The scheduling algorithm", required = True)
    #args = parser.parse_args()
    #run_scheduler(args.addr, args.port)
    if len(sys.argv) != 4:
        print "Usage: ",sys.argv[0], " <IPADDR>  <PORT>  <Scheduling Algorithm>"
        print "Example: ./python_scheduler localhost 9000 FIFO"

    else:
        scheduling_algorithm = ""
        if sys.argv[3] == "FIFO":
            scheduling_algorithm = "FIFO"
        elif sys.argv[3] == "SJF":
            scheduling_algorithm = "SJF"
        else :
            scheduling_algorithm = "SJF"

        run_scheduler(sys.argv[1], sys.argv[2], scheduling_algorithm)

