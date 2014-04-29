/* ------------------------------------------------------------------------
 *
 * server_tipc.c
 *
 * Short description: TIPC benchmark demo (server side)
 *
 * ------------------------------------------------------------------------
 *
 * Copyright (c) 2001-2005, 2014, Ericsson AB
 * Copyright (c) 2004-2006, Wind River Systems
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

#include "common_tipc.h"

#define SRV_TIMEOUT 30

static unsigned char *buf = NULL;
static int wait_for_connection(int listener_sd);
static void echo_messages(int peer_sd, int master_sd, int srv_id);
static __u32 own_node_addr;

static void srv_to_master(uint cmd, struct srv_info *sinfo)
{
	struct srv_to_master_cmd c;

	memset(&c, 0, sizeof(c));
	c.cmd = htonl(cmd);
	c.tipc_addr = htonl(own_node_addr);
	if (sinfo)
		memcpy(&c.sinfo, sinfo, sizeof(*sinfo));
	if (sizeof(c) != sendto(master_sd, &c, sizeof(c), 0,	
				(struct sockaddr *)&master_srv_addr,
				sizeof(master_srv_addr)))
		die("Server: unable to send info to master\n");
}

static void srv_from_master(uint *cmd, uint* msglen, uint *msgcnt, uint *echo)
{
	struct master_srv_cmd c;

	if (wait_for_msg(master_sd))
		die("No command from master\n");

	if (sizeof(c) != recv(master_sd, &c, sizeof(c), 0))
		die("Server: Invalid info msg from master\n");

	*cmd = ntohl(c.cmd);
	*msglen = ntohl(c.msglen);
	if (msgcnt)
		*msgcnt = ntohl(c.msgcnt);
	if (echo)
		*echo = ntohl(c.echo);
}

int main(int argc, char *argv[], char *dummy[])
{
	ushort tcp_port = 4711;
	struct srv_info sinfo;
	uint cmd;
	uint max_msglen;
	struct sockaddr_in srv_addr;
	int lstn_sd, peer_sd;
	int srv_id = 0, srv_cnt = 0;;

	own_node_addr = own_node();

	memset(&sinfo, 0, sizeof(sinfo));
		
	if (signal(SIGALRM, sig_alarm) == SIG_ERR)
		die("Server master: can't catch alarm signals\n");

	printf("******   TIPC Benchmark Server Started   ******\n");

	/* Create socket for communication with master: */
