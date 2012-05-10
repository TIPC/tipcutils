/* ------------------------------------------------------------------------
 *
 * tipc-pipe.c
 *
 * Short description: TIPC CLI utility, similar to netcat.
 *
 * ------------------------------------------------------------------------
 *
 * Written by Constantine Shulyupin const@makelinux.com for Compass EOS
 *
 * Copyright (c) 2012, Compass EOS Ltd http://compass-eos.com/
 *
 * A few code is borrowed from tipc demos by
 * Copyright (c) 2005,2010 Wind River Systems
 * Copyright (c) 2003, Ericsson Research Canada
 *
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

#define _XOPEN_SOURCE 500
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <string.h>
#include <sys/param.h>
#include <sys/poll.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <assert.h>
#include <arpa/inet.h>

#include <linux/tipc.h>

#define TRACE_ON

#ifdef TRACE_ON
#define chkne(a) \
( ret = (a),\
       ((ret<0)?fprintf(stderr,"%s:%i %s FAIL errno = %i \"%s\" %i = %s\n",\
       __FILE__,__LINE__,__FUNCTION__,errno,strerror(errno),ret,#a)\
        :0),\
       ret)
#define trvd_(d) fprintf(stderr,#d" = %d ",(int)d)
#define trvx_(d) fprintf(stderr,#d" = %x ",(int)d)
#define trln() fprintf(stderr,"\n");
#define trl() fprintf(stderr,"%s:%i %s\n",__FILE__,__LINE__,__FUNCTION__)
#define trl_() fprintf(stderr,"%s:%i %s ",__FILE__,__LINE__,__FUNCTION__)
#else
#define chkne(a) ret=(a)
#define trvd_(d)
#define trvx_(d)
#define trln()
#define trl()
#define trl_()
#endif

int sock_type = SOCK_STREAM;
int addr_type = TIPC_ADDR_NAME;        /* Note: TIPC_ADDR_MCAST == TIPC_ADDR_NAMESEQ */
int server_type = 1000;                /* should be bigger than TIPC_RESERVED_TYPES */
int delay = 0;
int data_num = 0;
int data_check = 0;
int data_size = 0;
int wait_peer = 0;
int replay = 0;
struct sockaddr_tipc addr_sk;
__thread int ret;
int addr1 = 0, addr2 = 0;
int recvq_depth = 0;
struct sockaddr_tipc name;
int buf_size = TIPC_MAX_USER_MSG_SIZE;
void *buf;
socklen_t addr_size = sizeof(struct sockaddr_tipc);

static enum client_mode_e {
       data_client_e,
       single_listener,
       multi_server,
       topology_client,
} mode;

/*
 * tipc_write - unified write, works with connected or connectionless socket.
 */

int tipc_write(int tipc, void *buf, int len)
{
       switch (sock_type) {
       case SOCK_DGRAM:
       case SOCK_RDM:
               len = sendto(tipc, buf, len, MSG_DONTWAIT, (void *)&addr_sk, sizeof(addr_sk));
               break;
       default:
               len = write(tipc, buf, len);
       }
       return len;
}

/*
 * generate_data - generates data for testing
 *
 * Generated data is checked by function check_generated_data
 */

int generate_data(int tipc, int data_num)
{
       int i;
       int eagin_stat = 0;
       int len_total = 0;

       for (i = 0; i < data_num; i++) {
               int try = 0;
               if ((i % 10) == 0) {
                       trl_();
                       trvd_(eagin_stat);
                       trvd_(i);
                       trln();
               }
             again:
               try++;
               if (data_size) {
                       sprintf(buf, "%0*d\n", data_size - 2, 0);
               } else {
                       sprintf(buf, "message %d try %d %x %d %x\n", i, try, name.addr.id.node, getpid(),
                               name.addr.id.ref);
               }
               ret = tipc_write(tipc, buf, strlen(buf) + 1);
               if (ret < 0 && errno == EAGAIN) {
                       eagin_stat++;
                       usleep(100000);
                       goto again;
               }
               if (ret < 0) {
                       perror(__FUNCTION__);
                       break;
               }
               if (ret > 0)
                       len_total += ret;
               trl_();
               trvd_(i);
               trvd_(len_total);
               trvd_(ret);
               trln();
               usleep(1000 * delay);
       }
       return ret;
}

