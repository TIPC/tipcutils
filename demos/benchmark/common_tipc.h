/* ------------------------------------------------------------------------
 *
 * common_tipc.h
 *
 * Short description: TIPC benchmark demo (common definitions and functions)
 *
 * ------------------------------------------------------------------------
 *
 * Copyright (c) 2014, Ericsson AB
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

#ifndef __COMMON_TIPC
#define __COMMON_TIPC

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
#include <arpa/inet.h>  
#include <netdb.h>
#include <sys/ioctl.h>
#include <net/if.h>

#define MAX_DELAY       300000		/* inactivity limit [in ms] */
#define MASTER_NAME     16666
#define SRV_CTRL_NAME   17777
#define SRV_LSTN_NAME   18888
#define CLNT_CTRL_NAME  19999

#define TERMINATE 1
#define DEFAULT_CLIENTS 8

#define DEBUG 0

#define dprintf(fmt, arg...)  do {if (DEBUG) printf(fmt, ## arg);} while(0)
#define die(fmt, arg...)  \
	do { \
            printf(fmt": ", ## arg); \
            perror(NULL); \
            exit(1);\
        } while(0)

static int wait_for_msg(int sd);	  

struct srv_cmd {
	__u32 cmd;
	__u32 msglen;
};

static const struct sockaddr_tipc master_srv_addr = {
	.family                  = AF_TIPC,
	.addrtype                = TIPC_ADDR_NAME,
	.addr.name.name.type     = MASTER_NAME,
	.addr.name.name.instance = 0,
	.scope                   = TIPC_ZONE_SCOPE,
	.addr.name.domain        = 0
};

static const struct sockaddr_tipc srv_ctrl_addr = {
	.family                  = AF_TIPC,
	.addrtype                = TIPC_ADDR_NAMESEQ,
	.addr.nameseq.type       = SRV_CTRL_NAME,
	.addr.nameseq.lower      = 0,
	.addr.nameseq.upper      = 0,
	.scope                   = TIPC_ZONE_SCOPE
};

static const struct sockaddr_tipc srv_lstn_addr = {
	.family                  = AF_TIPC,
	.addrtype                = TIPC_ADDR_NAME,
	.addr.name.name.type     = SRV_LSTN_NAME,
	.addr.name.name.instance = 0,
	.scope                   = TIPC_ZONE_SCOPE,
	.addr.name.domain        = 0
};

static int master_sd;

struct srv_info {
	__u16 tcp_port;
	__u16 num_ips;
	__u32 ips[16];
};

#define SRV_INFO         0
#define SRV_MSGLEN_ACK   1
#define SRV_FINISHED     2
struct srv_to_master_cmd {
	__u32 cmd;
	__u32 tipc_addr;
	struct srv_info sinfo;
};

#define TIPC_CONN         0
#define TCP_CONN          1
#define RCV_MSG_LEN       2
#define RESTART           3
struct master_srv_cmd {
	__u32 cmd;
	__u32 msglen;
	__u32 msgcnt;
	__u32 echo;
};

static void sig_alarm(int signo)
{
	printf("TIPC benchmark timeout, exiting...\n");
	exit(0);
}

__u32 wait_for_name(__u32 name_type, __u32 name_instance, int wait)
{
	struct sockaddr_tipc topsrv;
	struct tipc_subscr subscr;
	struct tipc_event event;

	int sd = socket(AF_TIPC, SOCK_STREAM, 0);

	memset(&topsrv, 0, sizeof(topsrv));
	topsrv.family = AF_TIPC;
	topsrv.addrtype = TIPC_ADDR_NAME;
	topsrv.addr.name.name.type = TIPC_TOP_SRV;
	topsrv.addr.name.name.instance = TIPC_TOP_SRV;

	/* Connect to topology server */

	if (0 > connect(sd, (struct sockaddr *)&topsrv, sizeof(topsrv)))
		die("Client: failed to connect to topology server");

	subscr.seq.type = htonl(name_type);
	subscr.seq.lower = htonl(name_instance);
	subscr.seq.upper = htonl(name_instance);
	subscr.timeout = htonl(wait);
	subscr.filter = htonl(TIPC_SUB_SERVICE);

	if (send(sd, &subscr, sizeof(subscr), 0) != sizeof(subscr))
		die("Client: failed to send subscription");

	/* Now wait for the subscription to fire */

	if (recv(sd, &event, sizeof(event), 0) != sizeof(event))
		die("Client: failed to receive event");

	if (event.event != htonl(TIPC_PUBLISHED))
		die("Client: server {%u,%u} not published within %u [s]\n",
		       name_type, name_instance, wait/1000);

	close(sd);

	return ntohl(event.port.node);
}

static int wait_for_msg(int sd)
{
	struct pollfd pfd;
	int pollres;
	int res;

	pfd.events = ~(POLLOUT | POLLWRNORM | POLLRDNORM);
	pfd.fd = sd;
	pollres = poll(&pfd, 1, MAX_DELAY);
	if (pollres < 0)
		res = -1;
	else if (pollres == 0)
		res = -2;
	else if (pfd.revents == POLLIN)
		res = 0;
	else{
		dprintf("revents: %x\n", pfd.revents);
		res = (pfd.revents & POLLIN ) ? 0 : pfd.revents;
	}
	return res;
}

static void get_ip_list(struct srv_info *sinfo)
{
	char buf[8192] = {0};
	struct ifconf ifc = {0};
	struct ifreq *ifr = NULL;
	int sck = 0;
	int num_ifs = 0;
	int ip_no = 0;
	int i = 0;
	struct ifreq *item;
	struct sockaddr *addr;
	int ip;

	sck = socket(PF_INET, SOCK_DGRAM, 0);
	if (sck <= 0)
		die("Failed to create socket\n");
	ifc.ifc_len = sizeof(buf);
	ifc.ifc_buf = buf;
	if(ioctl(sck, SIOCGIFCONF, &ifc) < 0)
		die("Failed to obtain IP list\n");
	ifr = ifc.ifc_req;
	num_ifs = ifc.ifc_len / sizeof(struct ifreq);
	
	for(i = 0; i < num_ifs; i++) 
	{
		item = &ifr[i];
		addr = &(item->ifr_addr);
		if(ioctl(sck, SIOCGIFADDR, item) < 0)
			perror("ioctl(OSIOCGIFADDR)");
		ip = ((struct sockaddr_in *)addr)->sin_addr.s_addr;

		/* Register if set and not loopback */
		if (ip && (ntohl(ip) != 0x7f000001)) {
			sinfo->ips[ip_no] = ip;
			sinfo->num_ips = htons(++ip_no);
		}
	}
}

static uint own_node(void)
{
	struct sockaddr_tipc addr;
	socklen_t sz = sizeof(addr);
	int sd;

	sd = socket(AF_TIPC, SOCK_RDM, 0);
	if (sd < 0)
		die("Master: Can't create client ctrl socket\n");

	if (getsockname(sd, (struct sockaddr *)&addr, &sz) < 0)
		die("Failed to get sock address\n");

	close(sd);
	return addr.addr.id.node;
}

#if DEBUG

static void print_peer_name(int s)
{
	struct sockaddr_in peer;
	unsigned int peer_len;
	
	peer_len = sizeof(peer);
	/* Ask getpeername to fill in peer's socket address.  */
	if (getpeername(s, (struct sockaddr*) &peer, &peer_len) == -1)
		die("getpeername() failed");

	printf("Peer's IP address is: %s\n", inet_ntoa(peer.sin_addr));
	printf("Peer's port is: %d\n", (int) ntohs(peer.sin_port));
}

static void print_sock_name(int s)
{
	struct sockaddr_in sa;
	unsigned int sa_len;
	
	sa_len = sizeof(sa);
	
	if (getsockname(s, (struct sockaddr*)&sa, &sa_len) == -1) {
		perror("getsockname() failed");
		return;
	}
	printf("Local IP address is: %s\n", inet_ntoa(sa.sin_addr));
	printf("Local port is: %d\n", (int) ntohs(sa.sin_port));
}


#endif
#endif
