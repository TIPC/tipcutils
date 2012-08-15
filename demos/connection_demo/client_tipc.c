/* ------------------------------------------------------------------------
 *
 * client_tipc.c
 *
 * Short description: TIPC connection demo (client side)
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
#include <sys/time.h>
#include <netinet/in.h>
#include <linux/tipc.h>

#define SERVER_TYPE  18888
#define SERVER_INST  17

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


int main(int argc, char *argv[], char *dummy[])
{
	int sd;
	struct sockaddr_tipc server_addr;
	char buf[40];

	printf("****** TIPC connection demo client started ******\n\n");

	wait_for_server(SERVER_TYPE, SERVER_INST, 10000);

	server_addr.family = AF_TIPC;
	server_addr.addrtype = TIPC_ADDR_NAME;
	server_addr.addr.name.name.type = SERVER_TYPE;
	server_addr.addr.name.name.instance = SERVER_INST;
	server_addr.addr.name.domain = 0;

	printf("Client: connection setup 1 - standard (TCP style) connect\n");
	sd = socket (AF_TIPC, SOCK_SEQPACKET, 0);
	if (connect(sd, (struct sockaddr *)&server_addr,
	                sizeof(server_addr)) != 0) {
		perror("Client: connect failed");
		exit(1);
	}
	printf("Client: connection established\n");
	strcpy(buf, "Hello World");
	if (0 > send(sd, buf, strlen(buf)+1, 0)) {
		perror("Client: failed to send");
		exit(1);
	}
	printf("Client: Sent msg \"%s\" \n", buf);
	if (0 >= recv(sd, buf, sizeof(buf), 0)) {
		perror("Client: failed to receive");
		exit(1);
	}
	printf("Client: received response \"%s\" \n", buf);
	printf("Client: shutting down connection\n\n");
	shutdown(sd, SHUT_RDWR);
	close(sd);

	printf("Client: connection setup 2 - optimized (TIPC style) connect\n");
	sd = socket(AF_TIPC, SOCK_SEQPACKET, 0);
	strcpy(buf, "Hello Again");
	if (0 > sendto(sd, buf, strlen(buf)+1, 0,
	                (struct sockaddr *)&server_addr, sizeof(server_addr))) {
		perror("Client: failed to send");
		exit(1);
	}
	printf("Client: sent msg \"%s\" \n", buf);
	if (0 >= recv(sd, buf, sizeof(buf), 0)) {
		perror("Client: failed to receive");
		exit(1);
	}
	printf("Client: received response \"%s\"\n", buf);
	printf("Client: killing connection without shutdown\n\n");
	close(sd);

	printf("Client: connection setup 3 - optimized (TIPC style) connect\n");
	sd = socket (AF_TIPC, SOCK_SEQPACKET,0);
	strcpy(buf, "Hello Again Again");
	if (0 > sendto(sd, buf, strlen(buf)+1, 0,
	                (struct sockaddr*)&server_addr, sizeof(server_addr))) {
		perror("Client: failed to send");
		exit(1);
	}
	printf("Client: sent msg \"%s\"\n", buf);
	printf("Client: will now exit without closing socket!!\n\n");
	exit(0);
}
