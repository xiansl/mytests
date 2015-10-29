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
#def select_node(acc_name):
#    return_dict = dict()
#    return_dict['node_ip'] = "30.0.0.51"
#    return_dict['node_url'] = "http://30.0.0.51:9000"
#    return_dict['section_id'] = "0"
#    return return_dict

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
        self.waiting_job_queue = list()#job_id

class FpgaSection(object):
    def __init__(self, section_id, port_id, node_ip, compatible_acc_list,
                 current_acc_id='',
                 current_job_id=''):
        self.section_id = section_id
        self.port_id = port_id
        self.node_ip = node_ip
        self.if_idle = True
        self.current_job_id = 0
        self.compatible_acc_list = list()
        self.waiting_job_queue = list()#job_id
        self.job_queue = list()#each waited job is handled on its corresponding queue by FIFO order
        for acc in compatible_acc_list:
            self.compatible_acc_list.append(acc)

        self.current_acc_id = current_acc_id
        self.current_job_id = current_job_id


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

        self.job_execution_time = 0
        self.job_configuration_time = 0
        self.job_complete_time = 0

        self.job_efficiency = 0
        self.job_total_time = 0


class FpgaScheduler(object):
    def __init__(self):
        self.current_job_id = 0
        self.current_job_count = 0
        self.job_list = dict()
        self.node_list = dict()
        self.section_list = dict()
        self.acc_type_list = dict() # <acc_name:acc_bw>
        self.node_list = dict() # <node_ip: PowerNode>
        self.job_list = dict()

        self.dbconfig = read_db_config()
        self.conn = MySQLConnection(**self.dbconfig)

    def initiate_acc_type_list(self):
        cursor = self.conn.cursor()
        query = "SELECT acc_name, acc_peak_bw FROM acc_type_list"
        try:
            cursor.execute(query)
            accs = cursor.fetchall()
            #print('Total available acc type(s):', cursor.rowcount)

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


    def select_node(self, job_id):
        job_node_ip = self.job_list[job_id].job_node_ip
        if self.node_list[job_node_ip].if_fpga_available == 1:
            return job_node_ip
        else:
            waiting_job_num = 100
            return_node_ip = 0
            for node_ip, node in self.node_list.items():
                if node.if_fpga_available == 1:
                    if len(node.waiting_job_queue) < waiting_job_num:
                        waiting_job_num = len(node.waiting_job_queue)
                        return_node_ip = node_ip
		        print "return_node_ip =%r" %return_node_ip
		        print "waiting job = %r" %waiting_job_num
			#return return_node_ip
            if return_node_ip == 0:
                print "oooops.  fail to select an available node"
            return return_node_ip

    def select_port(self, fpga_node_ip, job_id):
        section_num = self.node_list[fpga_node_ip].section_num
        for i in range(section_num):
            port_id = str(i)
            section_id = fpga_node_ip+":section"+port_id
            if self.section_list[section_id].if_idle == True:
                return port_id
    	return None

    def execute_scheduling(self, job_id):
	print "begin scheduling"
        fpga_node_ip = self.select_node(self.current_job_id) 
	port_id = self.select_port(fpga_node_ip, self.current_job_id)
        if port_id == None:
            print "oooooops. fail to select an idle slot"
        return (fpga_node_ip, port_id)


    def on_receive_request(self, job_node_ip, in_buffer_size, out_buffer_size, acc_name):
        mutex.acquire()
        ret = dict()
        self.current_job_count+=1
        self.current_job_id = uuid.uuid4()
        job_arrival_time = time.time()
        readable_time=time.strftime("job arrives at %i:%m:%s id is ")+str(self.current_job_count)
        print readable_time

        self.job_list[self.current_job_id] = FpgaJob(self.current_job_id, job_node_ip, in_buffer_size, out_buffer_size, acc_name, job_arrival_time)

        fpga_slot_info = self.execute_scheduling(self.current_job_id)
        fpga_node_ip = fpga_slot_info[0]
        port_id = fpga_slot_info[1]
        if port_id == None:
            ret["ifuse"] = "1"
            ret["ip"] =""
            ret["port"] = ""
            ret["section_id"] = ""

        else:
            if fpga_node_ip == job_node_ip:
                print "open a local fpga accelerator"
                ret["ip"] = fpga_node_ip
                ret["port"] = ""
                ret["ifuse"] = "3"
                ret["section_id"] = port_id

            else:
	        print "ask the corresponding node server to open remote FPGA"
                node_url = self.node_list[fpga_node_ip].node_url
		print "url = %r, port_id =%s " %(node_url, port_id,)
    	        node_server = xmlrpclib.ServerProxy(node_url)
    	        ret = node_server.rpc_service(port_id, in_buffer_size, out_buffer_size, acc_name)
                ret["section_id"] = port_id

        if ret["ifuse"] == "0" or ret["ifuse"] == "3":

            section_id = fpga_node_ip+":section"+port_id

            self.section_list[section_id].if_idle = False
            self.section_list[section_id].current_job_id = self.current_job_id
            self.section_list[section_id].waiting_job_queue.append(self.current_job_id)
            self.node_list[fpga_node_ip].waiting_job_queue.append(self.current_job_id)

    	for i,j in ret.items():
            print i,j,type(j)

        mutex.release()
        return ret

    def update_job_info(self, current_job_id, job_execution_time):
        job_complete_time = time.time()
        self.job_list[current_job_id].job_execution_time = float(job_execution_time)
        self.job_list[current_job_id].job_complete_time = job_complete_time
        self.job_list[current_job_id].job_total_time = job_complete_time - self.job_list[current_job_id].job_arrival_time
        self.job_list[current_job_id].job_efficiency = self.job_list[current_job_id].job_execution_time/self.job_list[current_job_id].job_total_time

    def on_receive_complete(self, node_ip, port_id, job_execution_time):
        mutex.acquire()
        self.current_job_count -= 1
        section_id = node_ip+":section"+port_id
        current_job_id = self.section_list[section_id].current_job_id
        self.section_list[section_id].if_idle = True
        self.section_list[section_id].waiting_job_queue.remove(current_job_id)
        self.node_list[node_ip].waiting_job_queue.remove(current_job_id)

        readable_time=time.strftime("Job completes at %I:%M:%S Id is")+str(self.current_job_count)
        print readable_time
        self.update_job_info(current_job_id, job_execution_time)
        mutex.release()
        return_dict = dict();
	return_dict["status"] ="0"
	return return_dict

