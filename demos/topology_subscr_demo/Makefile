#
# Makefile for TIPC hello world demo 
# 

ifdef INCLUDE_PATH
# Specifies path to directory that resolves #include <linux/tipc.h>
IFLAGS = -I${INCLUDE_PATH}
endif

OFLAGS = -Wall -O2
CFLAGS = ${IFLAGS} ${DFLAGS} ${OFLAGS} ${EXTRAS}

CC = gcc

all: subscriptions server_tipc 

clean:
	rm -f ./*.o subscriptions server_tipc

subscriptions: subscriptions.o ${LOBJS}
	${CC} ${CFLAGS} $< ${LOBJS} -o $@

server_tipc: server_tipc.o
	${CC} ${CFLAGS} $< ${LOBJS} -o $@

