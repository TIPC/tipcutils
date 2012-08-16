/* ------------------------------------------------------------------------
 *
 * client_tipc.c
 *
 * Short description: TIPC multicast demo (client side)
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
#include <errno.h>
#include <unistd.h>
#include <netinet/in.h>
#include <linux/tipc.h>

#define SERVER_TYPE  18888
#define BUF_SIZE 100

void wait_for_server(__u32 name_type, __u32 name_instance, int wait)
{
	struct sockaddr_tipc topsrv;
	struct tipc_subscr subscr;
	struct tipc_event event;

	int sd = socket(AF_TIPC, SOCK_SEQPACKET, 0);

	memset(&topsrv, 0, sizeof(topsrv));
	topsrv.family = AF_TIPC;
	topsrv.addrtype = TIPC_ADDR_NAME;
	topsrv.addr.name.name.type = TIPC_TOP_SRV;
	topsrv.addr.name.name.instance = TIPC_TOP_SRV;

	/* Connect to topology server */

	if (0 > connect(sd, (struct sockaddr *)&topsrv, sizeof(topsrv))) {
		perror("Client: failed to connect to topology server");
		exit(1);
	}

	subscr.seq.type = htonl(name_type);
	subscr.seq.lower = htonl(name_instance);
	subscr.seq.upper = htonl(name_instance);
	subscr.timeout = htonl(wait);
	subscr.filter = htonl(TIPC_SUB_SERVICE);

	if (send(sd, &subscr, sizeof(subscr), 0) != sizeof(subscr)) {
		perror("Client: failed to send subscription");
		exit(1);
	}
	/* Now wait for the subscription to fire */

	if (recv(sd, &event, sizeof(event), 0) != sizeof(event)) {
		perror("Client: failed to receive event");
		exit(1);
	}
	if (event.event != htonl(TIPC_PUBLISHED)) {
		printf("Client: server {%u,%u} not published within %u [s]\n",
		       name_type, name_instance, wait/1000);
		exit(1);
	}

	close(sd);
}


void client_mcast(int sd, int lower, int upper)
{
	struct sockaddr_tipc server_addr;
	char buf[BUF_SIZE];

	server_addr.family = AF_TIPC;
	server_addr.addrtype = TIPC_ADDR_MCAST;
	server_addr.addr.nameseq.type = SERVER_TYPE;
	server_addr.addr.nameseq.lower = lower;
	server_addr.addr.nameseq.upper = upper;

	if (sd >= 0) {
		sprintf(buf, "message to {%u,%u,%u}",
		        server_addr.addr.nameseq.type,
		        server_addr.addr.nameseq.lower,
		        server_addr.addr.nameseq.upper);
		printf("Client: sending %s\n", buf);
	} else {
		sd = -sd;
		buf[0] = '\0';
		printf("Client: sending termination message to {%u,%u,%u}\n",
		       server_addr.addr.nameseq.type,
		       server_addr.addr.nameseq.lower,
		       server_addr.addr.nameseq.upper);
	}

	if (0 > sendto(sd, buf, strlen(buf) + 1, 0,
	                (struct sockaddr *)&server_addr, sizeof(server_addr))) {
		perror("Client: failed to send");
		exit(1);
	}
}

/**
 * Mainline for client side of multicast demo.
 *
 * Usage: client_tipc [lower [upper [kill]]]
 *
 * If no arguments supplied, sends a predetermined set of multicast messages.
 * If optional "lower" and "upper" arguments are specified, client sends a
 * single message to the specified instance range; if "upper" is omitted,
 * it defaults to the same value as "lower".
 * If optional "kill" argument is specified (it can have any value), the client
 * sends the server termination message rather than the normal server data
 * message.
 */

int main(int argc, char *argv[], char *envp[])
{
	int lower;
	int upper;
	char dummy;
	int sd = socket(AF_TIPC, SOCK_RDM, 0);

	if (sd < 0) {
		printf("TIPC not active on this node\n");
		exit(1);
	}

	/* run standard demo if no arguments supplied by user */

	if (argc < 2) {
		printf("****** TIPC client multicast demo started ******\n\n");

		wait_for_server(SERVER_TYPE, 399, 10000);

		client_mcast(sd,  99, 100);
		client_mcast(sd, 150, 250);
		client_mcast(sd, 200, 399);
		client_mcast(sd,   0, 399);
		client_mcast(-sd,  0, 399);

		printf("\n****** TIPC client multicast demo finished ******\n");
		exit(0);
	}

	/* multicast to instance range specified by user */

	if (sscanf(argv[1], "%u%c", &lower, &dummy) != 1) {
		perror("Client: invalid lower bound");
		exit(1);
	}
	if (argc > 2) {
		if (sscanf(argv[2], "%u%c", &upper, &dummy) != 1) {
			perror("Client: invalid upper bound");
			exit(1);
		}
	} else
		upper = lower;

	client_mcast((argc > 3) ? -sd : sd, lower, upper);

	exit(0);
}