/*
 * check_generated_data - checks data generated by function generate_data
 */

int check_generated_data(int tipc)
{
       /* map of expected sequence numbers for each sender */
       int i[255] = { 0, };
       ssize_t len;
       struct sockaddr_tipc peer;

       while (1) {
               int seq = 0;
               chkne(len = recvfrom(tipc, buf, buf_size, 0, (void *)&peer, &addr_size));
               if (len <= 0)
                       break;
               if (0 < sscanf(buf, "message %d", &seq)) {
                       if (seq - i[peer.addr.id.ref % 256]) {
                               fprintf(stderr, "#%d %d lost on %x\n", i[peer.addr.id.ref % 256],
                                       seq - i[peer.addr.id.ref % 256], peer.addr.id.ref);
                       }
                       i[peer.addr.id.ref % 256] = seq;
               }
               write(fileno(stdout), buf, len);
               i[peer.addr.id.ref % 256]++;
               usleep(1000 * delay);
       }
       trl();
       return len;
}

/*
 * pipe_start - sends data from stdin to TIPC socket, and data from TIPC socket to stdout
 */

int pipe_start(int tipc)
{
       struct pollfd pfd[2];
       struct sockaddr_tipc peer;
       ssize_t len, data_in_len, len_total = 0;
       int i = 0;

       trl();
       pfd[0].fd = fileno(stdin);
       pfd[0].events = POLLIN;
       pfd[1].fd = tipc;
       pfd[1].events = POLLIN;
       /* Note: when zero length data received, transfer it and exit
        */
       while (poll(pfd, sizeof(pfd) / sizeof(pfd[0]), -1) > 0) {
               data_in_len = 0;
               if (pfd[0].revents & POLLIN) {
                       len = data_in_len = read(fileno(stdin), buf, buf_size);
#if VERBOSE
                       trvd_(data_in_len);
                       trln();
#endif
                       if (data_in_len < 0)
                               break;
                     again:
                       chkne(len = tipc_write(tipc, buf, data_in_len));
                       if (len < 0 && errno == EAGAIN) {
                               usleep(100000);
                               goto again;
                       }
               }
               if (pfd[1].revents & POLLIN) {
                       chkne(len = data_in_len = recvfrom(tipc, buf, buf_size, 0, (void *)&peer, &addr_size));
                       if (replay) {
                               addr_sk = peer;
                       }
                       if (data_in_len < 0)
                               break;
                       write(fileno(stdout), buf, data_in_len);
               }
               if (data_in_len > 0)
                       len_total += data_in_len;
#if VERBOSE
               trl_();
               trvd_(i);
               trvd_(len_total);
               trvd_(data_in_len);
               trln();
#endif
               i++;
               if (pfd[0].revents & POLLHUP || pfd[1].revents & POLLHUP && !data_in_len)
                       break;
               usleep(1000 * delay);
       }
       return len;
}

#ifndef TIPC_SOCK_RECVQ_MAX_DEPTH
#define TIPC_SOCK_RECVQ_MAX_DEPTH    133
#endif
/*
 * data_io - perform data excahge accordingly configuration
 *
 * Options:
 * - perform stdin, stdout and socket I/O
 * - generate data
 * - check generated data
 */

int data_io(int tipc)
{
       if (recvq_depth) {
               /* this is custom parameter, not yet implemented in mainstream source */
               setsockopt(tipc, SOL_TIPC, TIPC_SOCK_RECVQ_MAX_DEPTH, &recvq_depth, sizeof(recvq_depth));
       }
       if (data_num)
               ret = generate_data(tipc, data_num);
       else if (data_check)
               ret = check_generated_data(tipc);
       else
               ret = pipe_start(tipc);
       return ret;
}

/*
 * listen_accept_and_io - performs servers side connection based operations
 *
 */

