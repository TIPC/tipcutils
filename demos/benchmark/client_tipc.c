/* ------------------------------------------------------------------------
 *
 * client_tipc.c
 *
 * Short description: TIPC benchmark demo (client side)
 * 
 * ------------------------------------------------------------------------
 *
 * Copyright (c) 2001-2005, Ericsson Research Canada
 * Copyright (c) 2004-2006,2010 Wind River Systems
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

#include <getopt.h>
#include <netinet/in.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <linux/tipc.h>

#define MAX_DELAY 300000		/* inactivity limit [in ms] */
#define MCLIENT_NAME 16666
#define SERVER_NAME  17777
#define CLIENT_NAME  18888
#define TERMINATE 1
#define DEFAULT_CLIENTS 8

#define DEBUG 0

#define dprintf(fmt, arg...)  do {if (DEBUG) printf(fmt, ## arg);} while(0)

unsigned char buf[TIPC_MAX_USER_MSG_SIZE] = {0,};

struct client_cmd {
	unsigned int cmd; /* 0: exec, 1: terminate */
	unsigned long long msg_size;
	unsigned long long msg_count;
	unsigned long long burst_size;
	unsigned int client_no;
};

__u32 wait_for_server(__u32 name_type, __u32 name_instance, int wait)
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

	return ntohl(event.port.node);
}


int wait_for_msg(int sd)
{
	struct pollfd pfd;
	int pollres;
	int res;

	pfd.events = ~POLLOUT;
	pfd.fd = sd;
	pollres = poll(&pfd, 1, MAX_DELAY);
	if (pollres < 0)
		res = -1;
	else if (pollres == 0)
		res = -2;
	else
		res = (pfd.revents & POLLIN) ? 0 : pfd.revents;
	return res;
}

static unsigned long long elapsedmillis(struct timeval *from)
{
	struct timeval now;

	gettimeofday(&now, 0);

	if (now.tv_usec >= from->tv_usec)
		return((now.tv_sec - from->tv_sec) * 1000 +
		       (now.tv_usec - from->tv_usec) / 1000);
	else
		return((now.tv_sec - 1 - from->tv_sec) * 1000 +
		       (now.tv_usec + 1000000 - from->tv_usec) / 1000); 
}

