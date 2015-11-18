#!/usr/bin/env python
# -*- coding:utf-8 -*-
#
#   Author  :   Zhuang Di ZHU
#   E-mail  :   zhuangdizhu@yahoo.com
#   Date    :   15/10/30 18:54:10
#   Desc    :
#
from __future__ import division
import socket
import json
import struct
import time, thread
import datetime
import sys
import uuid
import random
from SocketServer import (BaseRequestHandler as BRH, ThreadingMixIn as TMI, TCPServer)

print sys.argv

global mutex, debug, BUFFER_SIZE, recv_fmt, send_fmt, scheduler, RDMA_FLAG, TCP_FLAG
mutex = thread.allocate_lock()
debug = 0
BUFFER_SIZE = 1024
recv_fmt = "!16s16s16s16s16s16s"
send_fmt = "!16s16s16s16s16s"
RDMA_FLAG = 0
TCP_FLAG = 1


class TCPServer(TCPServer):
    allow_reuse_address = True


class Server(TMI, TCPServer):
    pass


class MyRequestHandler(BRH):
    def handle(self):
        recv_data = self.request.recv(BUFFER_SIZE)
        recv_data = struct.unpack(recv_fmt, recv_data)
        if debug:
            for i in recv_data:
                print i
        # -- in-come data contains: status, real_in_size, in_buf_size, out_buf_size, acc_name, job_id(vacant) --#
        data = list()
        for i in recv_data:
            data.append(i.strip('\x00'))

        # handle client's request, and return the value
        global scheduler
        status = data[0].strip('\x00')

        if status == "open":
            ret = scheduler.handle_open_request(self.client_address, data)
            schedule_ret = (ret, "open")
            response = self.on_send_response(schedule_ret)
            self.request.sendall(response)

        elif status == "close":
            response = "closed"
            self.request.sendall(response)
            scheduler.handle_close_request(data)


    def on_send_response(self, ret):
        if debug:
            print "return data:"
            for i in ret[0]:
                print type(i)

        send_data = struct.pack(send_fmt, str(ret[0][0]), str(ret[0][1]), str(ret[0][2]), str(ret[0][3]),
                                    str(ret[0][4]))
        return send_data


class PowerNode(object):
    def __init__(self, node_id, node_ip, server_port, pcie_bw, if_fpga_available, section_num, roce_bw):
        self.node_id = node_id
        self.node_ip = node_ip
        self.pcie_bw = pcie_bw
        self.node_port = server_port
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
        self.job_fpga_port = ""
        self.job_server_ip = ""
        self.job_current_section_id = ""