reset:
	master_sd = socket(AF_TIPC, SOCK_RDM, 0);
	if (master_sd < 0)
		die("Server: Can't create socket to master\n");

	if (bind(master_sd, (struct sockaddr *)&srv_ctrl_addr,
		 sizeof(srv_ctrl_addr)))
		die("Server: Failed to bind to master socket\n");

	/* Wait for command from master: */
	srv_from_master(&cmd, &max_msglen, 0, 0);
	buf = malloc(max_msglen);
	if (!buf)
		die("Failed to create buffer of size %u\n", ntohl(max_msglen));

	/* Create TIPC or TCP listening socket: */

	if (cmd == TIPC_CONN) {
		lstn_sd = socket (AF_TIPC, SOCK_STREAM,0);
		if (lstn_sd < 0)
			die("Server master: can't create listening socket\n");
		
		if (bind(lstn_sd, (struct sockaddr *)&srv_lstn_addr,
			 sizeof(srv_lstn_addr)) < 0)
			die("TIPC Server master: failed to bind port name\n");

		printf("******   TIPC Listener Socket Created    ******\n");
		srv_to_master(SRV_INFO, 0);
		close(master_sd);

	} else if (cmd == TCP_CONN) {
		if ((lstn_sd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
			die("TCP Server: failed to create listener socket");

		/* Construct listener address structure */
		memset(&srv_addr, 0, sizeof(srv_addr));
		srv_addr.sin_family = AF_INET;
		srv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
		srv_addr.sin_port = htons(tcp_port);
	
		/* Bind socket to address */
		while (0 > bind(lstn_sd, (struct sockaddr *) &srv_addr,
				sizeof(srv_addr)))
			srv_addr.sin_port = htons(++tcp_port);

		/* Inform master about own IP addresses and listener port number */
		get_ip_list(&sinfo);
		sinfo.tcp_port = htons(tcp_port);
		printf("******    TCP Listener Socket Created    ******\n");
		srv_to_master(SRV_INFO, &sinfo);
		close(master_sd);
	} else {
		close(master_sd);
		goto reset;
	}

	/* Listen for incoming connections */
	if (listen(lstn_sd, 32) < 0)
		die("Server: listen() failed");

	while (1) {
		if (waitpid(-1, NULL, WNOHANG) > 0) {
			if (--srv_cnt)
				continue;
			close(lstn_sd);
			printf("******      Listener Socket Deleted      ******\n");
			goto reset;
		}

		peer_sd = wait_for_connection(lstn_sd);
		if (!peer_sd)
			continue;
		srv_id++;
		srv_cnt++;
		if (fork()) {
			close(peer_sd);
			continue;
		}

		/* Continue in child process */
		close(lstn_sd);
		dprintf("calling echo: peer_sd: %u, srv_cnt = %u\n",peer_sd, srv_cnt);
		master_sd = socket(AF_TIPC, SOCK_RDM, 0);
		if (master_sd < 0)
			die("Server: Can't create socket to master\n");
		
		if (bind(master_sd, (struct sockaddr *)&srv_ctrl_addr,
			 sizeof(srv_ctrl_addr)))
			die("Server: Failed to bind to master socket\n");
		
		echo_messages(peer_sd, master_sd, srv_id);
	}
	close(lstn_sd);
	printf("******   TIPC Benchmark Server Finished   ******\n");
	exit(0);
	return 0;
}

static int wait_for_connection(int lstn_sd)
{
	int peer_sd;
	fd_set fds;
	struct timeval tv;
	int res;
	
	/* Accept another client connection */
	
	FD_ZERO(&fds);
	FD_SET(lstn_sd, &fds);
	tv.tv_sec =  0;
	tv.tv_usec = 500000;
	res = select(lstn_sd + 1, &fds, 0, 0, &tv);
	if (res > 0 && FD_ISSET(lstn_sd, &fds)) {
		peer_sd = accept(lstn_sd, 0, 0);
		if (peer_sd <= 0 )
			die("Server master: accept failed\n");
		return peer_sd;
	}
	return 0;
}

static void echo_messages(int peer_sd, int master_sd, int srv_id)
{
	uint cmd, msglen, msgcnt, echo, rcvd = 0;

	do {
		/* Get msg length and number to expect, and ack: */
		srv_from_master(&cmd, &msglen, &msgcnt, &echo);

		if (cmd != RCV_MSG_LEN)
			break;

		srv_to_master(SRV_MSGLEN_ACK, 0);

		dprintf("srv %u: expecting %u msgs of size %u, echoing = %u\n", 
			srv_id, msgcnt,msglen,echo);
		while (rcvd < msgcnt) {
			if (wait_for_msg(peer_sd))
				die("poll() from client failed\n");
			if (msglen != recv(peer_sd, buf, msglen, MSG_WAITALL))
				die("Server %u: echo_messages recv() error\n", srv_id);
			rcvd++;
			if (!echo)
				continue;
			if (msglen != send(peer_sd, buf, msglen, 0))
				die("echo_msg: send failed\n");
		};
		dprintf("srv %u: reporting FINISHED to master\n", srv_id);
		srv_to_master(SRV_FINISHED, 0);
		rcvd = 0;
	} while (1);

	dprintf("Server shutdown\n");
	shutdown(peer_sd, SHUT_RDWR);
	close(peer_sd);
	close(master_sd);
	exit(0);
}
