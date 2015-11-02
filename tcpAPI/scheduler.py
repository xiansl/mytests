#!/usr/bin/env python
# -*- coding:utf-8 -*-
#
#   Author  :   Zhuang Di ZHU
#   E-mail  :   zhuangdizhu@yahoo.com
#   Date    :   15/10/30 18:54:10
#   Desc    :   
#
import socket
import json
import struct
import time, thread
import datetime
import sys
import argparse
import math
import uuid
print sys.argv

global mutex
mutex = thread.allocate_lock()
debug = 1

class PowerNode(object):
    def __init__(self, node_id, node_ip, node_url, pcie_bw, if_fpga_available, section_num, roce_bw):
		self.node_id = node_id
		self.node_ip = node_ip
		self.node_url = node_url
		self.pcie_bw = pcie_bw
		self.if_fpga_available = if_fpga_available
		self.section_num = section_num
		self.roce_bw = roce_bw
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

        self.current_job_id = list()
        self.current_acc_name = None
        self.current_acc_bw = None

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
	def __init__(self, port, algorithm):
		self.count = 0
		self.recv_size = 16*6
		self.recv_fmt = "!16s16s16s16s16s16s"
		self.send_size = 16*5
		self.send_fmt = "!16s16s16s16s16s"
		self.backlog = 10
		self.recv_from_server_size = 1024 
		self.port = port
		self.scheduling_algorithm = algorithm

		self.section_list = dict()
		self.acc_type_list = dict() # <acc_name:acc_bw>
		self.node_list = dict() # <node_ip: PowerNode>
		self.job_list = dict()
		self.job_waiting_list = dict()#<job_id: weight> could be arrival time, execution time, etc;

	def deamon_start(self):
		s=socket.socket(socket.AF_INET, socket.SOCK_STREAM)
		s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
		s.bind(('',self.port))
		s.listen(self.backlog)
		while 1:
			client,client_addr = s.accept()
			data = client.recv(self.recv_size)
			recv_data = struct.unpack(self.recv_fmt,data)
			#if debug:
			#	for i in recv_data:
			#		print i
			ret = self.on_receive_request(client_addr, recv_data)
			if ret[1] == "open":
				if debug:
					print "return data:"
					for i in ret[0]:
						print type(i)
				send_data = struct.pack(self.send_fmt,str(ret[0][0]), str(ret[0][1]), str(ret[0][2]), str(ret[0][3]), str(ret[0][4]))
				client.send(send_data)
				client.close()

			elif ret[1] == "close":
				send_data = "closed"
				client.send(send_data)
				client.close()

	def on_receive_request(self, client_addr, data):
		status = data[0]
		#if debug:
		#	print "status :'%s'" %status
		#	for i in status:
		#		print "'%r'" %i
		#	status = status.strip('\x00')
		#	print status
		status = status.strip('\x00')
		if status == "open":
			ret = self.handle_open_request(client_addr, data)
			return (ret, "open")
		else:
			ret = self.handle_close_request(data)
			return (ret, "close")

	def close_acc(self, job_id):
		print job_id

	def handle_open_request(self, client_addr, data):
		job_id = self.current_job_count
		self.current_job_count += 1
		if debug:
			print "client_addr:", client_addr[0], client_addr[1]
			for i in data:
				print i
		ret = self.launch_new_job(job_id)
		if debug:
			print "ret:"
			for i in ret:
				print i
		return ret


	def handle_close_request(self, data):
		job_id = data[5];
		self.close_acc(job_id)

	def deamon_close(self):
		print "Existing"


	def launch_new_job(self, current_job_id):
		section_id = "0"
		status = "0"
		job_id = "1"
		print "send request to remote server ..."
		server_host = 'localhost'
		server_port = 5000
		s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
		s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
		s.connect((server_host, server_port))
		s_data = dict()
		s_data["section_id"] = "0"
		s_data["in_buf_size"] = "4096"
		s_data["out_buf_size"] = "4096"
		s_data["acc_name"] = "AES"
		s_data = json.dumps(s_data)
		s.send(s_data)
		response = s.recv(self.recv_from_server_size)
		response = json.loads(response)

		host = response["ip"]
		port = response["port"]
		status = response["ifuse"]
		#status = "3"
		ret = (host, port, section_id, status, job_id)
		return  ret




def run_scheduler(port, algorithm):
	scheduler = FpgaScheduler(port, algorithm)
	#scheduler.initiate_system_status()
	try:
		scheduler.deamon_start()
	except KeyboardInterrupt:
		scheduler.deamon_close()


if __name__ == "__main__":
	if len(sys.argv) != 3:
		print "Usage: ", sys.argv[0], "<port> <algorithm>"
		print "Example: ./scheduler 9000 FIFO"
	else:
		algorithm = sys.argv[2]
		print "Using %r algorithm ...." %algorithm
		run_scheduler(int(sys.argv[1]), algorithm)