int listen_accept_and_io(int tipc)
{
       int peer_sd;
       trl();
       trvd_(mode);
       trln();
       ret = 0;
       chkne(listen(tipc, 0));
      again:
       switch (mode) {
       case single_listener:
               chkne(peer_sd = accept(tipc, 0, 0));
               ret = data_io(peer_sd);
               shutdown(peer_sd, SHUT_RDWR);
               close(peer_sd);
               break;
       case multi_server:
               chkne(peer_sd = accept(tipc, 0, 0));
               if (!fork()) {
                       ret = data_io(peer_sd);
                       shutdown(peer_sd, SHUT_RDWR);
                       close(peer_sd);
                       exit(0);
               }
               goto again;
       }
       return ret;
}

/* tipc_addr_set - utility function to fill struct sockaddr_tipc
 */

void tipc_addr_set(struct sockaddr_tipc *A, int addr_type, int server_type, int a1, int a2)
{
       memset(A, 0, sizeof(*A));
       A->family = AF_TIPC;
       A->scope = TIPC_CLUSTER_SCOPE;
       A->addrtype = addr_type;
       switch (addr_type) {
       case TIPC_ADDR_MCAST:
               A->addr.nameseq.type = server_type;
               A->addr.nameseq.lower = a1;
               A->addr.nameseq.upper = a2;
               break;
       case TIPC_ADDR_NAME:
               A->addr.name.name.type = server_type;
               A->addr.name.domain = 0;
               A->addr.name.name.instance = a1;
               break;
       case TIPC_ADDR_ID:
               A->addr.id.node = a1;
               A->addr.id.ref = a2;
               break;
       }
}

#define add_literal_option(o)  do { options[optnum].name = #o; \
       options[optnum].flag = &o; options[optnum].has_arg = 1; \
       options[optnum].val = -1; optnum++; } while (0)

#define add_flag_option(n,p,v) do { options[optnum].name = n; \
       options[optnum].flag = (int*)p; options[optnum].has_arg = 0; \
       options[optnum].val = v; optnum++; } while (0)

static struct option options[100];
int optnum;

int options_init()
{
       optnum = 0;
       /* on gcc 64, pointer to variable can be used only on run-time
        */
       memset(options, 0, sizeof(options));
       add_literal_option(sock_type);
       add_literal_option(server_type);
       add_literal_option(addr_type);
       add_literal_option(delay);
       add_literal_option(data_num);
       add_literal_option(buf_size);
       add_literal_option(data_size);
       add_literal_option(wait_peer);
       add_literal_option(recvq_depth);
       add_flag_option("rdm", &sock_type, SOCK_RDM);
       add_flag_option("pct", &sock_type, SOCK_PACKET);
       add_flag_option("stm", &sock_type, SOCK_STREAM);
       add_flag_option("sqp", &sock_type, SOCK_SEQPACKET);
       add_flag_option("mc", &addr_type, TIPC_ADDR_MCAST);
       add_flag_option("nam", &addr_type, TIPC_ADDR_NAME);
       add_flag_option("top", &mode, topology_client);
       add_flag_option("id", &addr_type, TIPC_ADDR_ID);
       add_flag_option("data_check", &data_check, 1);
       add_flag_option("replay", &replay, 1);
       options[optnum].name = strdup("help");
       options[optnum].has_arg = 0;
       options[optnum].val = 'h';
       optnum++;
       return optnum;
}

/* expand_arg, return_if_arg_is_equal - utility functions to translate command line parameters
 * from string to numeric values using predefined preprocessor defines
 */

#define return_if_arg_is_equal(entry) if (0 == strcmp(arg,#entry)) return entry

int expand_arg(char *arg)
{
       if (!arg)
               return 0;
       return_if_arg_is_equal(SOCK_STREAM);
       return_if_arg_is_equal(SOCK_DGRAM);
       return_if_arg_is_equal(SOCK_RDM);
       return_if_arg_is_equal(SOCK_SEQPACKET);

       return_if_arg_is_equal(TIPC_ADDR_NAMESEQ);
       return_if_arg_is_equal(TIPC_ADDR_MCAST);
       return_if_arg_is_equal(TIPC_ADDR_NAME);
       return_if_arg_is_equal(TIPC_ADDR_ID);
       return atoi(arg);
}

