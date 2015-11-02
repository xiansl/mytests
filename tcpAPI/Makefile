.PHONY: clean

APPS    := accServicelayer.so test_bench acc_monitor.so

CLIENT_CFLAGS := -I/usr/include
CLIENT_LIBS := -L/usr/lib/x86_64-linux-gnu
TCP_LIBS := -lpthread
CFLAGS  := -Wall -g

accServicelayer.so: acc_servicelayer.c tcp_transfer.c fpga-sim/driver/fpga-libacc.c 
	$(CC) $(CFLAGS) -c -fpic acc_servicelayer.c tcp_transfer.c fpga-sim/driver/fpga-libacc.c $(CLIENT_CFLAGS) $(CLIENT_LIBS) $(TCP_LIBS)
	$(CC) -shared acc_servicelayer.o tcp_transfer.o fpga-libacc.o -o libaccServicelayer.so  $(TCP_LIBS)
	sudo cp libaccServicelayer.so /lib64/
	sudo cp libaccServicelayer.so /lib/
	sudo cp libaccServicelayer.so /usr/local/lib/

test_bench: test_bench.c
	$(CC) $(CFLAGS) -o test_bench test_bench.c -laccServicelayer 

acc_monitor.so: acc_monitor.c 
	python setup.py build 
	cp build/lib.linux-x86_64-2.7/acc_monitor.so .

all: ${APPS}

clean:
	rm -f ${APPS}
	rm -rf *.o
	rm -rf *.so
	sudo rm -rf build
