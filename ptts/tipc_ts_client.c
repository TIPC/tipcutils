/* ------------------------------------------------------------------------
 *
 * tipc_ts_client.c
 *
 * Short description: Portable TIPC Test Suite -- common client routines
 * 
 * ------------------------------------------------------------------------
 *
 * Copyright (c) 2006,2008 Wind River Systems
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


#include "tipc_ts.h"   /* must use " for rtp projects */

int importance;	   /* global controlling the TIPC_IMPORTANCE level */
int TS_BLAST_REPS  = 100000;	/* default # packets sent in blaster testing */


/**
 * client_SendConnectionless - client for connectionless sink server
 */

void client_SendConnectionless 
(
int socketType,	  /* socket type to be used SOCK_DGRAM or SOCK_RDM */
int numRequests,  /* number of times to send the message */
int msgSize	  /* size of the message to be sent */
) 
{
	int sockfd_C;				 /* socket */
	struct sockaddr_tipc addr;	 /* address of socket */

	debug ("client_SendConnectionless: Connectionless source: %dx%d bytes out\n",
	       numRequests, msgSize);

	recvSyncTIPC (TS_SYNC_ID_1);
	setServerAddr (&addr);
	sockfd_C = createSocketTIPC (socketType);
	setOption(sockfd_C, TIPC_IMPORTANCE, importance);  /* this is done for the Importance test */
	sendtoSocketTIPC (sockfd_C, &addr, numRequests, msgSize, 0);

	closeSocketTIPC (sockfd_C);

	sendSyncTIPC (TS_SYNC_ID_2);
}

/**
 * client_test_connectionless - run tests using connectionless sockets
 */

void client_test_connectionless
(
int sockType	/* socket type to be tested SOCK_DGRAM or SOCK_RDM */
)
{
	info("\nclient_test_connectionless: subtest 1\n");
	client_SendConnectionless(sockType, 10, 100);

	info("\nclient_test_connectionless: subtest 2\n");
	client_SendConnectionless(sockType, 10, 1000);

	if (sockType == SOCK_RDM) {

		/* A burst of > 50 DGRAM messages can lead to discards
		 * due to link congestion, so only do it for RDM */

		info("\nclient_test_connectionless: subtest 3\n");
		client_SendConnectionless(sockType, 100, 100);
		
		info("\nclient_test_connectionless: subtest 4\n");
		client_SendConnectionless(sockType, 100, 1000);
	}
}

/**
 * client_stress_connectionless - run tests using connectionless sockets
 * 
 * THIS IS A STRESS TEST AND WILL RUN UNTIL THE PROCESS IS TERMINATED
 * 
 */

void client_stress_connectionless
(
int sockType	/* socket type to be tested SOCK_DGRAM or SOCK_RDM */
)
{
	while (1) {
		client_test_connectionless(sockType);
	}
}

/**
 * reportBlast - report results of blaster/blastee test
 * 
 */

void reportBlast
(
int numPackets,
int packetSize,
unsigned int elapsedTime
)
{
	unsigned int divisor = (elapsedTime == 0) ? 1 : elapsedTime;
	unsigned int rate;

	/* Compute throughput, getting best possible precision
	   without causing overflow in 32 bit arithmetic */

	rate = packetSize * TS_BLAST_REPS;
	if (rate <= 536870u)
		rate = (rate * 8000u) / divisor;
	else
		rate = (rate / divisor) * 8000u;

	printf("Sent %d packets of size %d in %d ms (%u bit/s)\n",
	       TS_BLAST_REPS, packetSize, elapsedTime, rate);
}

/**
 * client_blast_connectionless - run tests using connectionless sockets
 * 
 */

void client_blast_connectionless
(
int sockType,
int numPackets,
int packetSize
)
{
	int sockfd_C;				 /* socket */
	struct sockaddr_tipc addr;	 /* address of socket */
	unsigned int startTime;
	unsigned int endTime;

	recvSyncTIPC (TS_SYNC_ID_1);
	getServerAddr (&addr);
	/* note: get port id (not port name) for best performance! */
	sockfd_C = createSocketTIPC (sockType);
	sendSyncTIPC (TS_SYNC_ID_3);
	startTime = getTimeStamp ();
	sendtoSocketTIPC (sockfd_C, &addr, numPackets, packetSize, 0);
	endTime = getTimeStamp ();
	closeSocketTIPC (sockfd_C);
	taskDelay(50);
	sendSyncTIPC (TS_SYNC_ID_2);

	reportBlast (numPackets, packetSize, (endTime - startTime));
}

