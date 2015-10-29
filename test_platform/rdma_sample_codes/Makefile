.PHONY: clean

CFLAGS  := -Wall -g
LD      := gcc
LDFLAGS := ${LDFLAGS} -lrdmacm -lpthread -libverbs

APPS    := server client

all: ${APPS}

client: client.o
	    ${LD} -o $@ $^ ${LDFLAGS}

server: server.o
	    ${LD} -o $@ $^ ${LDFLAGS}



clean:
	rm -f ${APPS}
	rm -f *.o

