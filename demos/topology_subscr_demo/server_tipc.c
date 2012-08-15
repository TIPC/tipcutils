/* ------------------------------------------------------------------------
 *
 * server_tipc.c
 *
 * Short description: TIPC subscription demo (server side)
 *
 * ------------------------------------------------------------------------
 *
 * Copyright (c) 2003, Ericsson Research Canada
 * Copyright (c) 2010 Wind River Systems
 * All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * Neither the name of Ericsson Research Canada nor the names of its
 * contributors may be used to endorse or promote products derived from this
 * software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * ------------------------------------------------------------------------
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <linux/tipc.h>

#define SERVER_TYPE  18888
#define SERVER_INST_LOWER  6
#define SERVER_INST_UPPER  53

int main(int argc, char *argv[], char *dummy[])
{
	struct sockaddr_tipc server_addr;
	int sd;

	server_addr.family = AF_TIPC;
	server_addr.addrtype = TIPC_ADDR_NAMESEQ;
	server_addr.addr.nameseq.type = SERVER_TYPE;
	server_addr.addr.nameseq.lower = SERVER_INST_LOWER;
	server_addr.addr.nameseq.upper = SERVER_INST_UPPER;
	server_addr.scope = TIPC_ZONE_SCOPE;

	/* Make server available */

	sd = socket(AF_TIPC, SOCK_RDM, 0);

	if (0 != bind(sd, (struct sockaddr *)&server_addr, sizeof(server_addr))) {
		printf("Server: failed to bind port name\n");
		exit(1);
	}
	printf("Server: bound port A to {%u,%u,%u} scope %u\n",
	       server_addr.addr.nameseq.type, server_addr.addr.nameseq.lower,
	       server_addr.addr.nameseq.upper, server_addr.scope);

	/* Bind name a second time, to get a higher share of the calls */

	if (0 != bind(sd, (struct sockaddr *)&server_addr, sizeof(server_addr))) {
		printf("Server: failed to bind port name\n");
		exit(1);
	}
	printf("Server: bound port A to name sequence {%u,%u,%u} scope %u\n",
	       server_addr.addr.nameseq.type, server_addr.addr.nameseq.lower,
	       server_addr.addr.nameseq.upper, server_addr.scope);

	/* Bind a third time, with a different name sequence */

	server_addr.addr.nameseq.lower = SERVER_INST_UPPER+1;
	server_addr.addr.nameseq.upper = SERVER_INST_UPPER+2;

	if (0 != bind(sd, (struct sockaddr *)&server_addr, sizeof(server_addr))) {
		printf("Server: failed to bind port name\n");
		exit(1);
	}
	printf("Server: bound port A to name sequence {%u,%u,%u} scope %u\n",
	       server_addr.addr.nameseq.type, server_addr.addr.nameseq.lower,
	       server_addr.addr.nameseq.upper, server_addr.scope);

	/* Bind a second port to the same sequence */

	sd = socket(AF_TIPC, SOCK_RDM,0);

	if (0 != bind(sd, (struct sockaddr *)&server_addr, sizeof(server_addr))) {
		printf("Server: failed to bind port name\n");
		exit(1);
	}
	printf("Server: bound port B to name sequence {%u,%u,%u} scope %u\n",
	       server_addr.addr.nameseq.type, server_addr.addr.nameseq.lower,
	       server_addr.addr.nameseq.upper, server_addr.scope);

	printf("\nServer: port names remain published until server is killed\n");

	/* Wait for user to kill server */

	while (1)
		sleep(1);

	exit(0);
}