char *usage = "Usage:\n\
       tipc-pipc <options> [address 1] [address 2]\n\
\n\
default address 1 is 0\n\
default address 2 is same as address 1\n\
\n\
options:\n\
\n\
default values are marked with '*'\n\
\n\
       -h | --help\n\
               show this help\n\
\n\
       -l\n\
               run in server mode, accept multiple connections\n\
       -s\n\
               run in single connection server mode, exit on connection close\n\
\n\
       default mode is client mode\n\
\n\
       --sock_type *SOCK_STREAM | SOCK_DGRAM | SOCK_RDM | SOCK_SEQPACKET\n\
\n\
       --server_type *1000|<n>\n\
\n\
       --addr_type TIPC_ADDR_NAMESEQ | TIPC_ADDR_MCAST | *TIPC_ADDR_NAME\n\
               For TIPC_ADDR_NAME only address 1 is used.\n\
               For TIPC_ADDR_NAMESEQ or TIPC_ADDR_MCAST\n\
               address 1 and address 2 are used.\n\
\n\
       --delay *0|<ms>\n\
               Defines data reading and writing delay in ms.\n\
\n\
       --data_num *0|<count>\n\
               Generates defined number of sample data and sends is.\n\
\n\
       --data_size *0\n\
               Generates packets of defined size when data_num is defined.\n\
\n\
       --data_check\n\
               Check sequence numbers in received data,\n\
               generated with option data_num.\n\
\n\
       --buf_size *66000|<n> \n\
               I/O buffer size (see TIPC_MAX_USER_MSG_SIZE).\n\
       --wait_peer *0\n\
               Wait for peer published state before communication.\n\
       --top\n\
               run topology client\n\
       --replay\n\
               force connectionless server send input to last connected client \n\
\n\
shortcuts:\n\
\n\
       --rdm\n\
               sock_type = SOCK_RDM\n\
       --pct\n\
               sock_type = SOCK_PACKET\n\
       --stm\n\
               * sock_type = SOCK_STREAM\n\
       --sqp\n\
               sock_type = SOCK_SEQPACKET\n\
       --nam\n\
               * addr_type = TIPC_ADDR_NAME\n\
       --mc\n\
               addr_type = TIPC_ADDR_MCAST or TIPC_ADDR_NAMESEQ\n\
       --id\n\
               addr_type = TIPC_ADDR_ID\n\
Samples:\n\
\n\
SOCK_STREAM single connection server with address zero and client:\n\
\n\
       tipc-pipe -s | tee input\n\
\n\
       date | tipc-pipe\n\
\n\
SOCK_RDM server with address 123 and client:\n\
\n\
       tipc-pipe --rdm --replay -l 123\n\
\n\
       tipc-pipe --rdm 123\n\
\n\
Start topology client for all addresses of specified optional server type\n\
\n\
tipc-pipe --server_type=1000 --top -- 0 -1\n\
\n\
";

int init(int argc, char *argv[])
{
       int opt = 0;
       int longindex = 0;
       options_init();
       opterr = 0;
       while ((opt = getopt_long(argc, argv, "hsl", options, &longindex)) != -1) {
               switch (opt) {
               case 0:
                       if (options[longindex].val == -1)
                               *options[longindex].flag = expand_arg(optarg);
                       break;
               case 'h':
                       printf("%s", usage);
                       exit(0);
                       break;
               case 's':
                       mode = single_listener;
                       break;
               case 'l':
                       mode = multi_server;
                       break;
               default:        /* '?' */
                       printf("Error in arguments\n");
                       exit(EXIT_FAILURE);
               }
       }
       if (optind < argc) {
               addr1 = addr2 = atoi(argv[optind]);
       }
       if (optind + 1 < argc) {
               addr2 = atoi(argv[optind + 1]);
       }
       trvd_(sock_type);
       trvd_(server_type);
       trvd_(addr_type);
       trvd_(addr1);
       trvd_(addr2);
       trvd_(delay);
       trvd_(data_num);
       trvd_(buf_size);
       trvd_(data_check);
       trln();
       assert(data_size + 1 < buf_size);
       return 0;
}

