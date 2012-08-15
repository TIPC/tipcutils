/* ------------------------------------------------------------------------
 *
 * server_tipc.c
 *
 * Short description: TIPC multicast demo (server side)
 *
 * ------------------------------------------------------------------------
 *
 * Copyright (c) 2003, Ericsson Research Canada
 * Copyright (c) 2005,2010 Wind River Systems
 * All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * Neither the name of the copyright holders nor the names of its
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
#include <sys/param.h>
#include <sys/poll.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <linux/tipc.h>

#define SERVER_TYPE  18888

void server_mcast(int lower, int upper)
{
	struct sockaddr_tipc server_addr;
	struct sockaddr_tipc client_addr;
	socklen_t alen = sizeof(struct sockaddr_tipc);
	int sd;
	char buf[100];

	server_addr.family = AF_TIPC;
	server_addr.addrtype = TIPC_ADDR_NAMESEQ;
	server_addr.scope = TIPC_CLUSTER_SCOPE;
	server_addr.addr.nameseq.type = SERVER_TYPE;
	server_addr.addr.nameseq.lower = lower;
	server_addr.addr.nameseq.upper = upper;

	sd = socket (AF_TIPC, SOCK_RDM, 0);

	if (bind(sd, (struct sockaddr *)&server_addr, sizeof(server_addr))) {
		printf("Server: port {%u,%u,%u} could not be created\n",
		       server_addr.addr.nameseq.type,
		       server_addr.addr.nameseq.lower,
		       server_addr.addr.nameseq.upper);
		exit (1);
	}
	printf("Server: port {%u,%u,%u} created\n",
	       server_addr.addr.nameseq.type,
	       server_addr.addr.nameseq.lower,
	       server_addr.addr.nameseq.upper);

	while (1) {
		if (0 > recvfrom(sd, buf, sizeof(buf), 0,
		                 (struct sockaddr *) &client_addr, &alen)) {
			perror("Server: port receive failed");
		}
		if (!buf[0]) {
			printf("Server: port {%u,%u,%u} terminated\n",
			       SERVER_TYPE, lower, upper);
			exit(0);
		}
		printf("Server: port {%u,%u,%u} received: %s\n",
		       SERVER_TYPE, lower, upper, buf);
	}
}

static void sig_child(int signo)
{
	/* Sync up with defunct child process to terminate zombie process */
	wait(NULL);
}

/**
 * Mainline for server side of multicast demo.
 *
 * Usage: server_tipc [lower [upper]]
 *
 * If no arguments supplied, creates a predetermined set of multicast servers.
 * If optional "lower" and "upper" arguments are specified, creates a single
 * server for the specified instance range; if "upper" is omitted, it defaults
 * to the same value as "lower".
 */

int main(int argc, char* argv[], char* envp[])
{
	int lower;
	int upper;
	char dummy;

	/* run standard demo if no arguments supplied by user */

	if (argc < 2) {
		if (signal(SIGCHLD, sig_child) == SIG_ERR) {
			printf("Server: can't catch child termination signals\n");
			perror(NULL);
			exit(1);
		}

		if (!fork())
			server_mcast(  0,  99);
		if (!fork())
			server_mcast(100, 199);
		if (!fork())
			server_mcast(200, 299);
		if (!fork())
			server_mcast(300, 399);
		exit(0);
	}

	/* create multicast server for instance range specified by user */

	if (sscanf(argv[1], "%u%c", &lower, &dummy) != 1) {
		perror("Server: invalid lower bound");
		exit(1);
	}
	if (argc > 2) {
		if (sscanf(argv[2], "%u%c", &upper, &dummy) != 1) {
			perror("Server: invalid upper bound");
			exit(1);
		}
	} else
		upper = lower;

	server_mcast(lower, upper);

	exit(0);
}