void clientmain(unsigned int client_id)
{
	struct sockaddr_tipc dest_addr;
	struct sockaddr_tipc server_addr;
	struct sockaddr_tipc client_master_addr;
	int server_comm_sd;
	int master_comm_sd;
	int imp = TIPC_MEDIUM_IMPORTANCE;
	__u32 server_num = 0;
	unsigned long long counter = 0;
	unsigned long long init_cnt = 0;
	unsigned long long burst;
	int wait_err;
	int sz;
	struct client_cmd cmd;

	/* Establish socket used for communication with client master */

	master_comm_sd = socket(AF_TIPC, SOCK_RDM, 0);
	if (master_comm_sd < 0) {
		printf("Client %u: Can't create socket to master\n", client_id);
		perror(NULL);
		exit(1);
	}

	dest_addr.family = AF_TIPC;
	dest_addr.addrtype = TIPC_ADDR_NAME;
	dest_addr.scope = TIPC_NODE_SCOPE;
	dest_addr.addr.name.name.type = CLIENT_NAME;
	dest_addr.addr.name.name.instance = client_id;

	if (bind(master_comm_sd, (struct sockaddr *)&dest_addr,
		 sizeof(dest_addr))) {
		printf("Client %u: Failed to bind\n", client_id);
		perror(NULL);
		exit(1);
	}

	/* Establish connection to benchmark server client_id */

	dprintf("Client %u: started, waiting for server...\n", client_id);

	server_addr.family = AF_TIPC;
	server_addr.addrtype = TIPC_ADDR_NAME;
	server_addr.addr.name.name.type = SERVER_NAME;
	server_addr.addr.name.name.instance = 0;
	server_addr.addr.name.domain = 0;

	server_comm_sd = socket(AF_TIPC, SOCK_SEQPACKET, 0);
	if (server_comm_sd < 0) {
		printf("Client %u: Can't create socket to server\n", client_id);
		perror(NULL);
		exit(1);
	}

	if (setsockopt(server_comm_sd, SOL_TIPC, TIPC_IMPORTANCE, 
		       &imp, sizeof(imp)) != 0) {
		printf("Client %u: Can't set socket options\n", client_id);
		perror(NULL);
		exit(1);
	}

	/* Try both ways of establishing a connection */

	if (client_id & 1) {
		dprintf("Client %u: sending setup ...\n", client_id);
		if (sendto(server_comm_sd, &server_num, 4, 0,
			   (struct sockaddr *)&server_addr,
			   sizeof(server_addr)) <= 0) {
			printf("Client %u: sending setup failed\n", client_id);
			perror(NULL);
			exit(1);
		}
	} else {
		dprintf("Client %u: doing connect ...\n", client_id);
		if (connect(server_comm_sd,
			    (struct sockaddr*)&server_addr,
			    sizeof(server_addr)) < 0) {
			printf("Client %u: connect failed\n", client_id);
			perror(NULL);
			exit(1);
		}
		if (send(server_comm_sd, &server_num, 4, 0) <= 0) {
			printf("Client %u: sending setup failed\n", client_id);
			perror(NULL);
			exit(1);
		}
	}

	dprintf("Client %u: getting ack from server ...\n", client_id);
	wait_err = wait_for_msg(server_comm_sd);
	if (wait_err) {
		printf("Client %u: No acknowledgement from server (err=%u)\n", 
		       client_id, wait_err);
		exit(1);
	}
	if (recv(server_comm_sd, &server_num, 4, 0) != 4) {
		printf("Client %u: Invalid acknowledgement from server\n",
		       client_id);
		perror(NULL);
		exit(1);
	}

	/* Notify client master that we're ready to run tests */

	client_master_addr.family = AF_TIPC;
	client_master_addr.addrtype = TIPC_ADDR_NAME;
	client_master_addr.addr.nameseq.type = MCLIENT_NAME;
	client_master_addr.addr.name.name.instance = 0;
	client_master_addr.addr.name.domain = 0;

	dprintf("Client %u: Notifying master of connection to server\n",
		client_id);
	server_num = ntohl(server_num);
	if (sendto(master_comm_sd, &server_num, 4, 0,
		   (struct sockaddr *)&client_master_addr,
		   sizeof(client_master_addr)) <= 0) {
		printf("Client %u: Unable to notify master\n", client_id);
		perror(NULL);
		exit(1);
	}

	/* Process commands from client master until told to shut down */

	for (;;) {

		/* Get next command from client master */

		wait_err = wait_for_msg(master_comm_sd);
		if (wait_err) {
			printf("Client %u: no command from master (err=%u)\n",
			       client_id, wait_err);
			exit(1);
		}
		sz = recv(master_comm_sd, &cmd, sizeof(cmd), 0);
		if (sz != sizeof(cmd)) {
			printf("Client %u: rxd illegal command\n", client_id);
			exit(1);
		}
		if (cmd.cmd == TERMINATE) {
			shutdown(server_comm_sd, SHUT_RDWR);
			close(server_comm_sd);
			close(master_comm_sd);
			dprintf("Client %u:last msg was %llu\n",
			        client_id, (init_cnt - counter));
			exit(0);
		}

		/* Execute command from client master */

		dprintf("Client %u: rec cmd: bounce %llu messages of size %llu\n",
		        client_id, cmd.msg_count, cmd.msg_size);
		init_cnt = counter = cmd.msg_count;
		burst = 0;

		do {
			unsigned long long msg_len;

			if (burst >= cmd.burst_size) {
				sched_yield();
				burst = 0;
			}

			msg_len = send(server_comm_sd, buf, cmd.msg_size, 0);
			if (msg_len != cmd.msg_size) {
				printf("Client %u: send failed\n", client_id);
				perror(NULL);
				exit(1);
			}
			
			wait_err = wait_for_msg(server_comm_sd);
			if (wait_err) {
				printf("Client %u: no response from server "
				       "(err=%u)\n", client_id, wait_err);
				exit(1);
			}
			
			msg_len = recv(server_comm_sd, buf, cmd.msg_size, 0);
			if (msg_len != cmd.msg_size) {
				printf("Client %u: invalid response from server,"
				       " (expected %lli, got %lli)\n",
				       client_id, cmd.msg_size, msg_len);
				perror(NULL);
				exit(1);
			}

			burst++;
		}
		while (counter--);

		/* Command is done, tell client master */

		dprintf("Client %u: reporting TASK_FINISHED to master\n", 
			client_id);
		cmd.client_no = client_id;
		if (sendto(master_comm_sd, &cmd, sizeof(cmd), 0, 
			   (struct sockaddr *)&client_master_addr,
			   sizeof(client_master_addr)) <= 0) {
			printf("Client %u: failed to send TASK_FINISHED msg\n",
			       client_id);
			perror(NULL);
			exit(1);
		}
	}
}

