#!/usr/bin/env python
import xmlrpclib
from SimpleXMLRPCServer import SimpleXMLRPCServer
from xmlrpclib import Binary
import datetime
import uuid
import demo
import argparse
import sys
print sys.argv

def rpc_service(w,x,y,z):
    ret = demo.start_service(w,x,y,z)
    #print type(ret)
    #for i,j in ret.items():
    #    print i,j,type(j)
    return ret

def run_server(addr, port):
    server = SimpleXMLRPCServer((addr,int (port)), logRequests = True, allow_none = True)
    server.register_function(rpc_service)
    server.register_multicall_functions()
    server.register_introspection_functions()
    print 'Use Control-C to exit'
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print 'Exiting'
        server.server_close()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="This is usd to start a RPC server.")
    parser.add_argument("-a","--addr", help = "The interface ip_address to start RPC server", required = True)
    parser.add_argument("-p", "--port", help = "The port number to bind", required = True)
    args = parser.parse_args()
    run_server(args.addr, args.port)