#def select_node(client_node_ip, job_id):
    ##query = "SELECT url FROM fpga_resources WHERE acc_name = %s"
    ##query_arg = (acc_name,)
    #try:
    #    #cursor = conn.cursor()
    #    #cursor.execute(query, query_arg)
    #    #rows = cursor.fetchall()
    #    #print('Total available sections(s):', cursor.rowcount)
    #    #rand = random.random()*cursor.rowcount
    #    #node_url = str(rows[int(math.floor(rand))][0])
	#node_url = "http://30.0.0.51:9000"
	#node_ip = "30.0.0.51"
	#section_id = 0
	#return_dict = dict()
	#return_dict[node_ip] = node_ip
	#return_dict[node_url] = node_url
	#return_dict[section_id] = section_id

    #except Error as e:
    #    print(e)

    #finally:
    #    #cursor.close()
    #    return fpga_node_ip





#def on_receive_request(client_node_ip, in_buffer_size, out_buffer_size, acc_name):
#    ret = dict()
#    node_info = select_node(acc_name)
#    server_node_ip = node_info['node_ip']
#    section_id = node_info['section_id']
#    node_url = node_info['node_url']
#    print "node_url = %r" %node_url
#    if client_node_ip == server_node_ip:
# 	print "open a local FPGA"
#        ret['ip'] = ""
#        ret['port'] = ""
#        ret['ifuse'] = "3"
#        ret['section_id'] = section_id
#    	for i,j in ret.items():
#        	print i,j,type(j)
#        return ret
#    else:
#    	node_server = xmlrpclib.ServerProxy(node_url)
#	print "ask the corresponding node server to open remote FPGA"
#    	ret = node_server.rpc_service(section_id, in_buffer_size, out_buffer_size, acc_name)
#        ret['section_id'] = section_id
#    	for i,j in ret.items():
#        	print i,j,type(j)
#    	return ret
#    #node_server = xmlrpclib.ServerProxy(node_url)
#    #print "select completed"
#    #ret = node_server.rpc_service(section_id, in_buffer_size, out_buffer_size, acc_name)
#    #ret['section_id'] = section_id
#    #for i,j in ret.items():
#    #    print i,j,type(j)
#    #return ret



def run_scheduler(addr, port):
    scheduler_object = FpgaScheduler()
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
    parser = argparse.ArgumentParser(description="This is used to start a RPC server.")
    parser.add_argument("-a","--addr", help = "The interface ip_address to start RPC server", required = True)
    parser.add_argument("-p", "--port", help = "The port number to bind", required = True)
    args = parser.parse_args()
    run_scheduler(args.addr, args.port)
