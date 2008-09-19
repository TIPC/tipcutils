#
# Makefile for tipc-config
#

ifdef INCLUDE_PATH
# Specifies path to directory that resolves #include <linux/tipc.h>
IFLAGS = -I${INCLUDE_PATH}
endif

OFLAGS = -Wall -O2
DFLAGS = -D VERSION=\"${VERSION}\"
CFLAGS = ${IFLAGS} ${DFLAGS} ${OFLAGS} ${EXTRAS}

all: tipc-config

tipc-config: tipc-config.o

clean:
	${RM} tipc-config.o tipc-config

