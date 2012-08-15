/* ------------------------------------------------------------------------
 *
 * server_tipc.c
 *
 * Short description: TIPC benchmark demo (server side)
 *
 * ------------------------------------------------------------------------
 *
 * Copyright (c) 2001-2005, Ericsson Research Canada
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

#include <getopt.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <linux/tipc.h>

#define SERVER_NAME  17777

#define DEBUG 0

#define dprintf(fmt, arg...)  do {if (DEBUG) printf(fmt, ## arg);} while(0)

static char buf[TIPC_MAX_USER_MSG_SIZE];

static int server_timeout = 30;		/* inactivity limit [in sec] */

static void sig_alarm(int signo)
{
	printf("TIPC benchmark server timeout, exiting...\n");
	printf("****** TIPC benchmark server finished ******\n");
	exit(0);
}

static void usage(char *app)
{
	fprintf(stderr, "Usage:\n");
	fprintf(stderr, "  %s [-i <idle timeout>]\n", app);
	fprintf(stderr, "\tidle timeout defaults to %d (seconds)\n",
	        server_timeout);
}

int main(int argc, char *argv[], char *dummy[])
{
	int c;
	int listener_sd;
	struct sockaddr_tipc server_addr;

	unsigned int acceptno = 0;
	int child_count = 0;
	int peer_sd;
	pid_t child_pid;
	fd_set fds;
	struct timeval tv;
	__u32 server_num;
	int r;

	int imp = TIPC_MEDIUM_IMPORTANCE;
	struct pollfd pfd;
	int pollres;
	int msglen = 0;
	unsigned int msg_count = 0;

	/* Process command line arguments */

	while ((c = getopt(argc, argv, "i:h")) != -1) {
		switch (c) {
		case 'i':
			server_timeout = atoi(optarg);
			if (server_timeout <= 0) {
				fprintf(stderr,
				        "Invalid idle timeout [%d]\n",
				        server_timeout);
				exit(1);
			}
			break;
		default:
			usage(argv[0]);
			return 1;
		}
	}

	if (signal(SIGALRM, sig_alarm) == SIG_ERR) {
		printf("Server master: can't catch alarm signals\n");
		perror(NULL);
		exit(1);
	}

	/* Create listening socket for answering calls from clients */

	listener_sd = socket (AF_TIPC, SOCK_SEQPACKET,0);
	if (listener_sd < 0) {
		printf("Server master: can't create listening socket\n");
		perror(NULL);
		exit(1);
	}

	server_addr.family = AF_TIPC;
	server_addr.addrtype = TIPC_ADDR_NAMESEQ;
	server_addr.scope = TIPC_ZONE_SCOPE;
	server_addr.addr.nameseq.type = SERVER_NAME;
	server_addr.addr.nameseq.lower = 0;
	server_addr.addr.nameseq.upper = 0;
	if (bind(listener_sd, (struct sockaddr *)&server_addr,
	                sizeof(server_addr)) < 0) {
		printf("Server master: failed to bind port name\n");
		perror(NULL);
		exit(1);
	}
	if (listen(listener_sd, 5) < 0) {
		printf("Server master: failed to listen for connections\n");
		perror(NULL);
		exit(1);
	}

	printf("****** TIPC benchmark server started ******\n");

	/* Accept connections until idle too long with no active children */

	alarm(server_timeout);

	while (1) {

		/* Try to accept another client connection */

		FD_ZERO(&fds);
		FD_SET(listener_sd, &fds);
		tv.tv_sec = 5;
		tv.tv_usec = 0;

		r = select(listener_sd + 1, &fds, 0, 0, &tv);
		if (r > 0 && FD_ISSET(listener_sd, &fds)) {
			peer_sd = accept(listener_sd, 0, 0);
			if (peer_sd < 0 ) {
				printf ("Server master: accept failed\n");
				perror(NULL);
				exit(1);
			}

			alarm(0);
			acceptno++;
			child_count++;

			fflush(stdout);
			child_pid = fork();
			if (child_pid < 0) {
				printf ("Server master: fork failed\n");
				perror(NULL);
				exit(1);
			}
			if (!child_pid)
				break;	/* Child process exits loop */

			close(peer_sd);
		}

		/* Terminate zombie children; start timer if no children left */

		while (waitpid(0, NULL, WNOHANG) > 0) {
			if (--child_count == 0)
				alarm(server_timeout);
		}
	}

	/* Child server gets ready to handle client requests */

	dprintf("Server process %u created\n", acceptno);
	close(listener_sd);

	setsockopt(peer_sd, SOL_TIPC, TIPC_IMPORTANCE, &imp, sizeof(imp));

	pfd.fd = peer_sd;
	pfd.events = 0xffff & ~POLLOUT;
	pollres = poll(&pfd, 1, server_timeout * 1000);
	if (pollres <= 0) {
		printf("Server %u: no setup msg after %u sec\n",
		       acceptno, server_timeout);
		exit(1);
	}
	if (recv(peer_sd, buf, TIPC_MAX_USER_MSG_SIZE, 0) <= 0) {
		printf("Server %u: unable to receive setup msg\n", acceptno);
		perror(NULL);
		exit(1);
	}
	server_num = htonl(acceptno);
	if (send(peer_sd, &server_num, 4, 0) <= 0) {
		printf("Server %u: unable to respond to setup msg\n", acceptno);
		perror(NULL);
		exit(1);
	}

	/* Child server echoes client messages until connection is closed */

	while (1) {
		pollres = poll(&pfd, 1, server_timeout * 1000);
		if (pollres < 0) {
			printf("Server %u: poll() error "
			       "(pollres=%x, revents=%x, msglen=%u)\n",
			       acceptno, pollres, pfd.revents, msglen);
			exit(1);
		} else if (pollres == 0) {
			printf("Server %u: no client msg after %u sec "
			       "(pollres=%x, revents=%x, msglen=%u)\n",
			       acceptno, server_timeout,
			       pollres, pfd.revents, msglen);
			exit(1);
		}
		if (!(pfd.revents & POLLIN)) {
			printf("Server %u: unexpected event (revents=%x)\n",
			       acceptno, pfd.revents);
			exit(1);
		}

		msglen = recv(peer_sd, buf, TIPC_MAX_USER_MSG_SIZE, 0);
		if (msglen < 0) {
			printf("Server %u: recv failed (pollres=%x)\n",
			       acceptno, pollres);
			perror(NULL);
			exit(1);
		} else if (msglen == 0) {
			dprintf("Server %u: normal conn shutdown\n", acceptno);
			break;
		}

		if (!(++msg_count % 50000)) {
			dprintf("Server %u received %u messages\n",
			        acceptno, msg_count);
		}

		if (send(peer_sd, buf, msglen, 0) != msglen) {
			printf("Server %u: send %u failed (msg len = %u)\n",
			       acceptno, msg_count, msglen);
			perror(NULL);
			exit(1);
		}

	}

	shutdown(peer_sd, SHUT_RDWR);
	close(peer_sd);
	exit(0);
}

