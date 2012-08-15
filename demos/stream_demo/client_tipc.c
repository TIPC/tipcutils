/* ------------------------------------------------------------------------
 *
 * server_tipc.c
 *
 * Short description: TIPC stream demo (server side)
 *
 * ------------------------------------------------------------------------
 *
 * Copyright (c) 2005,2010 Wind River Systems
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

#define BUF_SIZE 2000
#define MSG_SIZE 80

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
	char buf[BUF_SIZE];
	int rec_num;
	int rec_size;
	int tot_size;
	int sent_size;
	int msg_size;

	printf("****** TIPC stream demo client started ******\n\n");

	wait_for_server(SERVER_TYPE, SERVER_INST, 10000);

	sd = socket(AF_TIPC, SOCK_STREAM, 0);

	server_addr.family = AF_TIPC;
	server_addr.addrtype = TIPC_ADDR_NAME;
	server_addr.addr.name.name.type = SERVER_TYPE;
	server_addr.addr.name.name.instance = SERVER_INST;
	server_addr.addr.name.domain = 0;

	if (connect(sd, (struct sockaddr *)&server_addr,
	                sizeof(server_addr)) != 0) {
		perror("Client: connect failed");
		exit(1);
	}

	/* Create buffer containing numerous (size,data) records */

	tot_size = 0;
	rec_size = 1;
	rec_num = 0;

	while ((tot_size + 4 + rec_size) <= BUF_SIZE) {
		__u32 size;

		rec_num++;
		size = htonl(rec_size);
		*(__u32 *)&buf[tot_size] = size;
		memset(&buf[tot_size + 4], rec_size, rec_size);
		printf("Client: creating record %d of size %d bytes\n",
		       rec_num, rec_size);

		tot_size += (4 + rec_size);
		rec_size = (rec_size + 147) & 0xFF;
		if (!rec_size)
			rec_size = 1; /* record size must be 1-255 bytes */
	}

	/* Now send records using messages that break record boundaries */

	printf("Client: sending records using %d byte messages\n", MSG_SIZE);
	sent_size = 0;
	while (sent_size < tot_size) {
		if ((sent_size + MSG_SIZE) <= tot_size)
			msg_size = MSG_SIZE;
		else
			msg_size = (tot_size - sent_size);
		if (0 > send(sd, &buf[sent_size], msg_size, 0)) {
			perror("Client: failed to send");
			exit(1);
		}
		sent_size += msg_size;
	}

	/* Now grab set of one-byte client acknowledgements all at once */

	printf("Client: waiting for server acknowledgements\n");
	if (recv(sd, buf, rec_num, MSG_WAITALL) != rec_num) {
		perror("Client: acknowledge error 1");
		exit(1);
	}
	if (recv(sd, buf, 1, MSG_DONTWAIT) >= 0) {
		perror("Client: acknowledge error 2");
		exit(1);
	}
	printf("Client: received %d acknowledgements\n", rec_num);

	shutdown(sd, SHUT_RDWR);
	close(sd);

	printf("****** TIPC stream demo client finished ******\n");
	exit(0);
}
