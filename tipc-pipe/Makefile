ifdef INCLUDE_PATH
IFLAGS = -I${INCLUDE_PATH}
endif

OFLAGS = -Wall -O2
CFLAGS = ${IFLAGS} ${OFLAGS} ${EXTRAS}

all: tipc-pipe

tipc-pipe: tipc-pipe.o

clean:
	rm -rf tipc-pipe.o tipc-pipe