static void sig_alarm(int signo)
{
	printf("TIPC benchmark server timeout, exiting...\n");
	exit(0);
}

static uint own_node(int sd)
{
	struct sockaddr_tipc addr;
	socklen_t sz = sizeof(addr);

	if (getsockname(sd, (struct sockaddr *)&addr, &sz) < 0) {
		perror("Failed to get sock address\n");
		exit(1);
	}
	return addr.addr.id.node;
}

static void usage(char *app)
{
	fprintf(stderr, "Usage:\n");
	fprintf(stderr,
		"  %s [-l <lat mult>] [-t <tput mult>] [-n <num clients>]\n",
		app);
	fprintf(stderr, "\tlatency test multiplier defaults to 1\n");
	fprintf(stderr, "\tthroughput test multiplier defaults to 1\n");
	fprintf(stderr, "\tnumber of clients defaults to %d\n", DEFAULT_CLIENTS);
}

/*
 * Client master
 */

int main(int argc, char *argv[], char *dummy[])
{
	int c;
	int l_mult = 1;
	int t_mult = 1;
	int req_clients = DEFAULT_CLIENTS;

	__u32 server_node;
	int client_master_sd;
	struct sockaddr_tipc dest_addr;
	pid_t child_pid;
	unsigned long long num_clients;
	unsigned int client_id;
	unsigned int server_num;
	struct client_cmd cmd = {0,0,0,0,0};
	int wait_err;

	/* Process command line arguments */

	while ((c = getopt(argc, argv, "n:t:l:h")) != -1) {
		switch (c) {
		case 'l':
			l_mult = atoi(optarg);
			if (l_mult < 0) {
				fprintf(stderr, 
					"Invalid latency multiplier [%d]\n",
					l_mult);
				exit(1);
			}
			break;
		case 't':
			t_mult = atoi(optarg);
			if (t_mult < 0) {
				fprintf(stderr, 
					"Invalid throughput multiplier [%d]\n",
					t_mult);
				exit(1);
			}
			break;
		case 'n':
			req_clients = atoi(optarg);
			if (req_clients <= 0) {
				fprintf(stderr, "Invalid number of clients "
					"[%d]\n", req_clients);
				exit(1);
			}
			break;
		default:
			usage(argv[0]);
			return 1;
		}
	}

	/* Wait for benchmark server to appear */

	server_node = wait_for_server(SERVER_NAME, 0, MAX_DELAY);

	/* Create socket used to communicate with child clients */

	client_master_sd = socket(AF_TIPC, SOCK_RDM, 0);
	if (client_master_sd < 0) {
		printf("Client master: Can't create main client socket\n");
		perror(NULL);
		exit(1);
	}

	dest_addr.family = AF_TIPC;
	dest_addr.addrtype = TIPC_ADDR_NAME;
	dest_addr.scope = TIPC_NODE_SCOPE;
	dest_addr.addr.name.name.type = MCLIENT_NAME;
	dest_addr.addr.name.name.instance = 0;
	if (bind(client_master_sd, (struct sockaddr *)&dest_addr,
		 sizeof(dest_addr))) {
		printf("Client master: Failed to bind\n");
		perror(NULL);
		exit(1);
	}

	printf("****** TIPC benchmark client started ******\n");

	num_clients = 0;

	dest_addr.addr.name.name.type = CLIENT_NAME;
	dest_addr.addr.name.domain = 0;

	/* Optionally run latency test */

	if (!l_mult)
		goto end_latency;

	printf("Client master: Starting Latency Benchmark\n");

	/* Create first child client */

	child_pid = fork();
	if (child_pid < 0) {
		printf ("Client master: fork failed\n");
		perror(NULL);
		exit(1);
	}
	num_clients++;
	if (!child_pid) {
		close(client_master_sd);
		clientmain(num_clients);
		/* Note: child client never returns */
	}

	dprintf("Client master: waiting for confirmation from client 1\n");
	wait_err = wait_for_msg(client_master_sd);
	if (wait_err) {
		printf("Client master: no confirmation from client 1 (err=%u)\n", 
		       wait_err);
		exit(1);
	}
	if (recv(client_master_sd, buf, 4, 0) != 4) {
		printf ("Client master: confirmation failure from client 1\n");
		perror(NULL);
		exit(1);
	}
	server_num = *(unsigned int *)buf;

	dprintf("Client master: client 1 linked to server %i\n", server_num);

	/* Run latency test */

	cmd.msg_size = 64;
	cmd.msg_count = 10240 * l_mult;
	cmd.burst_size = cmd.msg_count;
	while (cmd.msg_size <= TIPC_MAX_USER_MSG_SIZE) {
		struct client_cmd ccmd;
		int sz;
		struct timeval start_time;
		unsigned long long elapsed;

		printf("Exchanging %llu messages of size %llu octets (burst size %llu)\n",
		       cmd.msg_count, cmd.msg_size, cmd.burst_size);
		dprintf("   client 1  <--> server %d\n", server_num);
		
		cmd.client_no = 1;
		dest_addr.addr.name.name.instance = 1;
		gettimeofday(&start_time, 0);

		if (sendto(client_master_sd, &cmd, sizeof(cmd), 0,
			   (struct sockaddr *)&dest_addr, sizeof(dest_addr))
		    != sizeof(cmd)) {
			printf("Client master: Can't send to client 1\n");
			perror(NULL);
			exit(1);
		}

		wait_err = wait_for_msg(client_master_sd);
		if (wait_err) {
			printf("Client master: No result from client 1 (err=%u)\n", 
			       wait_err);
			exit(1);
		}
		sz = recv(client_master_sd, &ccmd, sizeof(ccmd), 0);
		elapsed = elapsedmillis(&start_time);
		if (sz != sizeof(ccmd)) {
			printf("Client master: invalid result from client 1\n");
			perror(NULL);
			exit(1);
		}

		dprintf("Client master:rec cmd msg of size %i [%u:%llu:%llu:%u]\n",
		        sz,ccmd.cmd,ccmd.msg_size,ccmd.msg_count,ccmd.client_no);
		dprintf("Client master: received TASK_FINISHED from client 1\n");

		printf("... took %llu ms (round-trip avg/msg: %llu us)\n",
		       elapsed, (elapsed * 1000)/cmd.msg_count);

		cmd.msg_size *= 4;
		cmd.msg_count /= 4;
		cmd.burst_size /= 4;
	}

	printf("Client master: Completed Latency Benchmark\n");

end_latency:

	/* Optionally run throughput test */

	if (!t_mult)
		goto end_thruput;
	
	printf("Client master: Starting Throughput Benchmark\n");

	/* Create remaining child clients */

	while (num_clients < req_clients) {
		int sz;

		child_pid = fork();
		if (child_pid < 0) {
			printf ("Client master: fork failed\n");
			perror(NULL);
			exit(1);
		}
		num_clients++;
		if (!child_pid) {
			close(client_master_sd);
			clientmain(num_clients);
			/* Note: child client never returns */
		}

		dprintf ("Client master: waiting for confirmation "
			 "from client %llu\n", num_clients);
		wait_err = wait_for_msg(client_master_sd);
		if (wait_err) {
			printf("Client master: no confirmation from client %llu "
			       "(err=%u)\n", num_clients, wait_err);
			exit(1);
		}
		sz = recv(client_master_sd, buf, 4, 0);
		if (sz != 4) {
			printf("Client master: confirmation failure "
			       "from client_id %llu\n", num_clients);
			exit(1);
		}
		server_num = *(unsigned int*)buf;

		dprintf("Client master: client %llu linked to server %i\n",
		        num_clients, server_num);

	}
	dprintf("Client master: all clients and servers started\n");
	sleep(2);   /* let console printfs flush before continuing */

	/* Get child clients to run throughput test */

	cmd.msg_size = 64;
	cmd.msg_count = 10240 * t_mult;
	cmd.burst_size = 10240/5;
	while (cmd.msg_size < TIPC_MAX_USER_MSG_SIZE) {
		struct timeval start_time;
		unsigned long long elapsed;
		unsigned long long msg_per_sec;
		unsigned long long procs;

		printf("Exchanging %llu*%llu messages of size %llu octets (burst size %llu)\n",
		       num_clients, cmd.msg_count, cmd.msg_size, cmd.burst_size);

		gettimeofday(&start_time, 0);

		for (client_id = 1; client_id <= num_clients; client_id++) {
			cmd.client_no = client_id;
			dest_addr.addr.name.name.instance = client_id;
			if (sendto(client_master_sd, &cmd, sizeof(cmd), 0,
				   (struct sockaddr *)&dest_addr,
				   sizeof(dest_addr)) != sizeof(cmd)) {
				printf("Client master: can't send to client %u\n",
				       client_id);
				perror(NULL);
				exit(1);
			}
		}

		for (client_id = 1; client_id <= num_clients; client_id++) {
			struct client_cmd report;
			int sz;
			 
			wait_err = wait_for_msg(client_master_sd);
			if (wait_err) {
				printf("Client master: result %u not received "
				       "(err=%u)\n", client_id, wait_err);
				exit(1);
			}
			sz = recv(client_master_sd, &report, sizeof(report), 0);
			if (sz != sizeof(report)) {
				printf("Client master: result %u invalid\n",
				       client_id);
				perror(NULL);
				exit(1);
			}
			dprintf("Client master: received TASK_FINISHED "
				"from client %u\n", report.client_no);
		}

		elapsed = elapsedmillis(&start_time);
		msg_per_sec = (cmd.msg_count * num_clients * 1000)/elapsed;
		procs = 1 + (server_node != own_node(client_master_sd));
		printf("... took %llu ms "
		       "(avg %llu msg/s/dir, %llu bits/s/dir)\n", 
		       elapsed, msg_per_sec/2, msg_per_sec*cmd.msg_size*8/2);
		printf("    avg execution time (send+receive) %llu us/msg\n",
		       (1000000 / (msg_per_sec * 2)) * procs);

		cmd.msg_size *= 4;
		cmd.msg_count /= 4;
		cmd.burst_size /= 4;
	}

	printf("Client master: Completed Throughput Benchmark\n");

end_thruput:

	/* Terminate all client processes */

	cmd.cmd = TERMINATE;
	for (client_id = 1; client_id <= num_clients; client_id++) {
		dest_addr.addr.name.name.instance = client_id;
		if (sendto(client_master_sd, &cmd, sizeof(cmd), 0,
			   (struct sockaddr *)&dest_addr,
			   sizeof(dest_addr)) <= 0) {
			printf("Client master: failed to send TERMINATE message"
			       " to client %u\n",
			       client_id);
			perror(NULL);
			exit(1);
		}
	}
	if (signal(SIGALRM, sig_alarm) == SIG_ERR) {
		printf("Client master: Can't catch alarm signals\n");
		perror(NULL);
		exit(1);
	}
	alarm(MAX_DELAY);
	for (client_id = 1; client_id <= num_clients; client_id++) {
		if (wait(NULL) <= 0) {
			printf("Client master: error during termination\n");
			perror(NULL);
			exit(1);
		}
	}

	printf("****** TIPC benchmark client finished ******\n");
	shutdown(client_master_sd, SHUT_RDWR);
	close(client_master_sd);
	exit(0);
}

