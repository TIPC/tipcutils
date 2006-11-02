#
# Makefile for tipcutils
#

ifndef KERNELDIR
	KERNELDIR = /usr/src/linux
endif

VERSION = 1.1.1
CFLAGS = -Wall -O2 -I${KERNELDIR}/include -D VERSION=\"${VERSION}\"

all: tipc-config

tipc-config: tipc-config.o

dist: clean
	git tar-tree HEAD tipcutils-$(VERSION) | gzip - > ../tipcutils-$(VERSION).tar.gz

clean:
	${RM} tipc-config.o tipc-config

# End of file
