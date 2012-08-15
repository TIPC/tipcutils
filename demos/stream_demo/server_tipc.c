/* ------------------------------------------------------------------------
 *
 * server_tipc.c
 *
 * Short description: TIPC stream demo (server side)
 *
 * ------------------------------------------------------------------------
 *
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
 * Neither the names of the copyright holders nor the names of its
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
#include <netinet/in.h>
#include <linux/tipc.h>

#define SERVER_TYPE  8888
#define SERVER_INST  17

#define MAX_REC_SIZE 256

int main(int argc, char *argv[], char *dummy[])
{
	struct sockaddr_tipc server_addr;
	int listener_sd;
	int peer_sd;
	char inbuf[MAX_REC_SIZE];
	char outbuf = 'X';
	int rec_num;

	printf("****** TIPC stream demo server started ******\n\n");

	listener_sd = socket(AF_TIPC, SOCK_STREAM, 0);
	if (listener_sd < 0) {
		perror("Server: unable to create socket\n");
		exit(1);
	}

	server_addr.family = AF_TIPC;
	server_addr.addrtype = TIPC_ADDR_NAMESEQ;
	server_addr.addr.nameseq.type = SERVER_TYPE;
	server_addr.addr.nameseq.lower = SERVER_INST;
	server_addr.addr.nameseq.upper = SERVER_INST;
	server_addr.scope = TIPC_ZONE_SCOPE;

	if (bind(listener_sd, (struct sockaddr *)&server_addr,
	                sizeof(server_addr))) {
		perror("Server: failed to bind port name\n");
		exit(1);
	}

	if (0 != listen(listener_sd, 0)) {
		perror("Server: failed to listen\n");
		exit (1);
	}

	peer_sd = accept(listener_sd, 0, 0);
	if (peer_sd < 0 ) {
		perror("Server: failed to accept connection\n");
		exit(1);
	}

	rec_num = 0;
	while (1) {
		int msg_size;
		int rec_size;

		msg_size = recv(peer_sd, inbuf, 4, MSG_WAITALL);
		if (msg_size == 0) {
			printf("Server: client terminated normally\n");
			exit(0);
		}
		if (msg_size < 0) {
			perror("Server: client terminated abnormally\n");
			exit(1);
		}

		rec_num++;

		rec_size = *(__u32 *)inbuf;
		rec_size = ntohl(rec_size);
		printf("Server: receiving record %d of %u bytes\n",
		       rec_num, rec_size);

		msg_size = recv(peer_sd, inbuf, rec_size, MSG_WAITALL);
		if (msg_size != rec_size) {
			printf("Server: receive error, got %d bytes\n",
			       msg_size);
			exit(1);
		}
		while (msg_size > 0) {
			if ((unsigned char)inbuf[--msg_size] != rec_size) {
				printf("Server: record content error\n");
				exit(1);
			}
		}
		printf("Server: record %d received\n", rec_num);

		/* Send 1 byte acknowledgement (value is irrelevant) */

		if (0 >= send(peer_sd, &outbuf, 1, 0)) {
			perror("Server: failed to send response\n");
			exit(1);
		}
		printf("Server: record %d acknowledged\n", rec_num);
	}

	printf("****** TIPC stream demo server finished ******\n");
	exit(0);
}
