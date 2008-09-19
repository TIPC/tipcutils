/* ------------------------------------------------------------------------
//
// client_tipc.c
//
// Short description: This progrem sends a hello world message, then
// waits for an acknowledge before exiting.
// ------------------------------------------------------------------------
//
// Copyright (c) 2003, Ericsson Research Canada
// All rights reserved.
// Redistribution and use in source and binary forms, with or without 
// modification, are permitted provided that the following conditions are met:
//
// Redistributions of source code must retain the above copyright notice, this 
// list of conditions and the following disclaimer.
// Redistributions in binary form must reproduce the above copyright notice, 
// this list of conditions and the following disclaimer in the documentation 
// and/or other materials provided with the distribution.
// Neither the name of Ericsson Research Canada nor the names of its 
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
//  $Id: client_tipc.c,v 1.6 2005/10/24 21:18:34 jonmaloy Exp $
//
//  Revision history:
//  ----------------
//  Rev	Date		Rev by	Reason
//  ---	----		------	------
//
//  PA1	2000-09-30	Jon Maloy	Created
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
#include <sys/poll.h>
#include <sys/ioctl.h>
#include <netdb.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#include <ctype.h>
#include <linux/tipc.h>

#define SERVER_TYPE  18888
#define SERVER_INST  17

void wait_for_server(struct tipc_name* name,int wait)
{
        struct sockaddr_tipc topsrv;
        struct tipc_subscr subscr = {{name->type,name->instance,name->instance},
                                     wait,TIPC_SUB_SERVICE,{}};
        struct tipc_event event;

        int sd = socket (AF_TIPC, SOCK_SEQPACKET,0);
        assert(sd > 0);

        memset(&topsrv,0,sizeof(topsrv));
	topsrv.family = AF_TIPC;
        topsrv.addrtype = TIPC_ADDR_NAME;
        topsrv.addr.name.name.type = TIPC_TOP_SRV;
        topsrv.addr.name.name.instance = TIPC_TOP_SRV;

        /* Connect to topology server: */

        if (0 > connect(sd,(struct sockaddr*)&topsrv,sizeof(topsrv))){
                perror("Failed to connect to topology server");
                exit(1);
        }
        if (send(sd,&subscr,sizeof(subscr),0) != sizeof(subscr)){
                perror("Failed to send subscription");
                exit(1);
        }
        /* Now wait for the subscription to fire: */
        if (recv(sd,&event,sizeof(event),0) != sizeof(event)){
                perror("Failed to receive event");
                exit(1);
        }
        if (event.event != TIPC_PUBLISHED){
                printf("Server %u,%u not published within %u [s]\n",
                       name->type,name->instance,wait/1000);
                exit(1);
        }
        close(sd);
}


int main(int argc, char* argv[], char* dummy[])
{
        int sd;
        struct sockaddr_tipc server_addr;
        char buf[40];

	server_addr.family = AF_TIPC;
        server_addr.addrtype = TIPC_ADDR_NAME;
        server_addr.addr.name.name.type = SERVER_TYPE;
        server_addr.addr.name.name.instance = SERVER_INST;
        server_addr.addr.name.domain = 0;

        printf("****** TIPC connection demo client started ******\n\n");

        wait_for_server(&server_addr.addr.name.name,10000);
  
        printf("Connection setup 1: standard (TCP style) connect\n");
        sd = socket (AF_TIPC, SOCK_SEQPACKET, 0);
        if (connect(sd, (struct sockaddr *)&server_addr,
                sizeof(server_addr)) != 0) {
                perror("Connect failed");
	}
        printf("Client: Connection established\n");
        strcpy(buf, "Hello World");
        if (0 > send(sd,buf,strlen(buf)+1,0)){
                perror("Client: Failed to send");
        }
        printf("Client: Sent msg \"%s\" \n", buf);
        if (0 >= recv(sd, buf, sizeof(buf), 0)) {
                perror("Client: Failed to receive");
        }
        printf("Client: Received response \"%s\" \n", buf);
        printf("Client: Shutting down connection\n\n");
        shutdown(sd, SHUT_RDWR);
        close(sd);

        printf("Connection setup 2: optimized (TIPC style) connect\n");
	sd = socket (AF_TIPC, SOCK_SEQPACKET, 0);
        strcpy(buf, "Hello Again");
        if (0 > sendto(sd, buf, strlen(buf)+1, 0,
                       (struct sockaddr *)&server_addr,
                       sizeof(server_addr))) {
                perror("Client: Failed to send");
        }
        printf("Client: Sent msg \"%s\" \n", buf);
        if (0 >= recv(sd, buf, sizeof(buf), 0)) {
                perror("Client: Failed to receive");
        }
        printf("Client: Received response \"%s\"\n", buf);
        printf("Client: Killing connection without shutdown\n\n");
        close(sd);

        printf("Connection setup 3: optimized (TIPC style) connect\n");
        sd = socket (AF_TIPC, SOCK_SEQPACKET,0);
        strcpy(buf, "Hello Again Again");
        if (0 > sendto(sd, buf, strlen(buf)+1, 0,
                       (struct sockaddr*)&server_addr,
                       sizeof(server_addr))) {
                perror("Client: Failed to send");
        }
        printf("Client: Sent msg \"%s\" \n", buf);
        printf("Client: Will now exit without closing socket!!\n\n");
        exit(0);
}
