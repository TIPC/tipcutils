#
# Makefile for TIPC connection demo 
# 

ifdef INCLUDE_PATH
# Specifies path to directory that resolves #include <linux/tipc.h>
IFLAGS = -I${INCLUDE_PATH}
endif

OFLAGS = -Wall -O2
CFLAGS = ${IFLAGS} ${DFLAGS} ${OFLAGS} ${EXTRAS}

CC = gcc

all: client_tipc server_tipc 

clean:
	rm -f ./*.o client_tipc server_tipc

client_tipc: client_tipc.o ${LOBJS}
	${CC} ${CFLAGS} $< ${LOBJS} -o $@

server_tipc: server_tipc.o
	${CC} ${CFLAGS} $< ${LOBJS} -o $@

