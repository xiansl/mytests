.PHONY: clean

CFLAGS  := -Wall -g -pg
LD      := gcc
LDFLAGS := ${LDFLAGS} -lrdmacm -lpthread -libverbs -pg

APPS    := server client client1

all: ${APPS}

client1: client1.o
	    ${LD} -o $@ $^ ${LDFLAGS}

client: client.o
	    ${LD} -o $@ $^ ${LDFLAGS}

server: server.o
	    ${LD} -o $@ $^ ${LDFLAGS}



clean:
	rm -f ${APPS}
	rm -f *.o