/**
 * client_blast_connection - run tests using connection-oriented sockets
 * 
 */

void client_blast_connection
(
int sockType,
int numPackets,
int packetSize
)
{
	int sockfd_C;				 /* socket */
	struct sockaddr_tipc addr;	 /* address of socket */
	unsigned int startTime;
	unsigned int endTime;

	recvSyncTIPC (TS_SYNC_ID_1);
	setServerAddr (&addr);
	sockfd_C = createSocketTIPC (sockType);
	connectSocketTIPC (sockfd_C, &addr);
	sendSyncTIPC (TS_SYNC_ID_3);
	startTime = getTimeStamp ();
	sendSocketTIPC (sockfd_C, numPackets, packetSize, 0);
	endTime = getTimeStamp ();
	closeSocketTIPC (sockfd_C);
	sendSyncTIPC (TS_SYNC_ID_2);

	reportBlast (numPackets, packetSize, (endTime - startTime));
}

/**
 * client_test_messageLimits - run tests using connectionless sockets
 * 							 - tests the ability to send messages from 1 to 66000
 */

void client_test_messageLimits(void)
{
	int msize;		/* size of the message to be tested */

	info("\nclient_test_messageLimits: \n");

	/*  want to test the limits of 1 and 66000 and everything in between but will 
		jump by TS_MSGINC each time just to save time */

	for (msize = 1; msize <= 66000;) {
		client_SendConnectionless(SOCK_RDM, 1, msize);  

		debug("message size = %d\n",msize);

		if (((msize + TS_MSGINC) > 66000) && (msize != 66000)) {
			msize = 66000;	 /* if we are close to but not 66000 make it 66000 */
		} else
			msize += TS_MSGINC;
	}


}

/**
 * client_mcast - 
 */

void client_mcast
(
int sd,			/* the socket to use */
int lower,	/* the lower address range */
int upper	/* the upper address range */
)
{
	struct sockaddr_tipc server_addr;  /* address to be filled in */
	char buf[100];			   /* buffer for the message */
	int len;

	setServerAddrTo (&server_addr, TIPC_ADDR_MCAST, TS_TEST_TYPE, lower, upper);

	sprintf(buf, "message to {%u,%u,%u}",
		server_addr.addr.nameseq.type,
		server_addr.addr.nameseq.lower,
		server_addr.addr.nameseq.upper);
	debug("Sending: %s\n", buf);

	len = strlen (buf) + 1;
	if (len != sendto(sd, buf, len, 0,
			  (struct sockaddr *)&server_addr, sizeof(struct sockaddr_tipc))) {
		failTest("Client: Failed to send");
	}

	sendSyncTIPC (TS_SYNC_ID_3);	/* tell server to check for messages */
	recvSyncTIPC (TS_SYNC_ID_4);	/* wait for server to complete check */
}

/**
 * client_test_multicast 	- run multicast tests
 * 				- set up an RDM socket and then send messages to an address range
 * 				- vary the address range to ensure that the server is recieving
 * 				  in the expected range
 * 				- note there has to be as many calls to Client_mcast as TIPC_MCAST_SUBTESTS
 */

void client_test_multicast(void)
{
	int sd;	    /* socket to send with */

	debug("client_test_multicast: \n");

	sd = createSocketTIPC (SOCK_RDM);

	recvSyncTIPC (TS_SYNC_ID_1);		   /* wait for the server to be ready to receive */

	client_mcast(sd,  99, 100);	 /* multicast to {x,  99, 100} */
	client_mcast(sd, 150, 250);	 /* multicast to {x, 150, 250} */
	client_mcast(sd, 200, 399);	 /* multicast to {x, 200, 399} */
	client_mcast(sd,   0, 399);	 /* multicast to {x,   0, 399} */

	sendSyncTIPC (TS_SYNC_ID_2);      
}

/**
 * client_SendConnection - client for connection-oriented server for SOCK_STREAM and SOCK_SEQPACKET
 */

