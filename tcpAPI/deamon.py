#!/usr/bin/env python
# -*- coding:utf-8 -*-
#
#   Author  :   Zhuang Di ZHU
#   E-mail  :   zhuangdizhu@yahoo.com
#   Date    :   15/10/30 18:28:51
#   Desc    :   
#
import sys
import argparse
import uuid
import datetime
import time
import socket
import json #for serialization/de-serialization
import acc_monitor #for open a socket communicating with remote server
print sys.argv
backlog=5
size=1024


def run_deamon(port):
	s=socket.socket(socket.AF_INET, socket.SOCK_STREAM)
	s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
	s.bind(('', int(port)))
	s.listen(backlog)
	print 'Use Control-C to exit'
	try:
		while 1:
			scheduler,address=s.accept()#receive request from scheduler
			raw_data=scheduler.recv(size)
			scheduler_context=json.loads(raw_data)

			section_id = scheduler_context["section_id"]
			in_buf_size = scheduler_context["in_buf_size"]
			out_buf_size = scheduler_context["out_buf_size"]
			acc_name = scheduler_context["acc_name"]
			for i,j in scheduler_context.items():
				print i,j
			ret_context = acc_monitor.start_service(section_id, in_buf_size, out_buf_size, acc_name)
			for i,j in ret_context.items():
				print i,j

			send_data=json.dumps(ret_context)
			scheduler.send(send_data)
			scheduler.close()
	except KeyboardInterrupt:
		print 'Exiting'
		return

if __name__ == "__main__":
	if len(sys.argv) != 2:
		print "Usage: ", sys.argv[0], "<port>"
		print "Example: ./deamon.py 5000"
	else:
		print "Running deamon process...."
		run_deamon(sys.argv[1])


