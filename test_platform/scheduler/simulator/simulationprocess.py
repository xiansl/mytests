#!/usr/bin/env python
# -*- coding:utf-8 -*-
#
#   Author  :   Hai Tao Yue
#   E-mail  :   bjhtyue@cn.ibm.com
#   Date    :   15/08/05 15:39:56
#   Desc    :
#
from __future__ import division
import uuid
import math
import random
import sys
from collections import namedtuple
from collections import defaultdict
from enum import Enum
#from mysql.connector import MySQLConnection, Error                                                                                                                          |
from mysql.connector import MySQLConnection,Error
from python_mysql_dbconfig import read_db_config

AccType = namedtuple('AccType',['acc_id', 'acc_name', 'acc_peak_bw', 'if_flow_intensive', 'acc_configuration_time'])
EventType = Enum('EventType','JOB_ARRIVAL JOB_START JOB_FINISH JOB_COMPLETE')
SchedulingAlgorithm = Enum('SchedulingAlgorithm','RANDOM FIFO FAIR_SHARING TEST_1 TEST_2 TEST_3 LOCAL_1')
class FpgaEvent(object):
    def __init__(self, event_id, event_type, event_time='', job_id='', section_id=''):
        self.event_type = event_type
        self.event_time = event_time
        self.job_id = job_id
        self.section_id = section_id

    def set_event_time(self,event_time):
        self.event_time = event_time

    def set_job_id(self, job_id):
        self.job_id = job_id

    def set_section_id(self, section_id):
        self.section_id = section_id




class FpgaJob(object):
    def __init__(self,
                 job_id, job_in_buf_size, job_out_buf_size, job_acc_id, job_arrival_time,node_ip):
        self.job_id = job_id
        self.job_in_buf_size = job_in_buf_size
        self.job_out_buf_size = job_out_buf_size
        self.job_acc_id = job_acc_id
        self.job_arrival_time = job_arrival_time
        self.node_ip = node_ip

        self.job_configuration_time = 0.0
        self.job_start_time = 0
        self.job_finish_time = 0
        self.section_id = 0
        self.job_complete_time = 0
        self.job_associate_event_list = [0,0,0,0]#this list contains three events:job_arrival, job_start, job_finish
                                                #each element in the list is a tuple: (event_id,event_time)
        self.job_anp = 0
        self.job_ideal_time = 0
        self.job_real_time = 0
        self.job_source_transfer_time = 0
        self.job_result_transfer_time = 0
        self.job_if_local = True
        self.if_configured = 0

    def set_job_arrival_event(self, event_id):
        self.job_associate_event_list[0]=(event_id, self.job_arrival_time)





class FpgaSection(object):
    def __init__(self,section_id, node_associate_id, node_ip, compatible_acc_list,
                 current_acc_id='',
                 current_acc_bw='',
                 current_job_id=''):
        self.section_id = section_id
        self.node_associated_id = node_associate_id
        self.node_ip = node_ip
        self.if_idle = True
        self.compatible_acc_list = list()
        self.job_queue = list()#each waited job is handled on its corresponding queue by FIFO order
        for acc in compatible_acc_list:
            self.compatible_acc_list.append(acc)

        self.current_acc_id = current_acc_id
        self.current_acc_bw = current_acc_bw

        self.current_job_id = current_job_id




class PowerNode(object):
    def __init__(self, node_ip, if_fpga_available, section_num, pcie_bw, roce_bw, roce_latency):
        self.node_ip = node_ip
        self.if_fpga_available = if_fpga_available
        self.section_num = section_num
        self.pcie_bw = pcie_bw
        self.roce_bw = roce_bw
        self.roce_latency = roce_latency
        self.section_id_list = list()