void client_SendConnection 
(
int socketType,	     /* socket type to use */
int numMessages,     /* number of messages to send */
int messageSize,     /* size of the messages sent */
int numReplies,	     /* number of the replies received */
int maxReplySize     /* size of the replies */
)
{
	int sockfd_C;				/* socket to use */
	struct sockaddr_tipc addr;  /* address for the socket */

	debug ("client_SendConnection: Connection echo: %dx%d bytes out, %dx%d(max) bytes in\n",
	       numMessages, messageSize, numReplies, maxReplySize);


	recvSyncTIPC (TS_SYNC_ID_1);

	setServerAddr (&addr);	      /* set up the address */
	sockfd_C = createSocketTIPC (socketType);

	/* this is done for the Importance test */
	setOption(sockfd_C, TIPC_IMPORTANCE, importance);  

	connectSocketTIPC (sockfd_C, &addr);

	/* send the message */
	sendSocketTIPC (sockfd_C, numMessages, messageSize, 0);

	/* wait for the message return */
	recvSocketTIPC (sockfd_C, numReplies, maxReplySize, 0, 0);

	/* close the socket */
	closeSocketTIPC (sockfd_C);

	sendSyncTIPC (TS_SYNC_ID_2);
}

/**
 * client_test_connection - run tests using connection-oriented sockets
 * 							SOCK_STREAM or SOCK_SEQPACKET
 */

void client_test_connection
(
int sockType		 /* socket type to run test on */
)
{
	info("\nclient_test_connection: subtest 1\n");
	client_SendConnection(sockType, 10, 100, 10, 100);
	info("\nclient_test_connection: subtest 2\n");
	client_SendConnection(sockType, 100, 1000, 0, 0);
	info("\nclient_test_connection: subtest 3\n");
	client_SendConnection(sockType, 0, 0, 50, 123);
}

/**
 * client_SendConnectionShutdown 	- client for connection-oriented server
 *                              	- calls shutdown when finished sending
 * 									- valid for SOCK_STREAM and SOCK_SEQPACKET
 */

void client_SendConnectionShutdown 
(
int socketType,	       /* socket type to test */
int numRequests,       /* number of messages to send */
int requestSize       /* size of the message */
)
{
	int sockfd_C;		   /* socket to use */
	struct sockaddr_tipc addr; /* address for the socket */

	debug ("client_SendConnectionShutdown: Connection echo: %dx%d bytes out\n",
	       numRequests, requestSize);


	recvSyncTIPC (TS_SYNC_ID_1);

	setServerAddr (&addr);
	sockfd_C = createSocketTIPC (socketType);

	setOption(sockfd_C, TIPC_IMPORTANCE, importance);  /* this is done for the Importance test */

	connectSocketTIPC (sockfd_C, &addr);

	/* send the messages */
	sendSocketTIPC (sockfd_C, numRequests, requestSize, 0);   

	/* now shut down the socket */
	if (shutdown(sockfd_C, SHUT_RDWR) != 0)	/* second parameter ignored*/
		failTest ("unable to shutdown send buffer");

	/* and do not forget to close the socket on this side */
	closeSocketTIPC (sockfd_C);

	sendSyncTIPC (TS_SYNC_ID_2);
}

/**
 * client_test_connectionShutdown - run tests using connection-oriented sockets
 */

void client_test_connection_shutdown
(
int sockType   /* socket type to use SOCK_SEQPACKET or SOCK_STREAM */
)
{
	info("\nclient_test_connection: subtest 1\n");
	client_SendConnectionShutdown(sockType, 10, 100);

	info("\nclient_test_connection: subtest 2\n");
	client_SendConnectionShutdown(sockType, 100, 1000);

	info("\nclient_test_connection: subtest 3\n");
	client_SendConnectionShutdown(sockType, 500, 1000);
}

/**
 * client_test_importance 	- run connection/connectionless tests while changing 
 *                  		the importance level 
 * 							- this test reruns previous tests with the client server
 * 							synchronization	being done at that level 
 * 							- do not mix the order of the tests	unless the server code
 * 							is also changed to match
 */

void client_test_importance(void)
{
	int localImportance = importance;	/* save to restore later */

	for (importance = TIPC_LOW_IMPORTANCE;
	    importance <= TIPC_CRITICAL_IMPORTANCE; 
	    importance++) {
		info("TIPC Client testing TIPC_IMPORTANCE = %d\n", importance);

		client_test_connectionless(SOCK_DGRAM);
		info("TIPC Client connectionless importance DGRAM test PASSED\n");

		client_test_connectionless(SOCK_RDM);
		info("TIPC Client connectionless importance RDM test PASSED\n");

		client_test_connection(SOCK_STREAM);
		info("TIPC Client connection importance STREAM test PASSED\n"); 

		client_test_connection(SOCK_SEQPACKET);
		info("TIPC Client connection importance SEQPACKET test PASSED\n");

		client_test_connection_shutdown(SOCK_STREAM);
		info("TIPC Client connection importance STREAM test PASSED\n");

		client_test_connection_shutdown(SOCK_SEQPACKET);
		info("TIPC Client connection importance SEQPACKET test PASSED\n");
	}

	importance = localImportance;  /* reset the default */
}

