.PHONY: clean

APPS    := acc_servicelayer.so test_bench demo.so

CLIENT_CFLAGS := -I/usr/include -I./xmlrpc
CLIENT_LIBS := -L/usr/lib/x86_64-linux-gnu -L./xmlrpc
CFLAGS  := -Wall -g
LOCAL_LIBS := -L. -L./fpga-sim/driver

RPC_LIBS := -lxmlrpc_client -lxmlrpc -lxmlrpc_util
RDMA_LIBS := -lrdmacm -libverbs -lpthread

acc_servicelayer.so: acc_servicelayer.c client.c fpga-sim/driver/fpga-libacc.c 
	$(CC) $(CFLAGS) -c -fpic acc_servicelayer.c client.c fpga-sim/driver/fpga-libacc.c $(CLIENT_CFLAGS) $(CLIENT_LIBS)
	$(CC) -shared acc_servicelayer.o client.o fpga-libacc.o -o libacc_servicelayer.so $(RPC_LIBS) $(RDMA_LIBS) $(CLIENT_LIBS) 

test_bench: test_bench.c
	$(CC) $(CFLAGS) -o test_bench test_bench.c -lacc_servicelayer $(LOCAL_LIBS) 
demo.so : demo.c server.c
	python setup.py build 
	cp build/lib.linux-x86_64-2.7/demo.so .

all: ${APPS}

clean:
	rm -f ${APPS}
	rm -rf *.o
	rm -rf *.so
	rm -rf build
