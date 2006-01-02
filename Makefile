#
# Makefile for tipcutils
#

ifndef KERNELDIR
	KERNELDIR = /usr/src/linux
endif

CFLAGS = -I${KERNELDIR}/include

all: tipc-config

tipc-config: tipc-config.o

clean:
	${RM} tipc-config.o tipc-config

# End of file