/**
 * client_test_anc_connection - test the Ancillary data for Connection based sockets  
 */

void client_test_anc_connection(void)
{
	int soc;					/* socket being used */
	int msgSize = 1;			/* this is the size of the message we expect to be returned */
	struct sockaddr_tipc saddr;	/* address for the socket */
	char buf[TS_ANCBUFSZ];		/* buffer for the message to be sent */
	char failStr[50];			/* string to record any failure messages */
	char str[16];				/* string for decoding the anc type */
	struct msghdr ctrlbuf;	    /* the message header */
	char ancSpace[CMSG_SPACE(8) + CMSG_SPACE(1024)];   /* allocation of data space for the anc structure */
	struct cmsghdr * anc = (struct cmsghdr *)ancSpace; /* pointer to the anc space */
	int sz;						   /* size of the received message */
	int anc_data[2];				   /* array for storing the anc data */
	struct iovec iov[1];				   /* the iov structure to access the anc data */


	setServerAddr(&saddr);

	ctrlbuf.msg_iov = iov;
	ctrlbuf.msg_iovlen = 1;
	ctrlbuf.msg_name = NULL;
	ctrlbuf.msg_namelen = 0;
	ctrlbuf.msg_controllen = (CMSG_SPACE(8) + CMSG_SPACE(1024));
	ctrlbuf.msg_control = (void*)anc;

	iov[0].iov_base = buf;
	iov[0].iov_len = TS_ANCBUFSZ;

	debug("SEQPACKET (client)...");

	recvSyncTIPC (TS_SYNC_ID_1);

	soc = createSocketTIPC (SOCK_SEQPACKET);

	connectSocketTIPC(soc, &saddr);

	sendSocketBuffTIPC(soc, "ALPHA", 1, 6, 0);

	strcpy (buf ,"");
	/* server will send 'ONE' messages, followed by a 'OMEGA' one when it decides to quit */
	for (;;) {
		ctrlbuf.msg_controllen = CMSG_SPACE(8) + CMSG_SPACE(1024);

		sz = recvmsg(soc ,&ctrlbuf ,0);

		debug("recvmsg returned %d \n",sz);

		if (sz <= 0) {
			break;
		}

		debug ("anc : received '%s'\n", buf);

		if (!strcmp (buf ,"ONE")) {


			strcpy (buf ,"END");
			msgSize = 4; /* record the size of the last message to be sent used later */
		} else {
			/* check anc to see if the type is TIPC_DESTNAME */
			anc = CMSG_FIRSTHDR(&ctrlbuf);
			if (anc != NULL || anc->cmsg_type != TIPC_DESTNAME) {
				anc_data_type(str, anc->cmsg_type);
				sprintf (failStr,"anc not null, anc data type = %d =%s", 
					 anc->cmsg_type, str);
				failTest(failStr);
			}
			strcpy (buf ,"TWO");
		}

		sendSocketBuffTIPC(soc, buf, 1, 4, 0);

	}

	if (sz != 0)
		failTest ("FAILED (client) wrong size");


	anc = CMSG_FIRSTHDR(&ctrlbuf);

	if (anc == NULL)
		failTest ("FAILED (client) anc is NULL");

	if (anc->cmsg_type != TIPC_ERRINFO) {
		anc_data_type(str, anc->cmsg_type);
		sprintf (failStr,"bad anc data type = %d = %s", 
			 anc->cmsg_type, str);
		failTest(failStr);
	}

	anc_data[0] = *((unsigned int*)(CMSG_DATA(anc) + 0));
	anc_data[1] = *((unsigned int*)(CMSG_DATA(anc) + 4));

	debug ("got anc data\n");
	debug ("anc_data[0] = %x\n",anc_data[0]);
	debug ("anc_data[1] = %x\n",anc_data[1]);


	if (anc_data[1] != msgSize) {
		sprintf (failStr,"bad return size %d expecting %d",anc_data[1], msgSize);
		failTest(failStr);
	}


	if (anc_data[0] != TIPC_ERR_NO_PORT) {
		sprintf (failStr,"bad return code %d",anc_data[0]);
		failTest(failStr);
	}

	anc = CMSG_NXTHDR (&ctrlbuf ,anc);
	if (anc == NULL) {
		failTest ("no returned msg...?");

	}
	if (strcmp ((char*)CMSG_DATA(anc) ,"TWO")) {
		failTest ("wrong returned message");

	}
	debug ("returned msg: %s\n" ,(char*) CMSG_DATA (anc));   

	closeSocketTIPC (soc);
	debug ("PASSED (client)\n");

	sendSyncTIPC (TS_SYNC_ID_2);
}

