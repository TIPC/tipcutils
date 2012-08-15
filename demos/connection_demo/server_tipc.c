/* ------------------------------------------------------------------------
 *
 * server_tipc.c
 *
 * Short description: TIPC connection demo (server side)
 *
 * ------------------------------------------------------------------------
 *
 * Copyright (c) 2003, Ericsson Research Canada
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
#include <assert.h>
#include <string.h>
#include <sys/param.h>
#include <errno.h>
#include <unistd.h>
#include <linux/tipc.h>

#define SERVER_TYPE  18888
#define SERVER_INST  17
#define BUF_SZ 40

int main(int argc, char *argv[], char *dummy[])
{
	struct sockaddr_tipc server_addr;
	int listener_sd;
	int conn_count;

	server_addr.family = AF_TIPC;
	server_addr.addrtype = TIPC_ADDR_NAMESEQ;
	server_addr.addr.nameseq.type = SERVER_TYPE;
	server_addr.addr.nameseq.lower = SERVER_INST;
	server_addr.addr.nameseq.upper = SERVER_INST;
	server_addr.scope = TIPC_ZONE_SCOPE;

	printf("****** TIPC connection demo server started ******\n");

	listener_sd = socket (AF_TIPC, SOCK_SEQPACKET, 0);

	if (0 != bind(listener_sd, (struct sockaddr *)&server_addr,
	                sizeof(server_addr))) {
		printf("Server: failed to bind port name\n");
		exit(1);
	}

	if (0 != listen(listener_sd, 0)) {
		printf ("Server: failed to listen\n");
		exit(1);
	}

	for (conn_count = 1; conn_count <= 3; conn_count++) {
		int peer_sd;
		int sz;
		char inbuf[BUF_SZ];
		char outbuf[BUF_SZ];

		peer_sd = accept(listener_sd, 0, 0);
		if (peer_sd < 0 ) {
			printf ("Server: accept failed\n");
			exit(1);
		}
		printf("\nServer: accept() returned\n");
		fflush(stdout);
		if (!fork()) {
			printf ("Server process %d created \n", conn_count);
			while (1) {
				sz = recv(peer_sd, inbuf, BUF_SZ, 0);
				if (sz == 0) {
					printf("Server %d: client terminated normally\n",
					       conn_count);
					exit(0);
				}
				if (sz < 0) {
					printf("Server %d : client terminated abnormally\n",
					       conn_count);
					exit(1);
				}
				printf("Server %d: received msg \"%s\"\n",
				       conn_count, inbuf);
				sprintf(outbuf, "Response for test %d", conn_count);
				if (0 >= send(peer_sd, outbuf, strlen(outbuf)+1, 0)) {
					printf("Server %d : failed to send response\n",
					       conn_count);
					exit(1);
				}
				printf("Server %d: responded with \"%s\"\n",
				       conn_count, outbuf);
			}
		}
	}

	exit(0);
}
