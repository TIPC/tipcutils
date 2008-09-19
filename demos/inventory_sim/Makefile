#
# Makefile for TIPC inventory simulation demo 
# 

ifdef INCLUDE_PATH
# Specifies path to directory that resolves #include <linux/tipc.h>
IFLAGS = -I${INCLUDE_PATH}
endif

OFLAGS = -Wall -O2
CFLAGS = ${IFLAGS} ${DFLAGS} ${OFLAGS} ${EXTRAS}

CC = gcc

all: inventory_sim

clean:
	rm -f ./*.o inventory_sim ./*Sim ./new*

inventory_sim: inventory_sim.o ${LOBJS}
	${CC} ${CFLAGS} $< ${LOBJS} -o $@
	@ln -f -s inventory_sim newSim
	@ln -f -s inventory_sim killSim
	@ln -f -s inventory_sim stopSim
	@ln -f -s inventory_sim startSim
	@ln -f -s inventory_sim newItem
	@ln -f -s inventory_sim newCust
	@ln -f -s inventory_sim helpSim