/**
 * client_test_anc_connectionless - test the Ancillary data for Connectionless based sockets  
 */

void client_test_anc_connectionless(void)
{
	int soc;					/* socket being used */
	int msgSize;			/* this is the size of the message we expect to be returned */
	struct sockaddr_tipc saddr;	/* address for the socket */
	char buf[TS_ANCBUFSZ];		/* buffer for the message to be sent */
	char failStr[50];			/* string to record any failure messages */
	char str[16];				/* string for decoding the anc type */
	struct msghdr ctrlbuf;	    /* the message header */
	char ancSpace[CMSG_SPACE(8) + CMSG_SPACE(1024)];   /* allocation of data space for the anc structure */
	struct cmsghdr * anc = (struct cmsghdr *)ancSpace; /* pointer to the anc space */
	int sz;						   /* size of the received message */
	int anc_data[2];				   /* array for storing the anc data */
	int tmp;										   /* temp variable to set a socket option */
	struct iovec iov[1];				   /* the iov structure to access the anc data */



	setServerAddr(&saddr);

	ctrlbuf.msg_iov = iov;
	ctrlbuf.msg_iovlen = 1;
	ctrlbuf.msg_name = NULL;        
	ctrlbuf.msg_namelen = 0;
	ctrlbuf.msg_controllen = (CMSG_SPACE(8) + CMSG_SPACE(1024));
	ctrlbuf.msg_control = (void*)anc;

	iov[0].iov_base = buf;
	iov[0].iov_len = TS_ANCBUFSZ;

	debug("RDM (client)...");

	/* wait for the server to get online */
	recvSyncTIPC (TS_SYNC_ID_1);
	soc = createSocketTIPC (SOCK_RDM);
	tmp = 0;
	setOption (soc , TIPC_DEST_DROPPABLE ,tmp);

	sendtoSocketBuffTIPC(soc, &saddr, "ALPHA", 1, 6, 0);

	msgSize = sizeof("OMEGA"); /* recorded here for a later test */
	sendtoSocketBuffTIPC(soc, &saddr, "OMEGA", 1, msgSize, 0);

	/* now that we have sent the 2 messages sync to the server */
	sendSyncTIPC (TS_SYNC_ID_2);

	ctrlbuf.msg_controllen = CMSG_SPACE(8) + CMSG_SPACE(1024);

	debug ("anc receiving...\n"); 

	sz = recvmsg(soc ,&ctrlbuf ,0);	   /* now try and receive the message */

	debug("recvmsg returned %d \n",sz);

	if (sz != 0)
		failTest ("recvmsg");

	debug ("got something...\n");

	anc = CMSG_FIRSTHDR(&ctrlbuf);

	if (anc == NULL)
		failTest ("FAILED (client) anc is NULL");

	if (anc->cmsg_type != TIPC_ERRINFO) {
		anc_data_type(str, anc->cmsg_type);
		sprintf (failStr, "bad anc data type = %d = %s",anc->cmsg_type, str);
		failTest (failStr);
	}

	debug ("anc : got anc data\n");   
	anc_data[0] = *((unsigned int*)(CMSG_DATA(anc) + 0));
	anc_data[1] = *((unsigned int*)(CMSG_DATA(anc) + 4));

	if (anc_data[1] != msgSize) {
		sprintf (failStr,"bad return size %d",anc_data[1]);
		failTest (failStr);
	}

	if (anc_data[0] != TIPC_ERR_NO_PORT) {
		sprintf (failStr,"bad return code %d",anc_data[0]);
		failTest (failStr);
	}

	anc = CMSG_NXTHDR (&ctrlbuf ,anc);

	if (anc == NULL)
		failTest ("no returned msg...?\n");


	debug ("returned msg: %s\n" ,(char*) CMSG_DATA (anc));

	if (strcmp ((char*)CMSG_DATA(anc) ,"OMEGA")) {
		failTest ("wrong returned message");
	}


	debug ("PASSED (client)\n");

	closeSocketTIPC(soc);

	sendSyncTIPC (TS_SYNC_ID_3);
}

