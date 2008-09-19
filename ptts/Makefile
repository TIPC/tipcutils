#
# Makefile for portable TIPC test suite 
# 

ifdef INCLUDE_PATH
# Specifies path to directory that resolves #include <linux/tipc.h>
IFLAGS = -I${INCLUDE_PATH}
endif

OFLAGS = -Wall -O2
CFLAGS = ${IFLAGS} ${DFLAGS} ${OFLAGS} ${EXTRAS}

CC = gcc

.PHONY: all clean

all: tipcTC tipcTS

tipcTC: tipc_ts_client_linux.o ${LOBJS}
	${CC} ${CFLAGS} $< ${LOBJS} -o $@

tipcTS: tipc_ts_server_linux.o ${LOBJS}
	${CC} ${CFLAGS} $< ${LOBJS} -o $@

clean:
	${RM} ./*.o tipcTC tipcTS