class FpgaScheduler(object):
    def __init__(self, port, algorithm):
        self.current_job_count = 0
        self.recv_size = 16 * 6
        self.recv_fmt = "!16s16s16s16s16s16s"
        self.send_size = 16 * 5
        self.send_fmt = "!16s16s16s16s16s"
        self.backlog = 10
        self.recv_from_server_size = 1024
        self.port = port
        self.scheduling_algorithm = algorithm
        self.section_list = dict()
        self.acc_type_list = dict()  # <acc_name:acc_bw>
        self.node_list = dict()  # <node_ip: PowerNode>
        self.job_list = dict()
        self.job_waiting_list = dict()  # <job_id: weight> could be arrival time, execution time, etc;
        self.epoch_time = time.time()
        self.job_arrival_time = time.time()

    def initiate_acc_type_list(self):
        acc_names = ["AES", "EC", "DTW", "FFT", "SHA"]
        acc_bws = ["1200", "1200", "1200", "1200", "150"]
        for i, name in enumerate(acc_names):
            self.acc_type_list[name] = acc_bws[i]

    def initiate_node_status(self):
        f = open("/home/tian/testAPI/fpga_node.txt", "r")
        lines = f.readlines()
        f.close()
        lines.pop(0)
        for i, line in enumerate(lines):
            node_info = line.split()
            node_id = i
            node_ip = node_info[0]
            server_port = node_info[1]
            pcie_bw = float(node_info[2])
            if_fpga_available = int(node_info[3])
            if if_fpga_available == 1:
                section_num = 4
            else:
                section_num = 0
            roce_bw = float(node_info[4] * 1024)
            self.node_list[node_ip] = PowerNode(node_id, node_ip, server_port, pcie_bw, if_fpga_available, section_num, roce_bw)
            if if_fpga_available == 1:
                for i in range(section_num):
                    section_id = node_ip + ":section" + str(i)
                    self.node_list[node_ip].section_list.append(section_id)

    def initiate_section_status(self):
        for node_ip, node in self.node_list.items():
            if node.if_fpga_available == 1:
                for i in range(node.section_num):
                    port_id = str(i)
                    section_id = node_ip + ":section" + port_id
                    compatible_acc_list = ["AES", "EC", "DTW", "FFT", "SHA"]
                    self.section_list[section_id] = FpgaSection(section_id, port_id, node_ip, compatible_acc_list)

    def initiate_system_status(self):
        print "[0]Begin to load system infomation ..."
        self.initiate_acc_type_list()
        self.initiate_node_status()
        self.initiate_section_status()
        print "[1] Information loaded. Begin running simulator ..."

    def execute_scheduling(self, job_id, event_type):
        if self.scheduling_algorithm == "FIFO":
            self.conduct_fifo_scheduling(job_id, event_type)
        else:
            self.conduct_locality_scheduling(job_id, event_type)

    def conduct_locality_scheduling(self, job_id, event_type):
        if event_type == "JOB_ARRIVAL":
            job_node_ip = self.job_list[job_id].job_node_ip

            if self.node_list[job_node_ip].if_fpga_available == True:
                idle_section_list = list()
                for section_id in self.node_list[job_node_ip].section_list:
                    if self.section_list[section_id].if_idle == True:
                        idle_section_list.append(section_id)

                if len(idle_section_list):
                    section_id = random.sample(idle_section_list,1)[0]
                    self.trigger_new_job(job_id, section_id)
                    return

            else:
                idle_section_list = list()
                for section_id, section in self.section_list.items():
                    if section.if_idle == True:
                        idle_section_list.append(section_id)

                if len(idle_section_list):
                    #print "section len = %r ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~" %(len(idle_section_list))
                    id_index = random.randint(0, len(idle_section_list) - 1)
                    # print "id_index %r" %id_index
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

            if len(self.job_waiting_list):
                sorted_list = sorted(self.job_waiting_list.items(), lambda x, y: cmp(x[1], y[1]))
                default_job_id = sorted_list[-1][0]
                for i in range(len(sorted_list)):
                    job_id = sorted_list[i][0]
                    job_node_ip = self.job_list[job_id].job_node_ip
                    if self.node_list[job_node_ip].if_fpga_available == False or section_node_ip == job_node_ip:
                        self.trigger_new_job(job_id, section_id)
                        self.remove_job_from_wait_queue(job_id)
                        return

                self.trigger_new_job(default_job_id, section_id)
                self.remove_job_from_wait_queue(job_id)
            return

    def trigger_new_job(self, job_id, section_id):
        print "[job %r] TRIGGERED on [%r]:" %(job_id, section_id)
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
        self.section_list[section_id].current_job_id.append(job_id)
        mutex.release()

        if job_node_ip == section.node_ip:
            self.job_list[job_id].job_status = "3"
            self.job_list[job_id].job_if_local = True

        else:
            self.job_list[job_id].job_status = "0"
            if RDMA_FLAG == 1:
                self.job_list[job_id].job_status = "2"
            self.job_list[job_id].job_if_local = False

    def remove_job_from_wait_queue(self, current_job_id):
        if current_job_id in self.job_waiting_list:
            del(self.job_waiting_list[current_job_id])



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

    def handle_open_request(self, client_addr, data):
        # -- in-come data contains: status, real_in_size, in_buf_size, out_buf_size, acc_name, job_id(vacant) --#
        # -- send-data contains: host, port, section_id, status, job_id --#
        if debug:
            print "client_addr:", client_addr[0], client_addr[1]
            for i in data:
                print i
        job_node_ip = client_addr[0]
        real_in_buffer_size = data[1]
        in_buffer_size = data[2]
        out_buffer_size = data[3]
        acc_name = data[4]
        self.current_job_count += 1
        current_job_id = self.current_job_count
        job_arrival_time = (10 ** 6) * (time.time() - self.epoch_time)
        interval = job_arrival_time - self.job_arrival_time
        self.job_arrival_time = job_arrival_time
        print "[job %r] ARRIVES after %.0f micro-secs, from %r, %r" % (current_job_id, interval, client_addr[0], client_addr[1])
        job_acc_bw = self.acc_type_list[acc_name]
        job_execution_time = (10 ** 6) * float(real_in_buffer_size) / float(job_acc_bw) / (2 ** 20)
        self.job_list[current_job_id] = FpgaJob(current_job_id, job_node_ip, in_buffer_size, out_buffer_size, acc_name,
                                                job_arrival_time)
        self.job_list[current_job_id].job_execution_time = job_execution_time

        self.execute_scheduling(current_job_id, "JOB_ARRIVAL")
        if self.job_list[current_job_id].job_if_triggered == 0:
            self.add_job_to_wait_queue(current_job_id)

        while (self.job_list[current_job_id].job_if_triggered == 0):
            pass

        ret = self.launch_new_job(current_job_id)

        if debug:
            print "ret:"
            for i in ret:
                print i

        return ret

    def handle_close_request(self, data):
        status = list()
        for i in data:
            status.append(i.strip('\x00'))
        # data contains: status, job_id, open_time, execution_time, close_time, total_time
        job_id = int(data[1])
        #job_open_time = float(data[2])
        #job_execution_time = float(data[3])
        #job_close_time = float(data[4])
        #job_total_time = float(data[5])
        #job_arrival_time = self.job_list[job_id].job_arrival_time
        #job_complete_time = job_arrival_time + job_total_time
        self.update_section_info(job_id)
        #self.update_job_info(job_id, job_open_time, job_execution_time, job_total_time, job_complete_time)
        self.execute_scheduling(job_id, "JOB_COMPLETE")
        print "[job %r] COMPLETES" % job_id

    def update_section_info(self, job_id):
        mutex.acquire()
        section_id = self.job_list[job_id].job_current_section_id
        self.section_list[section_id].current_job_id.remove(job_id)
        if len(self.section_list[section_id].current_job_id) == 0:
            self.section_list[section_id].if_idle = True
        mutex.release()

    def update_job_info(self, current_job_id, job_open_time, job_real_execution_time, job_total_time,
                        job_complete_time):

        self.job_list[current_job_id].job_open_time = float(job_open_time)
        self.job_list[current_job_id].job_real_execution_time = float(job_real_execution_time)
        self.job_list[current_job_id].job_total_time = float(job_total_time)

        self.job_list[current_job_id].job_complete_time = job_complete_time

        self.job_list[current_job_id].job_efficiency = self.job_list[current_job_id].job_real_execution_time / \
                                                       self.job_list[current_job_id].job_total_time
        return

    def launch_new_job(self, current_job_id):
        #print "begin to launch [job %r]" % current_job_id
        section_id = self.job_list[current_job_id].job_current_section_id
        fpga_node_ip = self.job_list[current_job_id].job_server_ip
        fpga_section_id = self.job_list[current_job_id].job_fpga_port
        fpga_node_port = self.node_list[fpga_node_ip].node_port
        fpga_status = self.job_list[current_job_id].job_status

        if fpga_status == "3":
            #print "open a LOCAL acc."
            host = fpga_node_ip
            port = ""
            section_id = fpga_section_id
            status = "3"
            job_id = current_job_id
            ret = (host, port, section_id, status, job_id)
        elif fpga_status == "0" or fpga_status == "2":
            #print "open a REMOTE acc"
            server_host = fpga_node_ip
            server_port = int(fpga_node_port)
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            s.connect((server_host, server_port))
            s_data = dict()
            s_data["job_id"] = str(current_job_id)
            s_data["status"] = fpga_status
            s_data["section_id"] = fpga_section_id
            s_data["in_buf_size"] = str(self.job_list[current_job_id].job_in_buf_size)
            s_data["out_buf_size"] = str(self.job_list[current_job_id].job_out_buf_size)
            s_data["acc_name"] = self.job_list[current_job_id].job_acc_name
            s_data = json.dumps(s_data)
            s.send(s_data)
            response = s.recv(self.recv_from_server_size)
            s.close()
            response = json.loads(response)

            if fpga_status == "0":
                host = server_host
            elif fpga_status == "2":
                host = response["ip"]
            port = response["port"]
            section_id = fpga_section_id
            status = response["ifuse"]
            job_id = current_job_id
            ret = (host, port, section_id, status, job_id)
        else:
            print "Unknown status:%r" %fpga_status

        return ret


def run_scheduler(port, algorithm):
    global scheduler
    scheduler = FpgaScheduler(port, algorithm)
    print type(scheduler)
    scheduler.initiate_system_status()
    host = ''
    address = (host, port)
    server = Server(address, MyRequestHandler)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print 'Existing ...'
        server.server_close()


if __name__ == "__main__":
    if len(sys.argv) != 3:
        print "Usage: ", sys.argv[0], "<port> <algorithm>"
        print "Example: ./scheduler 9000 FIFO"
    else:
        algorithm = sys.argv[2]
        if algorithm == "FIFO":
            print "Using algorithm FIFO ...."
        else:
            print "Using algorithm LOCALITY Sensitive ...."
        run_scheduler(int(sys.argv[1]), algorithm)