/**
 * client_test_socketOptions - test the TIPC socket options on the client  
 */

void client_test_socketOptions(void)
{
	common_test_socketOptions(); /* do the client side first */
	sendSyncTIPC(TS_SYNC_ID_1);			/* now tell the server to go ahead */
	recvSyncTIPC(TS_SYNC_ID_2);			/* wait for the server to finish */
}

/**
 * client_test_stream -   
 */

void client_test_stream(void)
{
#define BUF_SIZE 2000                   /* size of the buffer */
#define MSG_SIZE 80                     /* size of the message */

	struct sockaddr_tipc server_addr;   /* address of the socket */
	int sd;				    /* socket to be used */
	char buf[BUF_SIZE];		    /* buffer for the message */
	int rec_num;			    /* number of records to send */
	int rec_size;			    /* size of the records to send */
	int tot_size;			    /* total size of the message to send */
	int sent_size;			    /* amount sent */
	int msg_size;			    /* message size sent */

	setServerAddr(&server_addr);

	info("****** TIPC stream test client started ******\n\n");

	recvSyncTIPC(TS_SYNC_ID_1);

	sd = createSocketTIPC (SOCK_STREAM);

	connectSocketTIPC(sd,&server_addr);

	/* Create buffer containing numerous (size,data) records */

	tot_size = 0;
	rec_size = 1;
	rec_num = 0;

	while ((tot_size + 4 + rec_size) <= BUF_SIZE) {
		__u32 size;

		rec_num++;
		size = rec_size;

		buf[tot_size] = (size >> 24); 
		buf[tot_size + 1] = (size >> 16) & 0xFF; 
		buf[tot_size + 2] = (size >> 8) & 0xFF; 
		buf[tot_size + 3] = size & 0xFF; 
		memset(&buf[tot_size + 4], rec_size, rec_size);
		info("Client: creating record %d of size %d bytes\n", 
		     rec_num, rec_size);

		tot_size += (4 + rec_size);
		rec_size = (rec_size + 147) & 0xFF;
		if (!rec_size)
			rec_size = 1; /* record size must be 1-255 bytes */
	}

	/* Now send records using messages that break record boundaries */

	info("Client: sending records using %d byte messages\n", MSG_SIZE);
	sent_size = 0;
	while (sent_size < tot_size) {
		if ((sent_size + MSG_SIZE) <= tot_size)
			msg_size = MSG_SIZE;
		else
			msg_size = (tot_size - sent_size);

		sendSocketBuffTIPC(sd, &buf[sent_size], 1, msg_size, 0);

		sent_size += msg_size;
	}

	/* Now grab set of one-byte client acknowledgements all at once */

	info("Client: waiting for server acknowledgements\n");
	if (recv(sd, buf, rec_num, MSG_WAITALL) != rec_num) {
		failTest("Client: acknowledge error 1");
	}
	if (recv(sd, buf, 1, MSG_DONTWAIT) >= 0) {
		failTest("Client: acknowledge error 2");
	}
	info("Client: received %d acknowledgements\n", rec_num);

	shutdown(sd, SHUT_RDWR);  /* second parameter ignored */
	closeSocketTIPC(sd);

	sendSyncTIPC(TS_SYNC_ID_2);

	info("****** TIPC stream test client finished ******\n");
}


/**
 * client_test_bigStream - run big stream test  
 */

void client_test_bigStream(void)
{
	struct sockaddr_tipc server_addr;   /* address of the socket */
	int sd;				    /* socket to be used */
	char buf[TS_BBUF_SIZE];			/* buffer for the message */

	setServerAddr(&server_addr);

	info("****** TIPC big stream test client started ******\n\n");

	recvSyncTIPC(TS_SYNC_ID_1);  /* wait for server to be ready */

	sd = createSocketTIPC (SOCK_STREAM);
	connectSocketTIPC(sd, &server_addr);
	memset(buf, TS_BBUF_DATA, TS_BBUF_SIZE);
	info("Client: sending %d bytes\n", TS_BBUF_SIZE);
	sendSocketBuffTIPC(sd, buf, 1, TS_BBUF_SIZE, 0);

	recvSyncTIPC(TS_SYNC_ID_2);  /* wait for server to consume 1st stream */

	sendSocketBuffTIPC(sd, buf, 1, TS_BBUF_SIZE, 0);
	shutdown(sd, SHUT_RDWR);
	closeSocketTIPC(sd);
	sendSyncTIPC(TS_SYNC_ID_3);

	info("****** TIPC big stream test client finished ******\n");
}

