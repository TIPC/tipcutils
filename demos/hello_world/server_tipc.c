/* ------------------------------------------------------------------------
//
// server_tipc.c
//
// Short description: This progrem waits for a hello message to be
// recieved, sends back an acknowledge and then exits.
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
//  $Id: server_tipc.c,v 1.3 2006/11/30 14:58:29 ajstephens Exp $
//
//  Revision history:
//  ----------------
//  Rev	Date		Rev by	Reason
//  ---	----		------	------
//
//  PA1	2001-03-11	Jon Maloy	Created
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
#include <sys/poll.h>
#include <sys/ioctl.h>
#include <netdb.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/tipc.h>

#define SERVER_TYPE  18888
#define SERVER_INST  17

int main(int argc, char* argv[], char* dummy[])
{
  int sd = socket (AF_TIPC, SOCK_RDM,0);
  struct sockaddr_tipc server_addr;
  struct sockaddr_tipc client_addr;
  socklen_t alen = sizeof(client_addr);
  char inbuf[40];
  char outbuf[40] = "Uh ?";

  server_addr.family = AF_TIPC;
  server_addr.addrtype = TIPC_ADDR_NAMESEQ;
  server_addr.addr.nameseq.type = SERVER_TYPE;
  server_addr.addr.nameseq.lower = SERVER_INST;
  server_addr.addr.nameseq.upper = SERVER_INST;
  server_addr.scope = TIPC_ZONE_SCOPE;

  printf("****** TIPC server hello world program started ******\n\n");

  /* Make server available: */

  if (0 != bind (sd, (struct sockaddr*)&server_addr,sizeof(server_addr))){
          printf ("Server: Failed to bind port name\n");
          exit (1);
  }

  if (0 >= recvfrom(sd,inbuf,sizeof(inbuf), 0,
                    (struct sockaddr*)&client_addr,
                    &alen)){
          perror("Unexepected message");
  }
  printf("Server: Message received: %s !\n", inbuf); 
  if (0 > sendto(sd,outbuf,strlen(outbuf)+1,0,
         (struct sockaddr*)&client_addr,
                 sizeof(client_addr))){
          perror("Server: Failed to send");
  }
  printf("\n****** TIPC server hello program finished ******\n");
  exit(0);
}
