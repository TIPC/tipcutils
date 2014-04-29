/* ------------------------------------------------------------------------
 *
 * client_tipc.c
 *
 * Short description: TIPC benchmark demo (client side)
 *
 * ------------------------------------------------------------------------
 *
 * Copyright (c) 2001-2005, 2014, Ericsson AB
 * Copyright (c) 2004-2006, 2010-2011 Wind River Systems
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

#define TERMINATE 1
#define DEFAULT_CLIENTS   8
#define DEFAULT_LAT_MSGS  200000
#define DEFAULT_THRU_MSGS 200000
#define DEFAULT_BURST     16
#define DEFAULT_MSGLEN    64


static const struct sockaddr_tipc clnt_ctrl_addr = {
	.family                  = AF_TIPC,
	.addrtype                = TIPC_ADDR_NAMESEQ,
	.addr.nameseq.type       = CLNT_CTRL_NAME,
	.addr.nameseq.lower      = 0,
	.addr.nameseq.upper      = 0,
	.scope                   = TIPC_NODE_SCOPE
};

static const struct sockaddr_tipc master_clnt_addr = {
	.family                  = AF_TIPC,
	.addrtype                = TIPC_ADDR_NAME,
	.addr.name.name.type     = MASTER_NAME,
	.addr.name.name.instance = 1,
	.scope                   = TIPC_ZONE_SCOPE,
	.addr.name.domain        = 0
};

static int master_clnt_sd;
static int master_srv_sd;
static uint client_id;
static unsigned char *buf = NULL;
static int select_ip(struct srv_info *sinfo);
static void stream_messages(int peer_sd, int clnt_id,
			    int msgcnt, int msglen,
			    int bounce);


#define CLNT_EXEC         3
#define CLNT_TERM         4
struct master_client_cmd {
	__u32 cmd;
	__u32 msglen;
	__u32 msgcnt;
	__u32 bounce;
};

static void master_to_client(uint cmd, uint msglen, uint msgcnt, uint bounce)
{
	struct master_client_cmd c;

	c.cmd = htonl(cmd);
	c.msglen = htonl(msglen);
	c.msgcnt = htonl(msgcnt);
	c.bounce = htonl(bounce);
	if (sizeof(c) != sendto(master_clnt_sd, &c, sizeof(c), 0,
				(struct sockaddr *)&clnt_ctrl_addr,
				sizeof(clnt_ctrl_addr)))
		die("Unable to send cmd %u to clients\n", cmd);
}

static void client_from_master(uint *cmd, uint *msglen, uint *msgcnt, uint *bounce)
{
	struct master_client_cmd c;

	if (wait_for_msg(master_sd))
		die("Client: No command from master\n");
	if (recv(master_sd, &c, sizeof(c), 0) != sizeof(c))
		die("Client: Invalid msg msg from master\n");
	*cmd = ntohl(c.cmd);
	*msglen = ntohl(c.msglen);
	*msgcnt = ntohl(c.msgcnt);
	*bounce = ntohl(c.bounce);
}


#define CLNT_READY    1
#define CLNT_FINISHED 2
struct client_master_cmd {
	__u32 cmd;
};

static void client_to_master(uint cmd)
{
	struct client_master_cmd c;

	c.cmd = htonl(cmd);
	if (sizeof(c) != sendto(master_sd, &c, sizeof(c), 0,
				(struct sockaddr *)&master_clnt_addr,
				sizeof(master_clnt_addr)))
		die("Client: Unable to send msg to master\n");
}

static void master_from_client(uint *cmd)
{
	struct client_master_cmd c;

	if (wait_for_msg(master_clnt_sd))
		die("Client: No command from master\n");
	
	if (recv(master_clnt_sd, &c, sizeof(c), 0) != sizeof(c))
		die("Client: Invalid msg msg from master\n");
	*cmd = ntohl(c.cmd);
}

static void master_to_srv(uint cmd, uint msglen, uint msgcnt, uint echo)
{
	struct master_srv_cmd c;

	c.cmd = htonl(cmd);
	c.msglen = htonl(msglen);
	c.msgcnt = htonl(msgcnt);
	c.echo = htonl(echo);
	if (sizeof(c) != sendto(master_srv_sd, &c, sizeof(c), 0,
				(struct sockaddr *)&srv_ctrl_addr,
				sizeof(srv_ctrl_addr)))
		die("Unable to send cmd %u to servers\n", cmd);
}

static void master_from_srv(uint *cmd, struct srv_info *sinfo, __u32 *tipc_addr)
{
	struct srv_to_master_cmd c;

	if (wait_for_msg(master_srv_sd))
		die("Master: No info from server\n");
	
	if (sizeof(c) != recv(master_srv_sd, &c, sizeof(c), 0))
		die("Master: Invalid info msg from server\n");
	
	*cmd = ntohl(c.cmd);
	if (tipc_addr)
		*tipc_addr = ntohl(c.tipc_addr);
	if (sinfo)
		memcpy(sinfo, &c.sinfo, sizeof(*sinfo));
}

static void usage(char *app)
{
	fprintf(stderr, "Usage:\n");
	fprintf(stderr," %s ", app);
	fprintf(stderr, "[-l <lat msgs>] [-t <tput <msgs>]"
                         " [-c <num conns>] [-p <tipc | tcp>]\n");
	fprintf(stderr, "\tmsgs to transfer for latency measurement (default %u)\n",
		DEFAULT_LAT_MSGS);
	fprintf(stderr, "\tmsgs to transfer for throughput measurement (default %u)\n",
		DEFAULT_THRU_MSGS);
	fprintf(stderr, "\tnumber of connections defaults to %d\n", DEFAULT_CLIENTS);
	fprintf(stderr, "\tprotocol to measure (defaults to tipc)\n");
}

static unsigned long long elapsednanos(struct timeval *from)
{
	struct timeval now;

	gettimeofday(&now, 0);

	if (now.tv_usec >= from->tv_usec)
		return((now.tv_sec - from->tv_sec) * 1000000000 +
		       (now.tv_usec - from->tv_usec) * 1000);
	else
		return((now.tv_sec - 1 - from->tv_sec) * 1000000000 +
		       (now.tv_usec + 1000000 - from->tv_usec) * 1000);
}

static void print_throughput_header(void)
{
	printf("+----------------------------------------------"
	       "-----------------------------------------------+\n");
	printf("|  Msg Size  | #     |  # Msgs/  |  Elapsed  |"
	       "                    Throughput                  |\n");
	printf("|  [octets]  | Conns |    Conn   |  [ms]     +"
	       "------------------------------------------------+\n");
	printf("|            |       |           |           | "
	       "Total [Msg/s] | Total [Mb/s] | Per Conn [Mb/s] |\n");
	printf("+-----------------------------------------------"
	       "----------------------------------------------+\n");
}

static void print_latency_header(void)
{
	printf("+----------------------------------------"
	       "-----------------------------+\n");
	printf("| Msg Size [octets] |   # Msgs   |"
	       " Elapsed [ms] | Avg round-trip [us] |\n");
	printf("+-----------------------------------------"
	       "----------------------------+\n");
}

static void client_create(unsigned int clnt_id, ushort tcp_port, int tcp_addr)
{
	int peer_sd;
	int imp = TIPC_MEDIUM_IMPORTANCE;
	uint cmd, msglen, msgcnt, bounce;
	struct sockaddr_in tcp_dest;

	fflush(stdout);
	if (fork())
		return;
	dprintf("Client %u created\n", clnt_id);
	client_id = clnt_id;
	close(master_clnt_sd);
	
	/* Create socket for communication with master: */

	master_sd = socket(AF_TIPC, SOCK_RDM, 0);
	if (master_sd < 0)
		die("Client %u: Can't create socket to master\n", clnt_id);
	
	if (bind(master_sd, (struct sockaddr *)&clnt_ctrl_addr, sizeof(clnt_ctrl_addr)))
		die("Client %u: Failed to bind\n", clnt_id);

	/* Establish connection to benchmark server */

	if (!tcp_port) {

		peer_sd = socket(AF_TIPC, SOCK_STREAM, 0);
		if (peer_sd < 0)
			die("Client %u: Can't create socket to server\n", clnt_id);
		
		if (setsockopt(peer_sd, SOL_TIPC, TIPC_IMPORTANCE,
			       &imp, sizeof(imp)) != 0)
			die("Client %u: Can't set socket options\n", clnt_id);

		/* Establish connection to server */

		if (connect(peer_sd, (struct sockaddr*)&srv_lstn_addr,
			    sizeof(srv_lstn_addr)) < 0)
			die("Client %u: connect failed\n", clnt_id);
	} else {

		if ((peer_sd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
			die("TCP Server: failed to create client socket");
		memset(&tcp_dest, 0, sizeof(tcp_dest));
		tcp_dest.sin_family = AF_INET;
		tcp_dest.sin_addr.s_addr = htonl(tcp_addr);
		tcp_dest.sin_port = htons(tcp_port);
		dprintf("TCP Client %u: using %s:%u \n", clnt_id,
			inet_ntoa(tcp_dest.sin_addr),tcp_port);		
		if (0 > connect(peer_sd, (struct sockaddr *) &tcp_dest, 
				sizeof(tcp_dest)))
			die("TCP connect() failed");
	}

	/* Notify master that we're ready to run tests */
	client_to_master(CLNT_READY);

	/* Process commands from client master until told to shut down */

	for (;;) {
		client_from_master(&cmd, &msglen, &msgcnt, &bounce);
		if (cmd == CLNT_TERM) {
			shutdown(peer_sd, SHUT_RDWR);
			close(peer_sd);
			close(master_sd);
			exit(0);
		}

		/* Execute command */
		stream_messages(peer_sd, client_id, msgcnt, msglen, bounce);

		/* Done. Tell master */
		client_to_master(CLNT_FINISHED);
	}
}

static void stream_messages(int peer_sd, int clnt_id, int msgcnt,
			    int msglen, int bounce)
{
	int sent = 0;
	int yield = (1 << 21) / msglen;
	dprintf("Cli %u: bouncing %u msg of len %u, bounce = %u\n",
		client_id, msgcnt,msglen,bounce);
	while (sent < msgcnt) {
		if (--yield == 0) {
			sched_yield();
			yield = (1 << 22) / msglen;
		}
		sent++;
		if (msglen != send(peer_sd, buf, msglen, 0))
			die("Client %u: send failed\n", clnt_id);

		if (!bounce)
			continue;

		if (wait_for_msg(peer_sd))
			die("Client %u: no resp from srv at %u\n", clnt_id, sent);
			
		if (msglen != recv(peer_sd, buf, msglen, MSG_WAITALL))
			die("Client %u: invalid msg from server \n", clnt_id);
	};
	dprintf("cli %u: reporting FINISHED to master\n", clnt_id);
}

/*
 * Master
 */
int main(int argc, char *argv[], char *dummy[])
{
	int c;
	uint cmd;
	uint latency_transf = DEFAULT_LAT_MSGS;
	uint thruput_transf = DEFAULT_THRU_MSGS;
	uint req_clients = DEFAULT_CLIENTS;
	uint first_msglen = DEFAULT_MSGLEN;
	uint last_msglen = TIPC_MAX_USER_MSG_SIZE;
	unsigned long long msglen;
	unsigned long long num_clients;
	struct timeval start_time;
	unsigned long long elapsed;
	unsigned long long msgcnt;
	unsigned long long iter;
	uint clnt_id;
	uint conn_typ = TIPC_CONN;
	ushort tcp_port = 0;
	uint tcp_addr = 0;
	struct srv_info sinfo;
	__u32 peer_tipc_addr;

	setbuf(stdout, NULL);

	/* Process command line arguments */

	while ((c = getopt(argc, argv, "l:t:c:p:m:")) != -1) {
		switch (c) {
		case 'l':
			latency_transf = atoi(optarg);
			break;
		case 't':
			thruput_transf = atoi(optarg);
			break;
		case 'm':
			first_msglen = atoi(optarg);
			last_msglen = first_msglen;
			break;
		case 'c':
			req_clients = atoi(optarg);
			if (req_clients == 0)
				die("We need at least one connection\n");
			break;
		case 'p':
			if (!strcmp("tcp", optarg))
				conn_typ = TCP_CONN;
			else if (strcmp("tipc", optarg))
				die("Invalid protocol; must be 'tcp' or 'tipc'\n");
			break;
		default:
			usage(argv[0]);
			return 1;
		}
	}

	buf = malloc(last_msglen);
	if (!buf)
		die("Unable to allocate buffer\n");

	/* Create socket used to communicate with clients */

	master_clnt_sd = socket(AF_TIPC, SOCK_RDM, 0);
	if (master_clnt_sd < 0)
		die("Master: Can't create client ctrl socket\n");

	if (bind(master_clnt_sd, (struct sockaddr *)&master_clnt_addr, 
		 sizeof(master_clnt_addr)))
		die("Master: Failed to bind to clientcontrol address\n");

	/* Create socket used to communicate with servers */

	master_srv_sd = socket(AF_TIPC, SOCK_RDM, 0);
	if (master_srv_sd < 0)
		die("Master: Can't create server ctrl socket\n");

	if (bind(master_srv_sd, (struct sockaddr *)&master_srv_addr, 
		 sizeof(master_srv_addr)))
		die("Master: Failed to bind to server control address\n");

	/* Wait for benchmark server to appear: */

	wait_for_name(SRV_CTRL_NAME, 0, MAX_DELAY);
	master_to_srv(RESTART, 0, 0, 0);
	sleep(1);

	/* Send connection type and buffer allocation size to server: */
	master_to_srv(conn_typ, last_msglen, 0, 0);

	/* Wait for ack */

	master_from_srv(&cmd, &sinfo, &peer_tipc_addr);
	if (peer_tipc_addr != own_node()) {
		if (latency_transf == DEFAULT_LAT_MSGS)
			latency_transf /= 10;
		if (thruput_transf == DEFAULT_THRU_MSGS)
			thruput_transf /= 10;			
	}
	
	tcp_port = ntohs(sinfo.tcp_port);
	tcp_addr = select_ip(&sinfo);

	printf("****** TIPC Benchmark Client Started ******\n");
	if (conn_typ == TCP_CONN) {
		struct in_addr s;
		s.s_addr = ntohl(tcp_addr);
		printf("Using server address %s:%d\n", 
		       inet_ntoa(s),tcp_port);
	}
	num_clients = 0;

	/* Optionally run latency test */
	if (!latency_transf)
		goto end_latency;

	printf("Transferring %u messages in %s Latency Benchmark\n", 
	       latency_transf, tcp_port ? "TCP" : "TIPC");

	/* Create first child client and wait until it is connected */
	client_create(++num_clients, tcp_port, tcp_addr);
	master_from_client(&cmd);
	sleep(1);
	print_latency_header();
	iter = 1;

	for (msglen = first_msglen; msglen <= last_msglen; msglen *= 4) {

		unsigned long long latency;
		unsigned long long latency_us;
		unsigned long long latency_dec;

		msgcnt = latency_transf / iter++;

		printf("| %16llu  | %9llu  |", msglen, msgcnt);

		/* Tell server and client instances what to do: */
		master_to_srv(RCV_MSG_LEN, msglen, msgcnt, 1);
		master_from_srv(&cmd, 0, 0);

		gettimeofday(&start_time, 0);
		master_to_client(CLNT_EXEC, msglen, msgcnt, 1);

		/* Wait until client and server are finished:*/
		master_from_client(&cmd);
		master_from_srv(&cmd, 0, 0);

		/* Calculate and present result: */
		elapsed = elapsednanos(&start_time);
		latency = elapsed/msgcnt;
		latency_us = latency/1000;
		latency_dec = latency%100;

		printf(" %11llu  | %15llu.%llu  |\n", 
		       elapsed/1000000, latency_us, latency_dec);
		printf("+--------------------------------------------------"
		       "-------------------+\n");
	}
	printf("Completed Latency Benchmark\n\n");

end_latency:

	/* Optionally run throughput test */

	if (!thruput_transf)
		goto end_thruput;

	printf("Transferring %u messages in %s Throughput Benchmark\n", 
	       thruput_transf, tcp_port ? "TCP" : "TIPC");

	/* Create remaining child clients. For each, wait until it is ready */

	while (num_clients < req_clients) {
		client_create(++num_clients, tcp_port, tcp_addr);
		master_from_client(&cmd);
	}

	dprintf("Master: all clients and servers started\n");
	sleep(2);   /* let console printfs flush before continuing */

	print_throughput_header();
	iter = 1;

	for (msglen = first_msglen; msglen <= last_msglen; msglen *= 4) {

		unsigned long long thruput;
		unsigned long long msg_per_sec;
		int i;

		msgcnt = thruput_transf / iter++;

		printf("| %9llu  | %4llu  | %8llu  ", msglen, num_clients, msgcnt);

		gettimeofday(&start_time, 0);

		/* Tell servers what to expect */
		master_to_srv(RCV_MSG_LEN, msglen, msgcnt, 0);

		/* Wait until all servers are ready: */
		for (i = 1; i <= num_clients; i++) {
			master_from_srv(&cmd, 0, 0);
		}

		/* Tell clients to run a throughput test: */
		master_to_client(CLNT_EXEC, msglen, msgcnt, 0);

		/* Wait until all clients and servers are finished */
		for (i = 1; i <= num_clients; i++) {
			master_from_client(&cmd);
			master_from_srv(&cmd, 0, 0);
		}

		/* Calculate and present result: */
		elapsed = elapsednanos(&start_time);
		msg_per_sec = (msgcnt * num_clients * 1000000000) / elapsed;
		thruput = msg_per_sec * msglen * 8/1000000;
		printf("| %8llu  | %12llu  | %11llu  | %14llu  |\n", 
		       elapsed/1000000, msg_per_sec, thruput, thruput/num_clients);
		printf("+-------------------------------------------------"
		       "--------------------------------------------+\n");
	}
	printf("Completed Throughput Benchmark\n");

end_thruput:

	/* Terminate all client processes */
	master_to_client(CLNT_TERM, 0, 0, 0);

	if (signal(SIGALRM, sig_alarm) == SIG_ERR)
		die("Master: Can't catch alarm signals\n");

	alarm(MAX_DELAY);
	for (clnt_id = 1; clnt_id <= num_clients; clnt_id++) {
		if (wait(NULL) <= 0)
			die("Master: error during termination\n");
	}

	printf("****** TIPC Benchmark Client Finished ******\n");
	shutdown(master_clnt_sd, SHUT_RDWR);
	close(master_clnt_sd);
	shutdown(master_srv_sd, SHUT_RDWR);
	close(master_srv_sd);
	exit(0);
}

static int select_ip(struct srv_info *sinfo)
{
	struct srv_info cinfo;
	int clnt_ip_num, srv_ip_num;
	int s_ipno = 0;
	int c_ipno = 0;
	int s_ip, c_ip, best_ip = 0;
	int best_prefix = 32;
	int mask, shift;

	get_ip_list(&cinfo);
	clnt_ip_num = ntohs(cinfo.num_ips);
	srv_ip_num = ntohs(sinfo->num_ips);
	for (; s_ipno < srv_ip_num; s_ipno++) {
		s_ip = ntohl(sinfo->ips[s_ipno]);
		for (c_ipno = 0; c_ipno < clnt_ip_num; c_ipno++) {
			c_ip = ntohl(cinfo.ips[c_ipno]);
			mask = ~0;
			shift = 0;

			if (c_ip == s_ip)
				return 0x7f000001;

			while ((c_ip & mask) != (s_ip & mask))
				mask <<= ++shift;

			if (shift < best_prefix) {
				best_prefix = shift;
				best_ip = s_ip;
			}
		}
	}
	return best_ip;
}