/**
 * client_test_sendto - test the TIPC sendto and recvfrom on the client  
 */

void client_test_sendto(void)
{
	common_test_sendto(TS_SYNC_ID_1);	 /* sync with server done in this routine */
	recvSyncTIPC(TS_SYNC_ID_2);			/* wait for the server to be ready */
	common_test_recvfrom(TS_SYNC_ID_3);	/*  sync with server done in this routine*/
	sendSyncTIPC(TS_SYNC_ID_4);			/* now tell the server  we are done */
}

/**
 * sendTIPCTest - connectionless send of the test # to be run
 */

void sendTIPCTest
(
int test	 /* test number to send to the server */
)
{
	int msgSize;		   /* size of the message to be sent */
	int res;		   /* sendto result */
	int sockfd_C;		   /* socket used */
	struct sockaddr_tipc addr; /* address of socket */
	int testbuf;

	recvSyncTIPC (TS_SYNC_WAITING_FOR_TEST_ID);
	setServerAddr (&addr);
	sockfd_C = createSocketTIPC (SOCK_RDM);

	testbuf = htonl(test);
	msgSize = sizeof(testbuf);

	res = sendto (sockfd_C, (void *)&testbuf, msgSize, 0, 
		      (struct sockaddr *)&addr, sizeof (addr));

	if (res != msgSize)
		failTest ("unexpected sendto() error sending Server test number");

	closeSocketTIPC (sockfd_C);

	/* only send the next sync if the 
		server is not going to kill itself */
	if (test > 0) {
		sendSyncTIPC (TS_SYNC_FINISHED_TEST_ID);  
	}
}

/**
 * client_test_dgram - wrapper to call client_test_connectionless(SOCK_DGRAM)
 */

void client_test_dgram(void)
{
	client_test_connectionless(SOCK_DGRAM);
}

/**
 * client_test_rdm - wrapper to call client_test_connectionless(SOCK_RDM)
 */

void client_test_rdm(void)
{
	client_test_connectionless(SOCK_RDM);
}

/**
 * client_test_conn_stream - wrapper to call client_test_connection(SOCK_STREAM)
 */

void client_test_conn_stream(void)
{
	client_test_connection(SOCK_STREAM);
}

/**
 * client_test_conn_seqpacket - wrapper to call client_test_connection(SOCK_SEQPACKET)
 */

void client_test_conn_seqpacket(void)
{
	client_test_connection(SOCK_SEQPACKET);
}

/**
 * client_test_shutdown_stream - wrapper to call client_test_connection_shutdown(SOCK_STREAM)
 */

void client_test_shutdown_stream(void)
{
	client_test_connection_shutdown(SOCK_STREAM);
}

/**
 * client_test_shutdown_seqpacket - wrapper to call client_test_connection_shutdown(SOCK_SEQPACKET)
 */  

void client_test_shutdown_seqpacket(void)
{
	client_test_connection_shutdown(SOCK_SEQPACKET);
}

/**
 * client_stress_rdm - wrapper to call client_stress_connectionless(SOCK_RDM)
 */

void client_stress_rdm(void)
{
	client_stress_connectionless(SOCK_RDM);
}


/**
 * cperfList - list of all performance speeds to test
 */

int cblastSize[] = { 1, 32, 64, 100, 128, 256, 512, 1000, 1024, 1280,
	1450, 2048, 8192, 10000, 0};
/**
 * client_blast_rdm - wrapper to call client_blast_connectionless()
 */

void client_blast_rdm(void)
{
	int i = 0;

	while (cblastSize[i] != 0) {
		client_blast_connectionless(SOCK_RDM, TS_BLAST_REPS,
					    cblastSize[i]);
		i++;
	}
}

/**
 * client_blast_seqpacket - wrapper to call client_blast_connection()
 */

void client_blast_seqpacket(void)
{
	int i = 0;

	while (cblastSize[i] != 0) {
		client_blast_connection(SOCK_SEQPACKET, TS_BLAST_REPS,
					cblastSize[i]);
		i++;
	}
}

/**
 * client_blast_stream - wrapper to call client_blast_connection()
 */

void client_blast_stream(void)
{
	int i = 0;

	while (cblastSize[i] != 0) {
		client_blast_connection(SOCK_STREAM, TS_BLAST_REPS,
					cblastSize[i]);
		i++;
	}
}


/**
 * tipcTestSuiteHelp - print out the valid tests 
 */