class FpgaSimulator(object):
    def __init__(self, scheduling_algorithm):
        self.event_sequence = defaultdict(list)
        self.waiting_job_list=list()#the first element in the waiting_job_list must have wait the longest time
        self.event_list = dict()#indexed by event_id
        self.acc_type_list = dict()#indexed by acc_id
        self.fpga_job_list =dict()#indexed by job_id
        self.fpga_section_list =dict()#indexd by section_id
        self.node_list =dict()#indexed by node_ip
        self.job_count = 0
        self.last_time = 0
        self.current_time = 0
        self.current_event_id = 0
        self.current_event_type = 0
        self.current_job_id = 0
        self.network_real_workload = 0
        self.network_ideal_workload = 0
        self.snp = 0
        self.make_span = 0
        self.ssp = 0
        self.reconfiguration_num = 0

        self.scheduling_algorithm = scheduling_algorithm
        self.dbconfig = read_db_config()
        self.conn = MySQLConnection(**self.dbconfig)


    def insert_event(self, job_id, event_type):
        event_time = 0
        event_id = 0
        job = self.fpga_job_list[job_id]
        if event_type == EventType.JOB_ARRIVAL:
            event_time = job.job_arrival_time
            event_id = job.job_associate_event_list[0][0]

        elif event_type == EventType.JOB_START:
            event_time = job.job_start_time
            event_id = job.job_associate_event_list[1][0]

        elif event_type == EventType.JOB_FINISH:
            event_time = job.job_finish_time
            event_id = job.job_associate_event_list[2][0]

        elif event_type == EventType.JOB_COMPLETE:
            event_time = job.job_complete_time
            event_id = job.job_associate_event_list[3][0]

        else:
            print 'ERROR! No such event type'
            pass

        self.event_sequence[event_time].append(event_id)
        self.event_list[event_id] = FpgaEvent(event_id, event_type, event_time, job_id)


    def remove_event(self,job_id,event_type,earlier_event_time='default'):
        event_time = 0
        event_id = 0
        job = self.fpga_job_list[job_id]
        if event_type == EventType.JOB_ARRIVAL:
            event_time = job.job_arrival_time
            event_id = job.job_associate_event_list[0][0]

        elif event_type == EventType.JOB_START:
            event_time = job.job_start_time
            event_id = job.job_associate_event_list[1][0]

        elif event_type == EventType.JOB_FINISH:
            event_time = job.job_finish_time
            event_id = job.job_associate_event_list[2][0]

        elif event_type == EventType.JOB_COMPLETE:
            event_time = job.job_complete_time
            event_id = job.job_associate_event_list[3][0]

        else:
            pass

        if earlier_event_time !='default':
            event_time = earlier_event_time

        i = 0
        for c_event_id in self.event_sequence[event_time]:
            if event_id == c_event_id:
                del(self.event_sequence[event_time][i])
                if len(self.event_sequence[event_time])==0:
                    del(self.event_sequence[event_time])
                del(self.event_list[event_id])
                break
            i+=1

        return event_id

    def remove_current_event(self):

        del(self.event_sequence[self.current_time][0])
        if len(self.event_sequence[self.current_time])==0:
            del(self.event_sequence[self.current_time])
        del(self.event_list[self.current_event_id])
        #print "remaining event num:"
        #print 'remaining event number is %r' % (len(self.event_list))
        #for event_time, event_id_list in self.event_sequence.items():
        #    print event_time
        #    for event_id in event_id_list:
        #        print event_id


    def initiate_available_acc_type_list(self):
        cursor = self.conn.cursor()
        query = "SELECT acc_id, acc_name, acc_peak_bw ,if_flow_intensive FROM acc_type_list"
        try:
            cursor.execute(query)
            accs = cursor.fetchall()
            #print('Total available acc type(s):', cursor.rowcount)
            for acc in accs:
                acc_peak_bw = float(acc[2])*1024
                self.acc_type_list[acc[0]] = AccType(acc[0], acc[1], acc_peak_bw, acc[3], 0.1)
        except Error as e:
            print(e)

        finally:
            cursor.close()

    def initiate_node_status(self):
        cursor = self.conn.cursor()
        query = "select node_ip, pcie_bw, if_fpga_available, section_num, roce_bw, roce_latency FROM fpga_nodes"
        #self.node_list['localhost'] = PowerNode('localhost',4, 1.2*1024)
        try:
            cursor.execute(query)
            nodes = cursor.fetchall()
            #print('Total fpga_node(s):', cursor.rowcount)
            for node in nodes:
                node_ip = node[0]
                pcie_bw = float(node[1])*1024
                if_fpga_available = int(node[2])
                section_num = int(node[3])
                roce_bw = float(node[4])*1024
                roce_latency = float(node[5])/(10 ** 6)
                self.node_list[node_ip] = PowerNode(node_ip, if_fpga_available, section_num, pcie_bw, roce_bw, roce_latency)
                if node[2] == 1:
                    for i in range(section_num):
                        #print i
                        section_id = node_ip+":section"+str(i)
                        #print section_id
                        self.node_list[node_ip].section_id_list.append(section_id)


        except Error as e:
            print(e)

        finally:
            cursor.close()

    def initiate_fpga_resources(self):
        cursor = self.conn.cursor()
        query = "SELECT acc_id FROM fpga_resources WHERE node_ip = %s AND section_id = %s"
        try:
            #print "len = %r" %len(self.node_list)
            for node_ip, node in self.node_list.items():
                #print "node_ip = %r" %node_ip
                #print "section_num = %r" %node.section_num
                for i in range(node.section_num):
                    #print "i=%r" %i
                    compatible_acc_list = list()
                    section_id = node_ip+":section"+str(i)
                    #print "section_id = %r" %section_id
                    query_args = (node_ip, section_id)
                    cursor.execute(query,query_args)
                    acc_list = cursor.fetchall()
                    #print "acc_list =%r" %len(acc_list)
                    for acc in acc_list:# acc=(acc_id,)
                        #print "acc=%r,%r" %(acc,type(acc))
                        compatible_acc_list.append(acc[0])
                    #print "%r,%r" %(section_id, acc_list)
                    self.fpga_section_list[section_id] = FpgaSection(section_id, i, node_ip, compatible_acc_list)

        except Error as e:
            print(e)

        finally:
            cursor.close()

        #for i in range(4):
        #    sec_id = 'localhost: section'+str(i)
        #    #print 'section_id =%r' %(sec_id)
        #    compatible_acc_list = list()
        #    #for j in range(i+1):
        #    compatible_acc_list.append('acc'+str(i))
        #    self.fpga_section_list[sec_id] = FpgaSection(sec_id, i, 'localhost', compatible_acc_list)
        #    #print 'compatible_acc_id = %r' %compatible_acc_list



    #def initiate_job_status(self):
    #    for i in range(4):
    #        job_id = i
    #        in_buf_size = 1.2 * 1024
    #        out_buf_size = 1.2 *1024
    #        acc_id = "acc"+str(i%4)
    #        arrival_time = 0
    #        ii = 1
    #        if i >= 4:
    #            arrival_time = 6
    #            ii = 2
    #        from_node = "192.168.3."+str(2*i+1)
    #        self.fpga_job_list[job_id] = FpgaJob(job_id,
    #                                                in_buf_size,
    #                                                 out_buf_size,
    #                                                 acc_id,
    #                                                 arrival_time,
    #                                                 from_node)

    def initiate_job_status(self):
        cursor = self.conn.cursor()
        try:
            query = "SELECT * FROM fpga_jobs"
            cursor.execute(query)
            job_list = cursor.fetchall()
            for job in job_list:
                job_id = job[0]
                #in_buf_size = float(job[1])*1024
                #out_buf_size = float(job[2])*1024
                in_buf_size = 1.2*1024
                out_buf_size = 1.2*1024
                acc_id = job[3]
                #arrival_time = 0
                #if int(job_id) >= 4:
                #    arrival_time = 6
                #arrival_time = int(job_id)%2
                arrival_time = float(job[4])
                from_node = job[5]
                self.fpga_job_list[job_id] = FpgaJob(job_id,
                                                     in_buf_size,
                                                     out_buf_size,
                                                     acc_id,
                                                     arrival_time,
                                                     from_node)
        except Error as e:
            print e

        finally:
            cursor.close()


    def initiate_event(self):
        #for job_id,job in self.fpga_job_list.items():
        for i in range(len(self.fpga_job_list)):
            job_id = sorted(self.fpga_job_list.iterkeys())[i]
            arrival_event_id = uuid.uuid4()
            self.fpga_job_list[job_id].set_job_arrival_event(arrival_event_id)
            #print 'initiate_event:job_id =%r' % job_id

            self.insert_event(job_id, EventType.JOB_ARRIVAL)





    def find_available_sections(self):
        available_section_list = dict()
        acc_id = self.fpga_job_list[self.current_job_id].job_acc_id
        for sec_id, section in self.fpga_section_list.items():
            if len(section.compatible_acc_list):
                    if acc_id in section.compatible_acc_list and section.if_idle == True:
                        available_section_list[sec_id] = section

        return available_section_list


    def find_waiting_jobs(self):
        return_job_list = dict()
        section_id = self.fpga_job_list[self.current_job_id].section_id
        compatible_acc_list = self.fpga_section_list[section_id].compatible_acc_list
        if len(self.waiting_job_list):
            for job in self.waiting_job_list:
                if job.job_acc_id in compatible_acc_list:
                    return_job_list[job.job_id] = job
        return return_job_list

    def get_idle_sections(self, node_ip):
        idle_sec_list = list()
        #node_ip = self.fpga_section_list[section_id].node_ip
        section_num = self.node_list[node_ip].section_num
        #print "section_num = %r" %section_num
        #print "len = %r" %len(self.node_list[node_ip].section_id_list)
        for sec_id in self.node_list[node_ip].section_id_list:
            #print sec_id
            if self.fpga_section_list[sec_id].if_idle == True:
                idle_sec_list.append(sec_id)

        return idle_sec_list

    def get_waiting_time(self, job_id):
        arrival_time = self.fpga_job_list[job_id].job_arrival_time
        return self.current_time - arrival_time


    def conduct_local_1_scheduling(self, incoming_list):
        job_acc_id = self.fpga_job_list[self.current_job_id].job_acc_id
        job_node_ip = self.fpga_job_list[self.current_job_id].node_ip

        if self.current_event_type == EventType.JOB_ARRIVAL:
            potential_sec_list = list()
            idle_sec_list = self.get_idle_sections(job_node_ip)
            for sec_id in idle_sec_list:
                if self.fpga_section_list[sec_id].current_acc_id == job_acc_id:
                    #print "job_id %r: sec_id %r" %(self.current_job_id, sec_id)
                    return sec_id
                else:
                    potential_sec_list.append(sec_id)

            if len(potential_sec_list):
                #print "return %r" %potential_sec_list[0]
                return potential_sec_list[0]
            else:
                return None
            #    print i
            #for sec_id, sec in incoming_list.items():
            #    if sec.node_ip == job_node_ip:
            #        if sec.current_acc_id == job_acc_id:
            #            return sec_id
            #        else:
            #            potential_sec_list.append(sec_id)
            #if len(potential_sec_list):
            #    #print "return %r" %potential_sec_list[0]
            #    return potential_sec_list[0]
            #else:
            #    return None


        elif self.current_event_type == EventType.JOB_COMPLETE:
            potential_job_list = list()
            for job_id, job in incoming_list.items():
                if job.node_ip == job_node_ip:
                    if job.job_acc_id == job_acc_id:
                        return job_id
                    else:
                        potential_job_list.append(job_id)
            if len(potential_job_list):
                return potential_job_list[0]
            else:
                return None






    def conduct_random_scheduling(self, incoming_list):
        id_index = random.randint(0,len(incoming_list)-1)
        id = incoming_list.keys()[id_index]

        if self.current_event_type == EventType.JOB_ARRIVAL:#incoming_list is a dict of sections
            return incoming_list[id].section_id

        else:#incoming_list is a dict of jobs

            id = incoming_list.keys()[0]
            return incoming_list[id].job_id

    def conduct_fifo_scheduling(self, incoming_list):
        if self.current_event_type == EventType.JOB_ARRIVAL:
            pass
        elif self.current_event_type == EventType.JOB_FINISH:
            pass
        else:
            print "Unknown event type"
            return None

    def conduct_test_3_scheduling(self, incoming_list):
        pass




    def conduct_test_2_scheduling(self, incoming_list):
        level_0_section_list = list()
        level_1_section_list = list()
        level_2_section_list = list()
        level_3_section_list = list()
        if self.current_event_type == EventType.JOB_ARRIVAL:
            from_node_ip = self.fpga_job_list[self.current_job_id].node_ip
            job_acc_id = self.fpga_job_list[self.current_job_id].job_acc_id
            for section_id, section in incoming_list.items():
                priority_level = self.get_priority_level(self.current_job_id, section_id)#return locality, acc
                if priority_level[0] == 2:
                    if priority_level[1] == 1:
                        level_3_section_list.append(section)
                    else:
                        level_1_section_list.append(section)
                elif priority_level[0] == 1:
                    if priority_level[1] == 1:
                        level_2_section_list.append(section)
                    else:
                        level_1_section_list.append(section)
                else:
                    level_0_section_list.append(section)

            if len(level_3_section_list):
                id_index = 0
                return level_3_section_list[id_index].section_id

            if len(level_2_section_list):
                id_index = 0
                return level_2_section_list[id_index].section_id

            elif len(level_1_section_list):
                id_index = 0
                return level_1_section_list[id_index].section_id

            else:
                id_index = 0
                return level_0_section_list[id_index].section_id

        elif self.current_event_type == EventType.JOB_COMPLETE:
            job_acc_id = self.fpga_job_list[self.current_job_id].job_acc_id
            section_id = self.fpga_job_list[self.current_job_id].section_id
            #section_node_ip = self.fpga_section_list[section_id].node_ip
            for job_id, job in incoming_list.items():
                priority_level = self.get_priority_level(job_id, section_id)
                if priority_level[0] == 2:
                    if priority_level[1] == 1:
                        level_3_section_list.append(job)
                    else:
                        level_1_section_list.append(job)
                elif priority_level[0] == 1:
                    if priority_level[1] == 1:
                        level_2_section_list.append(job)
                    else:
                        level_1_section_list.append(job)
                else:
                    level_0_section_list.append(job)

            if len(level_3_section_list):
                id_index = 0
                return level_3_section_list[id_index].job_id

            if len(level_2_section_list):
                id_index = 0
                return level_2_section_list[id_index].job_id

            elif len(level_1_section_list):
                id_index = 0
                return level_1_section_list[id_index].job_id

            else:
                id_index = 0
                return level_0_section_list[id_index].job_id





    def conduct_test_1_scheduling(self, incoming_list):
        level_0_section_list = list()
        level_1_section_list = list()
        level_2_section_list = list()

        if self.current_event_type == EventType.JOB_ARRIVAL:
            from_node_ip = self.fpga_job_list[self.current_job_id].node_ip
            job_acc_id = self.fpga_job_list[self.current_job_id].job_acc_id
            for section_id, section in incoming_list.items():
                priority_level = self.get_priority_level(self.current_job_id, section_id)#return locality, acc
                if priority_level[0] >= 1 :
                    if priority_level[1] == 1:
                        level_2_section_list.append(section)
                    else:
                        level_1_section_list.append(section)
                else:
                    level_0_section_list.append(section)


            if len(level_2_section_list):
                #id_index = random.randint(0, len(level_2_section_list)-1)
                id_index = 0
                return level_2_section_list[id_index].section_id

            elif len(level_1_section_list):
                #id_index = random.randint(0, len(level_1_section_list)-1)
                id_index = 0
                return level_1_section_list[id_index].section_id

            else:
                #id_index = random.randint(0, len(level_0_section_list)-1)
                id_index = 0
                return level_0_section_list[id_index].section_id

        elif self.current_event_type == EventType.JOB_COMPLETE:
            job_acc_id = self.fpga_job_list[self.current_job_id].job_acc_id
            section_id = self.fpga_job_list[self.current_job_id].section_id
            #section_node_ip = self.fpga_section_list[section_id].node_ip
            for job_id, job in incoming_list.items():
                priority_level = self.get_priority_level(job_id, section_id)
                if priority_level[0] == 1:
                    if priority_level[1] == 1:
                        level_2_section_list.append(job)
                    else:
                        level_1_section_list.append(job)
                else:
                    level_0_section_list.append(job)


            if len(level_2_section_list):
                #id_index = random.randint(0, len(level_2_section_list)-1)
                id_index = 0
                return level_2_section_list[id_index].job_id

            elif len(level_1_section_list):
                #id_index = random.randint(0, len(level_1_section_list)-1)
                id_index = 0
                return level_1_section_list[id_index].job_id

            #elif len(level_0_section_list):
            else:
                #id_index = random.randint(0, len(level_0_section_list)-1)
                id_index = 0
                return level_0_section_list[id_index].job_id


    def conduct_fair_sharing_scheduling(self, incoming_list):
        level_0_section_list = list()
        level_1_section_list = list()
        level_2_section_list = list()

        if self.current_event_type == EventType.JOB_ARRIVAL:
            from_node_ip = self.fpga_job_list[self.current_job_id].node_ip
            job_acc_id = self.fpga_job_list[self.current_job_id].job_acc_id
            for section_id, section in incoming_list.items():
                if section.node_ip == from_node_ip:
                    if section.current_acc_id == job_acc_id:# if this section is on the same node and acc_compatible
                        level_2_section_list.append(section)
                    else:# if this sectino is on the same node
                        level_1_section_list.append(section)

                elif section.current_acc_id == job_acc_id:# from different nodes, but same acc_type
                    level_0_section_list.append(section)


            if len(level_2_section_list):
                id_index = random.randint(0, len(level_2_section_list)-1)
                return level_2_section_list[id_index].section_id

            elif len(level_1_section_list):
                id_index = random.randint(0, len(level_1_section_list)-1)
                return level_1_section_list[id_index].section_id

            elif len(level_0_section_list):
                id_index = random.randint(0, len(level_0_section_list)-1)
                return level_0_section_list[id_index].section_id

            else:
                id_index = random.randint(0, len(incoming_list)-1)
                i = 0
                id = 0
                for c_id, c_object in incoming_list.items():
                    if id_index == i:
                        id = c_id
                        break
                    i += 1

                return incoming_list[id].section_id

        elif self.current_event_type == EventType.JOB_COMPLETE:
            job_acc_id = self.fpga_job_list[self.current_job_id].job_acc_id
            section_id = self.fpga_job_list[self.current_job_id].section_id
            section_node_ip = self.fpga_section_list[section_id].node_ip
            for job_id, job in incoming_list.items():
                if section_node_ip == job.node_ip:
                    if job_acc_id == job.job_acc_id:
                        level_2_section_list.append(job)
                    else:
                        level_1_section_list.append(job)
                elif job_acc_id == job.job_acc_id:
                    level_0_section_list.append(job)

            if len(level_2_section_list):
                id_index = random.randint(0, len(level_2_section_list)-1)
                return level_2_section_list[id_index].job_id

            elif len(level_1_section_list):
                id_index = random.randint(0, len(level_1_section_list)-1)
                return level_1_section_list[id_index].job_id

            elif len(level_0_section_list):
                id_index = random.randint(0, len(level_0_section_list)-1)
                return level_0_section_list[id_index].job_id

            else:
                id_index = random.randint(0, len(incoming_list)-1)
                i = 0
                id = 0
                for c_id, c_object in incoming_list.items():
                    if id_index == i:
                        id = c_id
                        break
                    i += 1
                return incoming_list[id].job_id


    def get_priority_level(self, job_id, section_id):
        locality_level = 0
        acc_level = 0
        return_dict = dict()


        job_node_ip = self.fpga_job_list[job_id].node_ip
        job_acc_id = self.fpga_job_list[job_id].job_acc_id

        section_node_ip = self.fpga_section_list[section_id].node_ip
        section_acc_id = self.fpga_section_list[section_id].current_acc_id

        if_fpga_available = self.node_list[job_node_ip].if_fpga_available

        if job_node_ip == section_node_ip:
            locality = 2

        elif if_fpga_available == 0:
            locality = 1

        else:
            locality = 0

        if job_acc_id == section_acc_id:
            acc_level = 1
        else:
            acc_level = 0

        return_dict[0] = locality
        return_dict[1] = acc_level

        return return_dict

    def execute_scheduling(self):
        return_id = None
        available_section_list = dict()
        potential_job_list = dict()

        if self.current_event_type == EventType.JOB_ARRIVAL:
            #print 'event type is job_arrival'
            available_section_list = self.find_available_sections()#return a dict of sections
            if len(available_section_list) == 0:
                return return_id

        elif self.current_event_type == EventType.JOB_COMPLETE:
            potential_job_list = self.find_waiting_jobs()#return a dict of jobs
            if len(potential_job_list) == 0:
                return return_id


        if self.current_event_type == EventType.JOB_ARRIVAL:
            incoming_list = available_section_list
        else:
            incoming_list = potential_job_list

        if self.scheduling_algorithm == SchedulingAlgorithm.RANDOM:
            return_id = self.conduct_random_scheduling(incoming_list)

        elif self.scheduling_algorithm == SchedulingAlgorithm.FIFO:
            return_id = self.conduct_fifo_scheduling(incoming_list)

        elif self.scheduling_algorithm == SchedulingAlgorithm.FAIR_SHARING:
            return_id = self.conduct_fair_sharing_scheduling(incoming_list)

        elif self.scheduling_algorithm == SchedulingAlgorithm.TEST_1:
            return_id = self.conduct_test_1_scheduling(incoming_list)

        elif self.scheduling_algorithm == SchedulingAlgorithm.TEST_2:
            return_id = self.conduct_test_2_scheduling(incoming_list)

        elif self.scheduling_algorithm == SchedulingAlgorithm.TEST_3:
            return_id = self.conduct_test_3_scheduling(incoming_list)

        elif self.scheduling_algorithm == SchedulingAlgorithm.LOCAL_1:
            return_id = self.conduct_local_1_scheduling(incoming_list)

        return return_id


    def find_associate_sections(self,node_ip):
        section_list = list()
        for section_id, section in self.fpga_section_list.items():
            if section.node_ip == node_ip and section.if_idle == False:
                job_id = section.current_job_id
                if self.fpga_job_list[job_id].job_start_time <= self.current_time < self.fpga_job_list[job_id].job_finish_time:#job has already begun
                    #print 'job_id = %r, start_time = %r' %(job_id, self.fpga_job_list[job_id].job_start_time)
                    section_list.append(section_id)
        return section_list# return a list of section_id

    def update_acc_real_bw(self, pcie_bw, acc_peak_bw_list):
        upper_list = dict()
        lower_list = dict()


        n = len(acc_peak_bw_list)
        #print 'n = %r' %n
        if n == 0:
            return lower_list
        else:
            e_default_bw = 0
            c_default_bw = 0
            while True:
                total_lower_bw = 0
                if len(lower_list.items()):
                    for id, acc_bw in lower_list.items():
                        total_lower_bw += acc_bw

                e_default_bw = c_default_bw
                #print 'now n= %r' %n
                c_default_bw = (pcie_bw - total_lower_bw)/(n - len(lower_list))
                for sec_id, acc_bw in acc_peak_bw_list.items():
                    if acc_bw <= c_default_bw:
                        lower_list[sec_id] = acc_bw
                    else:
                        upper_list[sec_id] = acc_bw
                if e_default_bw == c_default_bw or len(lower_list) == n:
                    break
            if len(upper_list):
                for sec_id,acc_bw in upper_list.items():
                    lower_list[sec_id] = c_default_bw

            return lower_list# return a dict of tuple with one element  sec_id: real_acc_bw


    def update_job_finish_time(self,associate_job_list):# associate_job_list is a dict <job_id:real_bw, earlier_bw>
        return_job_id_list = dict()
        #print 'length of associate_job_list:%r' %associate_job_list
        for job_id, bw in associate_job_list.items():
            e_finish_time = self.fpga_job_list[job_id].job_finish_time
            section_id = self.fpga_job_list[job_id].section_id
            #print 'job_id =%r, running on %r, current_acc_bw=%r' %(job_id, section_id, bw[0])
            #print 'type finish_time %r, time current_time %r' %(e_finish_time,self.current_time)
            c_finish_time = ((e_finish_time - self.current_time)*bw[1])/bw[0] + self.current_time

            self.fpga_job_list[job_id].job_finish_time = c_finish_time# update job_finish_time
            c_event_id = self.fpga_job_list[job_id].job_associate_event_list[2][0]
            self.fpga_job_list[job_id].job_associate_event_list[2] = (c_event_id, c_finish_time)# update (event_id, finish_time)

            return_job_id_list[job_id] = e_finish_time

        return return_job_id_list #return a dict of tuples, tuples=(job_id, old_finish_time)

    def update_associate_events(self, job_id_list, event_type):
        if len(job_id_list) == 0:
            #print "len is 0"
            return
        if event_type == EventType.JOB_FINISH:
            for job_id,event_time in job_id_list.items():
                self.remove_event(job_id, EventType.JOB_FINISH, event_time)
                self.insert_event(job_id, EventType.JOB_FINISH)
        elif event_type == EventType.JOB_COMPLETE:
            for job_id,event_time in job_id_list.items():
                self.remove_event(job_id, EventType.JOB_COMPLETE, event_time)
                self.insert_event(job_id, EventType.JOB_COMPLETE)
        else:
            print "error"


    def update_complete_status(self,section_id, job_id_list):
        return_list = dict()
        for job_id in job_id_list:
            old_job_complete_time = self.fpga_job_list[job_id].job_complete_time
            new_job_finish_time = self.fpga_job_list[job_id].job_finish_time

            self.fpga_job_list[job_id].job_complete_time = new_job_finish_time + self.fpga_job_list[job_id].job_result_transfer_time
            c_job_complete_time = self.fpga_job_list[job_id].job_complete_time
            return_list[job_id] = old_job_complete_time # return a dict of <job_id: old_job_complete_time>
            c_event_id = self.fpga_job_list[job_id].job_associate_event_list[3][0]
            self.fpga_job_list[job_id].job_associate_event_list[3] = (c_event_id, c_job_complete_time)# update (event_id, finish_time)

        return return_list

    def update_finish_status(self, section_id):#update sections, job_finish_times, event_lists/sequences
        node_ip = self.fpga_section_list[section_id].node_ip
        job_id = self.fpga_section_list[section_id].current_job_id
        total_pcie_bw = self.node_list[node_ip].pcie_bw
        associate_section_list = list()
        associate_section_list = self.find_associate_sections(node_ip)#return a list of section_id
        #print 'associate section list length is %r' %len(associate_section_list)



        associate_acc_real_bw_list = dict()
        associate_acc_peak_bw_list = dict()
        associate_job_list = dict()
        new_event_time_list = dict()

        earlier_acc_real_bw_list = dict()


        if len(associate_section_list) == 0:
            #print 'associate_section_list is empty'
            return new_event_time_list #no associate sections
        else:
            for sec_id in associate_section_list:
                current_section = self.fpga_section_list[sec_id]
                current_acc_id = current_section.current_acc_id
                current_acc_peak_bw = self.acc_type_list[current_acc_id].acc_peak_bw
                associate_acc_peak_bw_list[sec_id] = current_acc_peak_bw# a dict of tuple  sec_id: peak_bw
                earlier_acc_real_bw_list[sec_id] = current_section.current_acc_bw

            acc_real_bw_list = self.update_acc_real_bw(total_pcie_bw,associate_acc_peak_bw_list)# return a dict of sec_id: real_bw

            for sec_id, real_bw in acc_real_bw_list.items():
                self.fpga_section_list[sec_id].current_acc_bw = real_bw#update fpga_section_list
                job_id = self.fpga_section_list[sec_id].current_job_id

                associate_job_list[job_id] = (real_bw, earlier_acc_real_bw_list[sec_id])# return a dict of tuple, tuple = (real_bw,earlier_bw)
                #next, update fpga_job_list

            old_finish_time_list = self.update_job_finish_time(associate_job_list)#return a dict of job_id: new_job_finish_time

            return old_finish_time_list #return a dict of tuples, tuples=(job_id, old_finish_time)



    def get_job_metrics(self, job_id):
        in_buf_size = self.fpga_job_list[job_id].job_in_buf_size
        out_buf_size = self.fpga_job_list[job_id].job_out_buf_size
        #section_id = self.fpga_job_list[job_id].section_id

        acc_id = self.fpga_job_list[job_id].job_acc_id
        acc_peak_bw = self.acc_type_list[acc_id].acc_peak_bw
        configuration_time = self.acc_type_list[acc_id].acc_configuration_time

        job_node_ip = self.fpga_job_list[job_id].node_ip

        if self.node_list[job_node_ip].if_fpga_available == False:
            ideal_total_time = configuration_time + in_buf_size/acc_peak_bw + self.fpga_job_list[job_id].job_result_transfer_time + self.fpga_job_list[job_id].job_source_transfer_time
            self.network_ideal_workload += (in_buf_size + out_buf_size)/1024
        else:
            ideal_total_time = configuration_time + in_buf_size/acc_peak_bw

        if self.fpga_job_list[job_id].job_if_local == False:
            self.network_real_workload += (in_buf_size + out_buf_size)/1024

        real_total_time = self.fpga_job_list[job_id].job_complete_time - self.fpga_job_list[job_id].job_arrival_time
        anp = ideal_total_time / real_total_time
        self.fpga_job_list[job_id].job_anp = anp
        self.fpga_job_list[job_id].job_ideal_time = ideal_total_time
        self.fpga_job_list[job_id].job_real_time = real_total_time

        self.snp += self.fpga_job_list[job_id].job_anp
        self.make_span = self.larger(self.fpga_job_list[job_id].job_finish_time,self.make_span)
        self.ssp += self.fpga_job_list[job_id].job_real_time
        self.reconfiguration_num += self.fpga_job_list[job_id].if_configured
        #return (ideal_total_time, real_total_time, anp)


    def insert_to_waiting_job_list(self):
        self.waiting_job_list.append(self.fpga_job_list[self.current_job_id])

    def handle_job_arrival(self):
        job_id = self.current_job_id
        section_id = self.execute_scheduling()
        #in_buf_size = self.fpga_job_list[job_id].job_in_buf_size
        if section_id == None:#no sections available, insert current job into waiting_list
            #print 'no sections available, adding this job to the waiting list'
            self.insert_to_waiting_job_list()

        else:
            self.start_new_job(job_id, section_id)
            #job_id_list = self.update_associate_status(section_id)#return a dict of job_id: new_finish_time
            #self.update_associate_events(job_id_list)

        self.remove_current_event()


    def start_new_job(self,job_id,section_id):

        job_acc_id = self.fpga_job_list[job_id].job_acc_id
        section_acc_id = self.fpga_section_list[section_id].current_acc_id

        self.fpga_job_list[job_id].section_id = section_id
        job_acc_bw = self.acc_type_list[job_acc_id].acc_peak_bw


        in_buf_size = self.fpga_job_list[job_id].job_in_buf_size
        out_buf_size = self.fpga_job_list[job_id].job_out_buf_size

        job_node_ip = self.fpga_job_list[job_id].node_ip
        section_node_ip = self.fpga_section_list[section_id].node_ip

        if job_acc_id != section_acc_id:
            #print'job_acc_id %r section_acc_id %r' %(job_acc_id,section_acc_id)
            self.fpga_job_list[job_id].job_configuration_time = self.acc_type_list[job_acc_id].acc_configuration_time
            self.fpga_job_list[job_id].if_configured = 1
            #print 'configuration time = %r' %self.fpga_job_list[job_id].job_configuration_time

        if job_node_ip != section_node_ip:
            roce_bw = self.node_list[job_node_ip].roce_bw# bw is in Megabytes
            roce_latency = self.node_list[job_node_ip].roce_latency

            self.fpga_job_list[job_id].job_if_local = False
            self.fpga_job_list[job_id].job_source_transfer_time = in_buf_size/roce_bw + roce_latency
            self.fpga_job_list[job_id].job_result_transfer_time = out_buf_size/roce_bw + roce_latency


        self.fpga_section_list[section_id].current_acc_id = job_acc_id
        self.fpga_section_list[section_id].current_job_id = job_id
        self.fpga_section_list[section_id].current_acc_bw = job_acc_bw
        self.fpga_section_list[section_id].if_idle = False


        #print type(self.fpga_job_list[job_id].job_start_time),type(self.current_time), type(self.fpga_job_list[job_id].job_configuration_time )
        self.fpga_job_list[job_id].job_start_time = self.current_time + self.fpga_job_list[job_id].job_configuration_time + self.fpga_job_list[job_id].job_source_transfer_time

        job_start_event_id = uuid.uuid4()
        job_start_time = self.fpga_job_list[job_id].job_start_time
        self.fpga_job_list[job_id].job_associate_event_list[1] = (job_start_event_id, job_start_time)


        job_finish_event_id = uuid.uuid4()
        job_finish_time = job_start_time + in_buf_size/job_acc_bw

        self.fpga_job_list[job_id].job_finish_time = job_finish_time
        self.fpga_job_list[job_id].job_associate_event_list[2] = (job_finish_event_id, job_finish_time)

        job_complete_event_id = uuid.uuid4()
        job_complete_time = job_finish_time + self.fpga_job_list[job_id].job_result_transfer_time

        self.fpga_job_list[job_id].job_complete_time = job_complete_time
        self.fpga_job_list[job_id].job_associate_event_list[3] = (job_complete_event_id, job_complete_time)

        self.insert_event(job_id, EventType.JOB_START)
        self.insert_event(job_id, EventType.JOB_FINISH)
        self.insert_event(job_id, EventType.JOB_COMPLETE)

        i = 0
        for job in self.waiting_job_list:
            if job.job_id == job_id:
                del self.waiting_job_list[i]
                break
            i += 1


    def handle_job_start(self, job_id):

        section_id = self.fpga_job_list[job_id].section_id

        old_finish_time_list = self.update_finish_status(section_id)#return a dict of job_id: old_finish_time
        self.update_associate_events(old_finish_time_list, EventType.JOB_FINISH)
        job_id_list = list()

        for job_id in old_finish_time_list:
            job_id_list.append(job_id)
        old_complete_time_list = self.update_complete_status(section_id, job_id_list)
        self.update_associate_events(old_complete_time_list, EventType.JOB_COMPLETE)

        self.remove_current_event()


    def handle_job_finish(self, job_id):

        section_id = self.fpga_job_list[self.current_job_id].section_id
        #self.fpga_section_list[section_id].if_idle = True
        earlier_acc_id = self.fpga_section_list[section_id].current_acc_id
        old_finish_time_list = self.update_finish_status(section_id)#return a dict of job_id: old_finish_time
        self.update_associate_events(old_finish_time_list, EventType.JOB_FINISH)

        job_id_list = list()
        for c_job_id in old_finish_time_list:
            #if job_id != c_job_id:
            job_id_list.append(c_job_id)

        old_complete_time_list = self.update_complete_status(section_id, job_id_list)
        self.update_associate_events(old_complete_time_list, EventType.JOB_COMPLETE)

        self.remove_current_event()



    def handle_job_complete(self, job_id):

        section_id = self.fpga_job_list[job_id].section_id
        self.fpga_section_list[section_id].if_idle = True
        if len(self.waiting_job_list):
            new_job_id = self.execute_scheduling()

            if new_job_id !=None:
                #print 'new_job_id=%r' % new_job_id
                self.start_new_job(new_job_id, section_id)
                #print 'new_job_id is not None'

        self.remove_current_event()


    def initiate_fpga_system(self):
        self.initiate_available_acc_type_list()
        self.initiate_node_status()
        self.initiate_fpga_resources()
        self.initiate_job_status()
        self.initiate_event()

    def simulation_input(self):
        print 'simulation is ready ...'
        self.initiate_fpga_system()
        print 'intial event number is %r' % (len(self.event_list))

    def larger(self, a, b):
        if a < b:
            return b
        else:
            return a
    def smaller(self, a,b):
        if a > b:
            return b
        else:
            return a

    def simulation_output(self):
        n = len(self.fpga_job_list)
        for job_id, job in self.fpga_job_list.items():
            self.get_job_metrics(job_id)

            #print '[job%r]  acc_id = %r, in_buf_size = %r, from_node %r...' %(job.job_id, job.job_acc_id, job.job_in_buf_size, job.node_ip)
            #print '[job%r]  section_id = %r, ARRIVAL_TIME = %r, START_TIME = %r, FINISH_TIME = %r COMPLETE_TIME = %r' %(job.job_id,
            #                                                                                                            job.section_id,
            #                                                                                                            job.job_arrival_time,
            #                                                                                                            job.job_start_time,
            #                                                                                                            job.job_finish_time,
            #                                                                                                            job.job_complete_time)
            #print '[job%r]  ideal_total_time = %r, real_total_time = %r, ANP = %r' %(job.job_id, job.job_ideal_time, job.job_real_time, job.job_anp)
            #print ''

        #self.snp = self.snp**(1.0/n)
        self.snp /= n
        self.make_span -= self.fpga_job_list[0].job_arrival_time
        self.ssp /= n
        #self.reconfiguration_num /= n
        print '[%r]  SNP = %.5f, Makespan = %.5f SSP = %.5f FR = %.5f' % (self.scheduling_algorithm, self.snp, self.make_span, self.ssp, self.reconfiguration_num)
        if self.network_real_workload != 0:
            print '[%r]  Network_Transfer = %.5f GB, Necessary_Network_Transfer= %.5f GB, efficiency= %.5f ' %(self.scheduling_algorithm, self.network_real_workload, self.network_ideal_workload, self.network_ideal_workload/self.network_real_workload)
        else:
            print '[%r]  Network_Transfer = %.5f GB, Necessary_Network_Transfer= %.5f GB '%(self.scheduling_algorithm, self.network_real_workload, self.network_ideal_workload)
        print ''



    def simulation_start(self):
        print 'simulation starts...'
        i = 0
        while len(self.event_sequence):
            i += 1
            self.current_time = sorted(self.event_sequence.iterkeys())[0]
            self.current_event_id = self.event_sequence[self.current_time][0]#since multiple events may happen at a certain time, here we use dicts of lists to represent multimaps in C++
            self.current_job_id = self.event_list[self.current_event_id].job_id

            #print 'current time is %r' %self.current_time
            #print 'job_id =%r' %self.current_job_id
            #print 'current_event_id = %r' %self.current_event_id
            self.current_event_type = self.event_list[self.current_event_id].event_type#get the earliest-happened event from event_sequence
            #print 'current_event_type = %r' %self.current_event_type
            #print 'current_time = %r, event_id = %r, event_type =%r' %(self.current_time, self.current_event_id, self.current_event_type)


            if self.current_event_type == EventType.JOB_ARRIVAL:
                self.handle_job_arrival()
            elif self.current_event_type == EventType.JOB_START:
                self.handle_job_start(self.current_job_id)
            elif self.current_event_type == EventType.JOB_FINISH:
                self.handle_job_finish(self.current_job_id)
            elif self.current_event_type == EventType.JOB_COMPLETE:
                self.handle_job_complete(self.current_job_id)
            else:
                pass
            #if i > 20:
             #   break



if __name__ == "__main__":
    if len(sys.argv) < 2:
        sys.exit('Usage: sys.argv[0] random/fair_sharing/test1/test2/test3/local1')
        sys.exit(1)

    if sys.argv[1] == "random":
        scheduling_algorithm = SchedulingAlgorithm.RANDOM

    elif sys.argv[1] == "fair_sharing":
        scheduling_algorithm = SchedulingAlgorithm.FAIR_SHARING

    elif sys.argv[1] == "test1":
        scheduling_algorithm = SchedulingAlgorithm.TEST_1

    elif sys.argv[1] == "test2":
        scheduling_algorithm = SchedulingAlgorithm.TEST_2

    elif sys.argv[1] == "test3":
        scheduling_algorithm = SchedulingAlgorithm.TEST_3

    elif sys.argv[1] == "local1":
        scheduling_algorithm = SchedulingAlgorithm.LOCAL_1

    else:
        sys.exit('Usage: sys.argv[0] random/fair_sharing/test2/test3')
        sys.exit(1)

    my_fpga_simulator = FpgaSimulator(scheduling_algorithm)
    my_fpga_simulator.simulation_input()
    my_fpga_simulator.simulation_start()
    my_fpga_simulator.simulation_output()