int run_server(tipc)
{
       trl();
       chkne(bind(tipc, (void *)&addr_sk, sizeof(addr_sk)));
       switch (sock_type) {
       case SOCK_SEQPACKET:
       case SOCK_STREAM:
               ret = listen_accept_and_io(tipc);
               break;
       default:
               ret = data_io(tipc);
       }
       trl();
       return ret;
}

int run_client(int tipc)
{
       trl();
       switch (sock_type) {
       case SOCK_SEQPACKET:
       case SOCK_STREAM:
               chkne(connect(tipc, (void *)&addr_sk, sizeof(addr_sk)));
               break;
       }
       ret = data_io(tipc);
       return ret;
}

/*
 * wait_for_server - wait for TIPC server presence on network.
 * Used for synchronization client with server.
 *
 * returns TIPC_PUBLISHED or TIPC_WITHDRAWN or TIPC_SUBSCR_TIMEOUT
 *
 */

int wait_for_server(__u32 name_instance, int wait)
{
       struct sockaddr_tipc topsrv;
       struct tipc_subscr subscr;
       struct tipc_event event;

       int sd = socket(AF_TIPC, SOCK_SEQPACKET, 0);
       tipc_addr_set(&topsrv, TIPC_ADDR_NAME, TIPC_TOP_SRV, TIPC_TOP_SRV, 0);
       chkne(connect(sd, (void *)&topsrv, sizeof(topsrv)));
       subscr.seq.type = htonl(server_type);
       subscr.seq.lower = subscr.seq.upper = htonl(name_instance);
       subscr.timeout = htonl(wait);
       subscr.filter = htonl(TIPC_SUB_SERVICE);

       chkne(write(sd, &subscr, sizeof(subscr)));
       chkne(read(sd, &event, sizeof(event)));
       close(sd);
       return ntohl(event.event);
}

int run_topology_client(int lower, int upper)
{
       struct sockaddr_tipc topsrv;
       struct tipc_subscr subscr = { {0} };

       int sd = socket(AF_TIPC, SOCK_SEQPACKET, 0);
       tipc_addr_set(&topsrv, TIPC_ADDR_NAME, TIPC_TOP_SRV, TIPC_TOP_SRV, 0);
       chkne(connect(sd, (void *)&topsrv, sizeof(topsrv)));
       subscr.seq.type = htonl(server_type);
       subscr.seq.lower = htonl(lower);
       subscr.seq.upper = htonl(upper);
       subscr.timeout = htonl(-1);
       subscr.filter = htonl(TIPC_SUB_SERVICE);

       chkne(write(sd, &subscr, sizeof(subscr)));
       do {
               struct tipc_event event = { 0 };
               ret = read(sd, &event, sizeof(event));
               fprintf(stderr, "TIPC_TOP_SRV event %d %d %d\n",
                       ntohl(event.event), ntohl(event.found_lower), ntohl(event.found_upper));
       } while (ret >= 0);
       close(sd);
       return ret;
}

int main(int argc, char *argv[])
{
       int tipc;
#ifdef TRACE_ON
       fprintf(stderr, "%s compiled " __DATE__ " " __TIME__ "\n", argv[0]);
#endif
       init(argc, argv);
       buf = malloc(buf_size);
       tipc = socket(AF_TIPC, sock_type, 0);
       chkne(getsockname(tipc, (void *)&name, &addr_size));
       trvx_(name.addr.id.ref);
       trln();
       tipc_addr_set(&addr_sk, addr_type, server_type, addr1, addr2);
       if (wait_peer)
               wait_for_server(addr1, wait_peer);
       switch (mode) {
       case topology_client:
               run_topology_client(addr1, addr2);
               break;
       case single_listener:
       case multi_server:
               run_server(tipc);
               break;
       default:
               run_client(tipc);
       }
       exit(0);
       free(buf);
       shutdown(tipc, SHUT_RDWR);
       close(tipc);
       exit(EXIT_SUCCESS);
}