void tipcTestSuiteHelp(void)
{
	int counter;	  /* loop counter to find the test names */

	printf("\n tipcTC:    Supported Testcases\n\n            Sanity Tests\n");
	for (counter = 0; counter <= TS_NUMBER_OF_TESTS; counter++) {
		if (counter < ts_lastSanityTest)
			printf("            %4d %s tests\n",nameList[counter].testNum, nameList[counter].name);

		if (counter == ts_lastSanityTest)
			printf("\n            Stress Tests\n");

		if (counter >= ts_lastSanityTest + 1)
			printf("            %4d %s test \n",nameList[counter].testNum, nameList[counter].name);

	}
	printf("\n\n");
}

/**
 * clientList - list of all client tests, must be in sync with: 
 * 		nameList[] in tipc_ts_common.c and 
 * 		serverList[] in tipc_ts_server.c and
 * 		TS_NUM in tipc_ts.h
 */

TSTEST clientList[] = {
	{ts_doAllTests, NULL},
	{ts_dgram, client_test_dgram},
	{ts_rdm, client_test_rdm},
	{ts_conn_stream, client_test_conn_stream},
	{ts_conn_seqpacket, client_test_conn_seqpacket},
	{ts_shutdown_stream, client_test_shutdown_stream},
	{ts_shutdown_seqpacket, client_test_shutdown_seqpacket},
	{ts_messageLimits, client_test_messageLimits},
	{ts_importance, client_test_importance},
	{ts_socketOptions, client_test_socketOptions},
	{ts_connection_anc, client_test_anc_connection},
	{ts_connectionless_anc, client_test_anc_connectionless},
	{ts_multicast, client_test_multicast},
	{ts_stream, client_test_stream},
	{ts_bigStream, client_test_bigStream},
	{ts_sendto, client_test_sendto},
#if DO_BLAST
	{ts_blast_rdm, client_blast_rdm},
	{ts_blast_seqpacket, client_blast_seqpacket},
	{ts_blast_stream, client_blast_stream},
#endif
	{ts_lastSanityTest, NULL},      
	{ts_stress_rdm, client_stress_rdm},
	{ts_lastStressTest, NULL}       
};


/**
 * tipcTestClientX - run specified TIPC test 
 */

void tipcTestClientX
(
int test /* the test to run */
)
{
	int ntimes = TS_NUMTIMES; /* this is the number of times a test will be run */
	int testIndex; /* this is the index used to find the test we want to run */

	if ((test < TS_FIRST_SANITY_TEST)
	    || (test >= (ts_lastStressTest))
	    ||((test>= ts_lastSanityTest) && (test < TS_FIRST_STRESS_TEST))) {
		tipcTestSuiteHelp();
		killme(1);
	}
	
	printf ("Test # %d\n", test); 
	sendTIPCTest(test);	/* tell the server what to do */

	/* now loop through all the tests and locate the one we want to run */  
	for (testIndex = 0; testIndex <= TS_NUMBER_OF_TESTS; testIndex++) {
		if (clientList[testIndex].testNum == test)
			do {
				printf("TIPC %s test...STARTED!\n", testName(test));
				clientList[testIndex].test();
				printf("TIPC %s test...PASSED!\n", testName(test));       

				if (TS_NUMTIMES > 1)
					printf("Client test loop %d\n", ntimes);

				printf("\n");
			}
			while ( --ntimes > 0 );
	}
}

/**
 * tipcTestClient - run TIPC sanity test X
 *      	  - test 0 will run all tests
 *      	  - if killFlag is set TS_KILL_SERVER is sent to the server to kill it 
 * 
 */

void tipcTestClient
(
int test	/* test number to run (0 = all tests) */
)
{
	int lastTestNum; /* the last test to be run */

	if (test == TS_KILL_SERVER) {
		printf("Shutting down server\n");
		sendTIPCTest(TS_KILL_SERVER);
		return;
	}

	if (((test >= ts_lastSanityTest) && (test < TS_FIRST_STRESS_TEST)) 
	    || (test >= ts_lastStressTest) || (test < 0)) {
		tipcTestSuiteHelp();
		killme(1);
	}

	if (test == 0) {
		/* do all tests */
		test = 1;
		lastTestNum = ts_lastSanityTest - 1;
	} else {
		/* do specified test only */
		lastTestNum = test;
	}

	/* run all indicated tests */

	importance = TIPC_LOW_IMPORTANCE; /* set up the default */

	for (; test <= lastTestNum; test++)
		tipcTestClientX(test);    
}

