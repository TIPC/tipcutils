/* ------------------------------------------------------------------------
//
// client_tipc.c
//
// Short description: Client side of multicast demo.
//
// ------------------------------------------------------------------------
//
// Copyright (c) 2003, Ericsson Research Canada
 * Copyright (c) 2005, Wind River Systems
// All rights reserved.
// Redistribution and use in source and binary forms, with or without 
// modification, are permitted provided that the following conditions are met:
//
// Redistributions of source code must retain the above copyright notice, this 
// list of conditions and the following disclaimer.
// Redistributions in binary form must reproduce the above copyright notice, 
// this list of conditions and the following disclaimer in the documentation 
// and/or other materials provided with the distribution.
// Neither the name of the copyright holders nor the names of its 
// contributors may be used to endorse or promote products derived from this 
// software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE 
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
// POSSIBILITY OF SUCH DAMAGE.
//
// ------------------------------------------------------------------------
//
//  Created 2000-09-30 by Jon Maloy
//
// ------------------------------------------------------------------------
//
//  $Id: client_tipc.c,v 1.6 2005/09/20 20:23:15 ajstephens Exp $
//
//  Revision history:
//  ----------------
//  Rev	Date		Rev by	Reason
//  ---	----		------	------
//
//  PA1	2000-09-30	Jon Maloy	Created
//  PA2 2004-03-25      M. Pourzandi    Simplified to support a simple hello message
//
// ------------------------------------------------------------------------
*/
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <sys/param.h>
#include <netdb.h>
#include <errno.h>
#include <unistd.h>
#include <sys/time.h>
#include <ctype.h>
#include <linux/tipc.h>

#define SERVER_TYPE  18888

void wait_for_server(int name_type, int name_inst, int wait)
{
        struct sockaddr_tipc topsrv;
        struct tipc_subscr subscr = {{name_type, name_inst, ~0}, wait,
				     TIPC_SUB_SERVICE, {}};
        struct tipc_event event;

        int sd = socket (AF_TIPC, SOCK_SEQPACKET, 0);
        assert(sd > 0);

        memset(&topsrv, 0, sizeof(topsrv));
	topsrv.family = AF_TIPC;
        topsrv.addrtype = TIPC_ADDR_NAME;
        topsrv.addr.name.name.type = TIPC_TOP_SRV;
        topsrv.addr.name.name.instance = TIPC_TOP_SRV;

        if (0 > connect(sd, (struct sockaddr *)&topsrv, sizeof(topsrv))) {
                perror("failed to connect to topology server");
                exit(1);
        }
        if (send(sd, &subscr, sizeof(subscr), 0) != sizeof(subscr)) {
                perror("failed to send subscription");
                exit(1);
        }
        if (recv(sd, &event, sizeof(event), 0) != sizeof(event)) {
                perror("Failed to receive event");
                exit(1);
        }
        if (event.event != TIPC_PUBLISHED) {
                printf("Server %u not published within %u [s]\n",
                       name_type, wait/1000);
                exit(1);
        }
        close(sd);
}

void client_mcast(int sd, int lower, int upper)
{
        struct sockaddr_tipc server_addr;
        char buf[100];

	server_addr.family = AF_TIPC;
        server_addr.addrtype = TIPC_ADDR_MCAST;
        server_addr.addr.nameseq.type = SERVER_TYPE;
        server_addr.addr.nameseq.lower = lower;
        server_addr.addr.nameseq.upper = upper;
	
	if (sd >= 0) {
		sprintf(buf, "message to {%u,%u,%u}",
			server_addr.addr.nameseq.type,
			server_addr.addr.nameseq.lower,
			server_addr.addr.nameseq.upper);
		printf("Sending: %s\n", buf);
	} else {
		sd = -sd;
		buf[0] = '\0';
		printf("Sending: termination message to {%u,%u,%u}\n",
			server_addr.addr.nameseq.type,
			server_addr.addr.nameseq.lower,
			server_addr.addr.nameseq.upper);
	}

        if (0 > sendto(sd, buf, strlen(buf) + 1, 0,
                       (struct sockaddr *)&server_addr, sizeof(server_addr))) {
                perror("Client: Failed to send");
        }
}

/**
 * Mainline for client side of multicast demo.
 * 
 * Usage: client_tipc [lower [upper [kill]]]
 * 
 * If no arguments supplied, sends a predetermined set of multicast messages.
 * If optional "lower" and "upper" arguments are specified, client sends a
 * single message to the specified instance range; if "upper" is omitted,
 * it defaults to the same value as "lower".
 * If optional "kill" argument is specified (it can have any value), the client
 * sends the server termination message rather than the normal server data
 * message.
 */

int main(int argc, char* argv[], char* envp[])
{
        int sd = socket (AF_TIPC, SOCK_RDM, 0);
	int lower;
	int upper;
	char dummy;

	if (sd < 0) {
		printf("TIPC not active on this node\n");
		exit(1);
	}

        /* run standard demo if no arguments supplied by user */

	if (argc < 2) {
		printf("****** TIPC client multicast demo started ******\n\n");

		wait_for_server(SERVER_TYPE, 399, 10000);

		client_mcast(sd,  99, 100);
		client_mcast(sd, 150, 250);
		client_mcast(sd, 200, 399);
		client_mcast(sd,   0, 399);
		client_mcast(-sd,  0, 399);

		printf("\n****** TIPC client multicast demo finished ******\n");
		exit(0);
	}

	/* multicast to instance range specified by user */

	if (sscanf(argv[1], "%u%c", &lower, &dummy) != 1)
		perror("invalid lower bound");
	if (argc > 2) {
		if (sscanf(argv[2], "%u%c", &upper, &dummy) != 1)
			perror("invalid upper bound");
	}
	else
		upper = lower;

	client_mcast((argc > 3) ? -sd : sd, lower, upper);

	exit(0);
}
