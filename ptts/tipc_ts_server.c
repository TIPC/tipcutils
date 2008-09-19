/* ------------------------------------------------------------------------
 *
 * tipc_ts_server.c
 *
 * Short description: Portable TIPC Test Suite -- common server routines
 * 
 * ------------------------------------------------------------------------
 *
 * Copyright (c) 2006, Wind River Systems
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

#include "tipc_ts.h"  /* must use " for rtp projects */



int TS_BLAST_SREPS = 100000;	/* default # packets sent in blaster testing */

static int client_in_same_cluster;

/**
 * recvSocketShutdownTIPC - receives messages from a socket
 *
 * "numTimes" specifies expected number of received messages;
 * if < 0, then any number of messages is OK (quits if connection closed)
 *
 * "recvErrorTarget" specifies expected number of recv() errors; 
 * if < 0, then any number of errors is OK
 *
 * "checkErrorTarget" specifies expected number of corrupted messages;
 * if < 0, then any number of corrupted messages is OK
 *
 * Note: a recv() error also counts as a corrupted message!
 *
 */  

void recvSocketShutdownTIPC 
(
int sockfd,		/* socket to use */
int numTimes,	    /* the number of received messages */
int maxSize,	    /* maximum size of the message to be received */
int checkErrorTarget/* expected corrupted messages */
)
{
	char *msgArea;		/* space for the message to be received */
	int checkErrorCount;	/* counter for the check errors */
	int res;		/* return code for the receive */
	int requested;		/* total size of the data to receive */
	int count = 0;		/* count of the received data */

	if (numTimes == 0)
		return;

	msgArea = (char*)malloc (maxSize);
	if (msgArea == NULL)
		failTest ("unable to malloc() receive buffer");

	checkErrorCount = 0;
	requested = numTimes*maxSize;

	do {
		res = recv (sockfd, msgArea, maxSize, 0);

		if (res == 0) {

			if ((numTimes >= 0) && (count == requested)) {
				debug ("recv() returned shutdown\n");
				closeSocketTIPC (sockfd);
				break;
			} else {
				failTest("recv() returned unexpected disconnect");
			}
		}
		if (res < 0) {
			break;
		} else if (checkArray (msgArea, res) == 0) {
			count += res;
		} else {
			checkErrorCount++;
		}

	} while (res > 0);




	if ((checkErrorTarget >= 0) && (checkErrorCount != checkErrorTarget))
		failTest ("unexpected number of corrupted messages received");

	free (msgArea);
}

/**
 * server_receiveConnectionless - connectionless sink server
 */ 
void server_receiveConnectionless 
(
int socketType,		  /* socket type to use SOCK_DGRAM or SOCK_RDM */
int numRequests,	  /* number of messages to receive */
int maxRequestSize	  /* size of the messages to receive */
) 
{
	int sockfd_S;		  /* socket to use */
	struct sockaddr_tipc addr;/* address of socket */

	debug ("server_receiveConnectionless: Connectionless sink: %dx%d(max) bytes in\n",
	       numRequests, maxRequestSize);

	setServerAddr (&addr);
	sockfd_S = createSocketTIPC (socketType);
	bindSocketTIPC (sockfd_S, &addr);

	sendSyncTIPC (TS_SYNC_ID_1);
	recvSocketTIPC (sockfd_S, numRequests, maxRequestSize, 0, 0);
	closeSocketTIPC (sockfd_S); 

	recvSyncTIPC (TS_SYNC_ID_2);
}

/**
 * server_test_connectionless - run sanity tests using connectionless sockets
 */

void server_test_connectionless
(
int sockType	 /* socket type to use SOCK_DGRAM or SOCK_RDM */
)
{
	info("server_test_connectionless subtest 1\n");
	server_receiveConnectionless(sockType, 10, 100);

	info("server_test_connectionless subtest 2\n");
	server_receiveConnectionless(sockType, 10, 1000);

	if (sockType == SOCK_RDM) {
		info("server_test_connectionless subtest 3\n");
		server_receiveConnectionless(sockType, 100, 100);

		info("server_test_connectionless subtest 4\n");
		server_receiveConnectionless(sockType, 100, 1000);
	}
}

/**
 * server_stress_connectionless - run stress tests using connectionless sockets
 */

void server_stress_connectionless
(
int sockType	     /* socket type to use SOCK_DGRAM or SOCK_RDM */
)
{
	while (1) {
		info("server_test_connectionless subtest 1\n");
		server_receiveConnectionless(sockType, 10, 100);

		info("server_test_connectionless subtest 2\n");
		server_receiveConnectionless(sockType, 10, 1000);

		/* due to the nature of DGRAM sockets they will drop messages 
		if too many are sent at once, which is not good for a sanity check,
		so we will only do the following tests on RDM */

		if (sockType == SOCK_RDM) {
			info("server_test_connectionless subtest 3\n");
			server_receiveConnectionless(sockType, 100, 100);

			info("server_test_connectionless subtest 4\n");
			server_receiveConnectionless(sockType, 100, 1000);
		}
	}
}

/**
 * server_blast_connectionless - run tests using connectionless sockets
 * 
 */

void server_blast_connectionless
(
int sockType,
int numPackets,
int packetSize
)
{
	int sockfd_S;		  /* socket to use */
	struct sockaddr_tipc addr;/* address of socket */

	setServerAddr (&addr);
	sockfd_S = createSocketTIPC (sockType);
	bindSocketTIPC (sockfd_S, &addr);
	sendSyncTIPC (TS_SYNC_ID_1);
	recvSyncTIPC (TS_SYNC_ID_3);
	recvSocketTIPC (sockfd_S, numPackets, packetSize, 0, -1);
	closeSocketTIPC (sockfd_S);
	recvSyncTIPC (TS_SYNC_ID_2);
}

/**
 * server_blast_connection - run tests using connection-oriented sockets
 * 
 */

void server_blast_connection
(
int sockType,
int numPackets,
int packetSize
)
{
	int sockfd_L;		    /* socket to listen on */
	int sockfd_A;		    /* socket to receive on */
	struct sockaddr_tipc addr;  /* address for socket */

	setServerAddr (&addr);
	sockfd_L = createSocketTIPC (sockType);
	listenSocketTIPC (sockfd_L);
	bindSocketTIPC (sockfd_L, &addr);
	sendSyncTIPC (TS_SYNC_ID_1);
	sockfd_A = acceptSocketTIPC (sockfd_L);
	closeSocketTIPC (sockfd_L);
	recvSyncTIPC (TS_SYNC_ID_3);
	recvSocketTIPC (sockfd_A, numPackets, packetSize, 0, -1);
	closeSocketTIPC (sockfd_A);
	recvSyncTIPC (TS_SYNC_ID_2);
}

/**
 * server_test_messageLimits - run tests using connectionless sockets
 */

void server_test_messageLimits(void)
{
	int msize;     /* message size to receive */

	info("\nserver_test_messageLimits: \n");

	for (msize = 1; msize <= 66000;) {
		server_receiveConnectionless(SOCK_RDM, 1, msize);  /* if first param set to 10 out of mem */

		debug("message size = %d\n",msize);
		if (((msize + TS_MSGINC)>66000) && (msize != 66000)) {
			msize = 66000;
		} else
			msize += TS_MSGINC;
	}
}

/**
 * server_receiveConnection - connection-oriented server
 *                  In this test we know the number of connections and the size to
 *                  expect. 
 *                  Could be written to just loop until we receive a shutdown message
 */

void server_receiveConnection 
(
int socketType,		 /* either SOCK_STREAM or SOCK_SEQPACKET */
int numConns,		 /* number of connections to make */
int numRequests,	 /* number of messages to receive */
int maxRequestSize,	 /* size of the messages to receive */
int numReplies,		 /* number of messages to send as a reply */
int replySize		 /* size of the message to send as a reply */
)
{
	int sockfd_L;		    /* socket to listen on */
	int sockfd_A;		    /* socket to receive on */
	struct sockaddr_tipc addr;  /* address for socket */
	int i;			    /* loop variable */

	debug ("server_receiveConnection: Connection echo: %d conns, %dx%d(max) bytes in, %dx%d bytes out\n",
	       numConns, numRequests, maxRequestSize, numReplies, replySize);

	setServerAddr (&addr);
	sockfd_L = createSocketTIPC (socketType);
	listenSocketTIPC (sockfd_L);
	bindSocketTIPC (sockfd_L, &addr);

	sendSyncTIPC (TS_SYNC_ID_1);		/* now tell the Client to go ahead */

	for (i = 0; i < numConns; i++) {
		sockfd_A = acceptSocketTIPC (sockfd_L);
		recvSocketTIPC (sockfd_A, numRequests, maxRequestSize, 0, 0);
		sendSocketTIPC (sockfd_A, numReplies, replySize, 0);
		closeSocketTIPC (sockfd_A);
	}
	closeSocketTIPC (sockfd_L);

	recvSyncTIPC (TS_SYNC_ID_2);	  /* tell the Client that we have finished */

}

/**
 * server_test_connection - run sanity tests using connection-oriented sockets
 */

void server_test_connection
(
int sockType	 /* socket type to use SOCK_STREAM or SOCK_SEQPACKET */
)
{
	server_receiveConnection(sockType, 1, 10, 100, 10, 100);  /* receive 10 send 10 */
	server_receiveConnection(sockType, 1, 100, 1000, 0, 0);	  /* receive 1000 send 0 */
	server_receiveConnection(sockType, 1, 0, 0, 50, 123);	  /* receive 0 send 50 */
}

/**
 * server_receiveConnectionShutdown - connection-oriented server
 *                  In this test we know the number of connections and the size to
 *                  expect. 
 *                  Loops until a shutdown message is received
 */

void server_receiveConnectionShutdown 
(
int socketType,	     /* socket type to use SOCK_STREAM or SOCK_SEQPACKET */
int numConns,	     /* number of connections to make */
int numRequests,     /* number of messages to receive */
int maxRequestSize   /* size of the messages to receive */
)
{
	int sockfd_L;		    /* socket to listen on */
	int sockfd_A;		    /* socket returned from the accept */
	struct sockaddr_tipc addr;  /* address to use */
	int i;			    /* loop counter */

	debug ("server_receiveConnectionShutdown: Connection echo: %d conns, %dx%d(max) bytes in\n",
	       numConns, numRequests, maxRequestSize);

	setServerAddr (&addr);
	sockfd_L = createSocketTIPC (socketType);
	listenSocketTIPC (sockfd_L);
	bindSocketTIPC (sockfd_L, &addr);

	sendSyncTIPC (TS_SYNC_ID_1);		/* now tell the Client to go ahead */

	for (i = 0; i < numConns; i++) {
		sockfd_A = acceptSocketTIPC (sockfd_L);
		recvSocketShutdownTIPC (sockfd_A, numRequests, maxRequestSize, 0);
	}
	closeSocketTIPC (sockfd_L);

	recvSyncTIPC (TS_SYNC_ID_2);	  /* tell the Client that we have finished */

}

/**
 * test_connection_shutdown - run sanity tests using connection-oriented sockets
 *                              Server will receive until a shutdown is seen
 */

void server_test_connection_shutdown
(
int sockType  /* socket type to use SOCK_STREAM or SOCK_SEQPACKET */
)
{
	server_receiveConnectionShutdown(sockType, 1, 10, 100);
	server_receiveConnectionShutdown(sockType, 1, 100, 1000);
	server_receiveConnectionShutdown(sockType, 1, 500, 1000);
}

/**
 * server_test_importance - run connection/connectionless tests while changing 
 *                  the importance level 
 */

void server_test_importance(void)
{
	int importance;							/* local increment */

	for (importance = TIPC_LOW_IMPORTANCE;
	    importance <= TIPC_CRITICAL_IMPORTANCE; 
	    importance++) {
		info("TIPC Server testing TIPC_IMPORTANCE = %d\n", importance);

		server_test_connectionless(SOCK_DGRAM);
		info("TIPC Server connectionless importance DGRAM test PASSED\n");

		server_test_connectionless(SOCK_RDM);
		info("TIPC Server connectionless importance RDM test PASSED\n");

		server_test_connection(SOCK_STREAM);
		info("TIPC Server connection importance STREAM test PASSED\n"); 

		server_test_connection(SOCK_SEQPACKET);
		info("TIPC Server connection importance SEQPACKET test PASSED\n");

		server_test_connection_shutdown(SOCK_STREAM);
		info("TIPC Server connection importance STREAM test PASSED\n"); 

		server_test_connection_shutdown(SOCK_SEQPACKET);
		info("TIPC Server connection importance SEQPACKET test PASSED\n");
	}
}

/**
 * server_test_anc_connection - test the Ancillary data for Connectionless based sockets  
 */

void server_test_anc_connection(void)
{
	int sol;			   /* socket to listen on */
	int sos;	       /* socket to receive on */
	int ii;				   /* loop counter */
	int anc_data[3];	   /* array for received data */

	char buf[TS_ANCBUFSZ];	       /* buffer to receive into */
	char failStr[50];	       /* string for failure return codes */
	char str[16];		       /* another string for return codes */
	char ancSpace[CMSG_SPACE(12)]; /* 12 bytes to retrieve the name used by the client */

	struct sockaddr_tipc saddr;			   /* address for the sending socket */
	struct msghdr ctrlbuf;				   /* the control buffer structure */
	struct cmsghdr * anc = (struct cmsghdr *)ancSpace; /* pointer to the anc data */        
	struct iovec iov[1];				   /* array of iov structures */

	iov[0].iov_base = buf;
	iov[0].iov_len = TS_ANCBUFSZ;

	ctrlbuf.msg_name = NULL;
	ctrlbuf.msg_namelen = 0;
	ctrlbuf.msg_controllen = (CMSG_SPACE(12));  /* see above */
	ctrlbuf.msg_control = (void*)anc;
	ctrlbuf.msg_iov = iov;
	ctrlbuf.msg_iovlen = 1;



	setServerAddrTo(&saddr, TIPC_ADDR_NAMESEQ, TS_TEST_TYPE, TS_LOWER, TS_UPPER);
	saddr.scope = TS_SCOPE;   

	sol = createSocketTIPC (SOCK_SEQPACKET);

	listenSocketTIPC (sol);	/*  was passing 20 to listen*/

	bindSocketTIPC(sol, &saddr);

	/* now sync with the client */

	sendSyncTIPC(TS_SYNC_ID_1);

	sos = acceptSocketTIPC(sol);

	if (0 > recvmsg (sos, &ctrlbuf, 0))
		failTest ("FAILED (server) bad recvmsg return code");



	/* retrieving the name used by the client; could be a name sequence (12 bytes) */
	anc = CMSG_FIRSTHDR(&ctrlbuf);

	if (anc == NULL)
		failTest ("FAILED (server) anc is NULL");

	if (TIPC_DESTNAME != anc->cmsg_type) {
		anc_data_type(str, anc->cmsg_type);
		sprintf (failStr,"bad anc data type = %d = %s",
			 anc->cmsg_type, str);
		failTest (failStr);
	}

	anc_data[0] = *((unsigned int*)(CMSG_DATA(anc) + 0));
	anc_data[1] = *((unsigned int*)(CMSG_DATA(anc) + 4));
	anc_data[2] = *((unsigned int*)(CMSG_DATA(anc) + 8));

	if (TS_TEST_TYPE != anc_data[0]) {
		sprintf (failStr,"FAILED (server) wrong name %d \n", anc_data[0]);
		failTest (failStr);
	}
	if (TS_TEST_INST != anc_data[1]) {
		sprintf (failStr,"FAILED (server) wrong instance %x\n", anc_data[1]);
		failTest (failStr);
	}
	if (TS_TEST_INST != anc_data[2]) {
		sprintf (failStr,"FAILED (server) wrong instance %x\n", anc_data[2]);
		failTest (failStr);
	}

	debug ("server_test_anc : anc data : %x %x %x\n"
	       ,(*(unsigned int*)(CMSG_DATA(anc) + 0))
	       ,(*(unsigned int*)(CMSG_DATA(anc) + 4))
	       ,(*(unsigned int*)(CMSG_DATA(anc) + 8))
	      );

	debug ("server_test_anc : msg with name used: '%s'\n" ,buf);


	/* exchange a couple of messages for the fun of it */
	for (ii = 0; ii < 3; ii++) {
		sendSocketBuffTIPC(sos, "ONE", 1, 4, 0);

		if (0 > recv (sos ,buf ,TS_ANCBUFSZ ,0))
			failTest ("server_test_anc: (recv)");


		debug ("server_test_anc : received '%s'\n" ,buf);  
	}

	debug ("server_test_anc : closing socket: \n"
	       "peer will get errcode through anc data\n");

	sendSocketBuffTIPC (sos, "OMEGA", 1, 6, 0); /* 'OMEGA' message will cause client to quit its loop */

	sleep (1);  /* wait for the client to send back a 'END' message so that it get rejected */

	closeSocketTIPC (sos);
	closeSocketTIPC (sol);

	recvSyncTIPC (TS_SYNC_ID_2);

}

/**
 * server_test_anc_connectionless - test the Ancillary data for Connectionless based sockets  
 */

void server_test_anc_connectionless(void)
{

	int sos;	  /* socket to send/receive on */
	int anc_data[3];  /* array for received anc data */

	char buf[TS_ANCBUFSZ];		    /* buffer for the message */
	char failStr[50];		    /* string for the failure return codes */
	char str[16];			    /* another string for the return codes */
	char ancSpace[CMSG_SPACE(12)];		/* 12 bytes to retrieve the name used by the client */

	struct sockaddr_tipc saddr;				/* address for the socket */
	struct msghdr ctrlbuf;					/* control buffer structure */
	struct cmsghdr * anc = (struct cmsghdr *)ancSpace;	/* pointer to the anc data */
	struct iovec iov[1];				    /* array of iov structures */

	iov[0].iov_base = buf;
	iov[0].iov_len = TS_ANCBUFSZ;

	ctrlbuf.msg_name = NULL;
	ctrlbuf.msg_namelen = 0;
	ctrlbuf.msg_controllen = (CMSG_SPACE(12));  /* see above */
	ctrlbuf.msg_control = (void*)anc;
	ctrlbuf.msg_iov = iov;
	ctrlbuf.msg_iovlen = 1;



	setServerAddrTo(&saddr, TIPC_ADDR_NAMESEQ, TS_TEST_TYPE, TS_LOWER, TS_UPPER);
	saddr.scope = TS_SCOPE;     


	sos = createSocketTIPC (SOCK_RDM);

	bindSocketTIPC (sos ,&saddr);

	/* now sync with the client */
	sendSyncTIPC(TS_SYNC_ID_1);

	/* the client will now send 2 messages 
	and the server has to wait for them to be sent before continuing 
	without this resync the server can close the socket too quickly and then the ANC data
	would have the error of TIPC_ERR_NO_NAME instead of TIPC_ERR_NO_PORT when client 
	and server are not on the same node */
	recvSyncTIPC(TS_SYNC_ID_2);

	ctrlbuf.msg_controllen = CMSG_SPACE(12);
	if (0 > recvmsg (sos, &ctrlbuf, 0)) {
		failTest ("recvmsg");

	}

	anc = CMSG_FIRSTHDR(&ctrlbuf);

	if (anc == NULL)
		failTest ("FAILED (server) anc is NULL");

	if (TIPC_DESTNAME != anc->cmsg_type) {
		anc_data_type(str, anc->cmsg_type);
		sprintf (failStr,"bad anc data type = %d = %s",
			 anc->cmsg_type, str);
		failTest (failStr);
	}


	anc_data[0] = *((unsigned int*)(CMSG_DATA(anc) + 0));
	anc_data[1] = *((unsigned int*)(CMSG_DATA(anc) + 4));
	anc_data[2] = *((unsigned int*)(CMSG_DATA(anc) + 8));

	if (TS_TEST_TYPE != anc_data[0]) {
		sprintf (failStr,"wrong name %x ",anc_data[2]);
		failTest(failStr);      
	}

	if (TS_TEST_INST != anc_data[1]) {
		sprintf (failStr,"wrong instance %x",anc_data[1]);
		failTest(failStr);
	}

	if (TS_TEST_INST != anc_data[2]) {
		sprintf (failStr,"wrong instance %x",anc_data[2]);
		failTest(failStr);
	}

	sleep (1);				/* may be able to replace the sleep with another sync message */
	closeSocketTIPC (sos);

	recvSyncTIPC (TS_SYNC_ID_3);

}

/**
 * server_bindMulticast - setup the socket and bind to it to wait for the multicast traffic
 */

void server_bindMulticast
(
int lower,	  /* lower address to receive from */
int upper,	  /* upper address to receive from */
int sd		/* socket descriptor */
)
{
	struct sockaddr_tipc server_addr; /* address for the socket to be created */

	setServerAddrTo (&server_addr, TIPC_ADDR_NAMESEQ, 
			 TS_TEST_TYPE, lower, upper);
	server_addr.scope = TS_SCOPE; 
	bindSocketTIPC(sd, &server_addr);
	tipc_printaddr(&server_addr); /* prints the address if the verbose is set to debug (2) */
	debug("Socket bind for multicast successful\n");
}

#define TIPC_MCAST_SUBTESTS 4				 /* number of subtests to run */
#define TIPC_MCAST_SOCKETS  5				 /* number of sockets to service */

/**
 * server_mcast - 
 */

void server_mcast
(
int *sd,		 /* array of sockets */
fd_set *readfds,	 /* file descriptor for all the sockets */
int numSubTest				 /* subtest currently being run */
)
{
	static int expected[TIPC_MCAST_SUBTESTS][TIPC_MCAST_SOCKETS] = {
		{1,1,1,0,0},
		{0,0,1,1,0},
		{0,0,0,1,1},
		{1,1,1,1,1}   
	};

	fd_set fds;
	struct timeval timeout;	/* timeout structure for select */
	int num_ready;
	int i;					 /* loop variable */
	int gotMsg;
	char buf[100];				/* buffer to receive into */

	recvSyncTIPC (TS_SYNC_ID_3);	/* wait for client to finish sending messages */

	fds = *readfds;
	timeout.tv_sec  = 0;
	timeout.tv_usec = 0;
	num_ready = select(FD_SETSIZE, &fds, NULL, NULL, &timeout);

	if (num_ready < 0)
		failTest("select() returned error");

	/* Handle special case of inter-cluster multicasting */

	if (!client_in_same_cluster) {
		if (num_ready > 0)
			failTest("unexpected multicast messages received");
		else
			goto exit;
	}

	/* Handle normal case of intra-cluster multicasting */

	for (i = 0; i < TIPC_MCAST_SOCKETS; i++) {
		gotMsg = !!FD_ISSET(sd[i], &fds);

		if (gotMsg != expected[numSubTest][i]) {
			printf("multicast subtest %d index %d expected %d got %d\n",numSubTest, i, expected[numSubTest][i], gotMsg);
			failTest("unexpected multicast result");
		}

		if (gotMsg && recvfrom(sd[i], buf, sizeof(buf), MSG_DONTWAIT,
				       NULL, NULL) < 0) {
			failTest("multicast message not received");
		}

		if (recvfrom(sd[i], buf, sizeof(buf), MSG_DONTWAIT, 
			     NULL, NULL) >= 0) {
			failTest("second multicast message received");
		}
	}

	exit:
	debug ("Just finished SubTest %d\n", numSubTest + 1);
	sendSyncTIPC (TS_SYNC_ID_4);	/* tell client to continue */
}

/**
 * server_test_multicast - run multicast tests (only tests RDM for now)
 */

void server_test_multicast(void)
{
	int sd[TIPC_MCAST_SOCKETS];		 /* array of sockets */
	fd_set readfds;				/* file descriptor for all the sockets */
	int i;					 /* loop variable */

	FD_ZERO(&readfds);

	for (i = 0; i < TIPC_MCAST_SOCKETS; i++) {
		sd[i] = createSocketTIPC (SOCK_RDM);
		FD_SET(sd[i], &readfds);
	}

	server_bindMulticast(  0,  99, sd[0]);    
	server_bindMulticast(  0,  99, sd[1]); /* bind 2 sockets to the same range */
	server_bindMulticast(100, 199, sd[2]);
	server_bindMulticast(200, 299, sd[3]);
	server_bindMulticast(200, 299, sd[3]); /* bind the same socket twice, should not change result */
	server_bindMulticast(300, 399, sd[4]);

	sendSyncTIPC (TS_SYNC_ID_1);

	for (i = 0; i < TIPC_MCAST_SUBTESTS; i++) {
		server_mcast(sd, &readfds, i);
	}

	for (i = 0; i < TIPC_MCAST_SOCKETS; i++) {
		closeSocketTIPC (sd[i]);
	}

	recvSyncTIPC (TS_SYNC_ID_2);
}

/**
 * server_test_socketOptions - run socket options tests
 * 							 - local test of socket options
 */

void server_test_socketOptions(void)
{
	/* test is first run on Client */
	recvSyncTIPC (TS_SYNC_ID_1);
	common_test_socketOptions();   /* now test gets run on Server */
	sendSyncTIPC (TS_SYNC_ID_2);
}


/**
 * server_test_stream - run stream tests
 */

void server_test_stream(void)
{
#define MAX_REC_SIZE  256

	struct sockaddr_tipc server_addr;     /* address for socket */
	int listener_sd;		      /* socket to listen on */
	int peer_sd;			      /* socket to receive on */
	int rec_num;			      /* current record number being processed */
	char inbuf[MAX_REC_SIZE];	      /* buffer to receive data into */
	char outbuf = 'X';		      /* buffer to send as an acknowledgement */
	char failMsg[50];		      /* string for failure return code */

	info("****** TIPC stream test server started ******\n\n");

	listener_sd = createSocketTIPC (SOCK_STREAM);

	setServerAddrTo(&server_addr, TIPC_ADDR_NAMESEQ, TS_TEST_TYPE, 
			TS_TEST_INST, TS_TEST_INST);

	server_addr.scope = TS_SCOPE; 

	bindSocketTIPC(listener_sd, &server_addr);

	listenSocketTIPC(listener_sd);

	/* now sync with the client */

	sendSyncTIPC(TS_SYNC_ID_1);

	peer_sd = acceptSocketTIPC(listener_sd);


	rec_num = 0;
	while (1) {
		int msg_size;
		int rec_size;

		msg_size = recv(peer_sd, inbuf, 4, MSG_WAITALL);
		if (msg_size == 0) {
			debug("Server: client terminated normally\n");    
			break;
		}
		if (msg_size < 0) {
			failTest("Server: client terminated abnormally\n");    
		}

		rec_num++;

		rec_size = *(__u32 *)inbuf;
		rec_size = ntohl(rec_size);

		debug("Server: receiving record %d of %u bytes\n", 
		      rec_num, rec_size);

		msg_size = recv(peer_sd, inbuf, rec_size, MSG_WAITALL);

		if (msg_size != rec_size) {
			sprintf(failMsg,"Server: receive error, got %d bytes\n",
				msg_size); 
			failTest(failMsg);
		}
		while (msg_size > 0) {
			if ((unsigned char)inbuf[--msg_size] != rec_size) {
				failTest("Server: record content error\n");    
			}
		}
		debug("Server: record %d received\n", rec_num);

		/* Send 1 byte acknowledgement (value is irrelevant) */

		if (0 >= send(peer_sd, &outbuf, 1, 0)) {
			failTest("Server: failed to send response\n");
		}
		debug("Server: record %d acknowledged\n", rec_num);    
	}

	closeSocketTIPC(peer_sd);
	closeSocketTIPC(listener_sd);
	recvSyncTIPC(TS_SYNC_ID_2);
	info("****** TIPC stream test server finished ******\n");
}

/**
 * do_receive - receive for the big stream tests
 *
 * This routine keeps receiving data until it gets TS_BBUF_SIZE bytes.
 * 
 * Returns: # of receives needed to receive all data, 0 if got no data,
 *          or < 0 if only got partial data
 * 
 * NOTE: This test does minimal verification of the content of the stream,
 *       since the basic stream test already does full validation.
 */

int do_receive 
(
int peer_sd,			/* socket to use for the receive */
int recvFlag			/* recv() flags [0 or MSG_WAITALL] */
)
{
	char inbuf[TS_BBUF_SIZE];	/* buffer to receive data into */
	int buf_size = TS_BBUF_SIZE;	/* amount of data still to receive */
	int chunk_size = 0;		/* amount of data received */
	int numRec = 0;			/* number of times we received */

	do {
		chunk_size = recv(peer_sd, inbuf, buf_size, recvFlag);
		debug("recv gave us %d\n", chunk_size);

		if (chunk_size == 0) {
			debug("Server: client terminated normally\n");    
			return -numRec;
		}

		if ((chunk_size < 0) || (chunk_size > buf_size))
			failTest("recv failed ");

		if ((inbuf[0] != TS_BBUF_DATA) || 
		    (inbuf[chunk_size-1] != TS_BBUF_DATA))
			failTest("recv corruption ");

		buf_size -= chunk_size;
		numRec++;
	}
	while (buf_size > 0);

	return numRec;
}

/**
 * server_test_bigStream - run big stream tests
 */

void server_test_bigStream(void)
{
	struct sockaddr_tipc server_addr;   /* address for socket */
	int listener_sd;		    /* socket to listen on */
	int peer_sd;			    /* socket to receive on */
	int numRec;						    /* number of messages received */
	char failStr[50];					/* failure string */

	info("****** TIPC big stream test server started ******\n\n");

	listener_sd = createSocketTIPC (SOCK_STREAM);
	setServerAddrTo(&server_addr, TIPC_ADDR_NAMESEQ, TS_TEST_TYPE, 
			TS_TEST_INST, TS_TEST_INST);
	server_addr.scope = TS_SCOPE;  
	bindSocketTIPC(listener_sd, &server_addr);
	listenSocketTIPC(listener_sd);

	sendSyncTIPC(TS_SYNC_ID_1);  /* tell client to start test */

	peer_sd = acceptSocketTIPC(listener_sd);
	numRec = do_receive(peer_sd, 0);
	info("Subtest 1 with the MSG_WAITALL flag not set, number received = %d\n", numRec);
	if (numRec <= 0) {
		sprintf(failStr, "SubTest 1 returned %d", numRec);
		failTest(failStr);
	}

	sendSyncTIPC(TS_SYNC_ID_2);  /* tell client to send 2nd stream */

	numRec = do_receive(peer_sd, MSG_WAITALL);
	info("Subtest 2 with the MSG_WAITALL flag set, number received = %d\n", numRec);
	if (numRec != 1) {
		sprintf(failStr, "SubTest 2 returned %d", numRec);
		failTest(failStr);
	}

	recvSyncTIPC(TS_SYNC_ID_3);  /* ensure client has closed connection */

	numRec = do_receive(peer_sd, MSG_WAITALL);
	info("Subtest 3 with the MSG_WAITALL flag set, number received = %d\n", numRec);
	if (numRec != 0) {
		sprintf(failStr, "SubTest 3 returned %d", numRec);
		failTest(failStr);
	}
	closeSocketTIPC(peer_sd);

	closeSocketTIPC(listener_sd);

	info("****** TIPC big stream test server finished ******\n");
}

/**
 * server_test_sendto - test the TIPC sendto on the server  
 */

void server_test_sendto(void)
{  
	common_test_recvfrom(TS_SYNC_ID_1);		/* do a recvfrom the client - sync is done in this routine*/
	sendSyncTIPC(TS_SYNC_ID_2);			/* tell client ready */
	common_test_sendto(TS_SYNC_ID_3);		/* do a sendto the client - sync is done in this routine */
	recvSyncTIPC(TS_SYNC_ID_4);			/* now wait for the client to finish */
	info("****** TIPC sendto - recvfrom test finished ******\n");
}

/**
 * server_test_dgram - wrapper to call server_test_connectionless(SOCK_DGRAM)
 */
void server_test_dgram(void)
{
	server_test_connectionless(SOCK_DGRAM);
}


/**
 * server_test_rdm - wrapper to call server_test_connectionless(SOCK_RDM)
 */
void server_test_rdm(void)
{
	server_test_connectionless(SOCK_RDM);
}


/**
 * client_test_conn_stream - wrapper to call server_test_connection(SOCK_STREAM)
 */
void server_test_conn_stream(void)
{
	server_test_connection(SOCK_STREAM);
}

/**
 * server_test_conn_seqpacket - wrapper to call server_test_connection(SOCK_SEQPACKET)
 */
void server_test_conn_seqpacket(void)
{
	server_test_connection(SOCK_SEQPACKET);
}


/**
 * server_test_shutdown_stream - wrapper to call server_test_connection_shutdown(SOCK_STREAM)
 */
void server_test_shutdown_stream(void)
{
	server_test_connection_shutdown(SOCK_STREAM);
}


/**
 * client_test_shutdown_seqpacket - wrapper to call client_test_connection_shutdown(SOCK_SEQPACKET)
 */  
void server_test_shutdown_seqpacket(void)
{
	server_test_connection_shutdown(SOCK_SEQPACKET);
}

/**
 * server_stress_rdm - wrapper to call server_stress_connectionless(SOCK_RDM)
 */  
void server_stress_rdm(void)
{
	server_stress_connectionless(SOCK_RDM);
}

/**
 * sperfList - list of all performance speeds to test
 */

int sblastSize[] = {1, 32, 64, 100, 128, 256, 512, 1000, 1024, 1280,
	1450, 2048, 8192, 10000, 0};

/**
 * server_blast_rdm - wrapper to call server_blast_connectionless()
 */  
void server_blast_rdm(void)
{
	int i =0;

	while (sblastSize[i] != 0) {
		server_blast_connectionless(SOCK_RDM, TS_BLAST_SREPS,
					    sblastSize[i]);
		i++;
	}

}

/**
 * server_blast_seqpacket - wrapper to call server_blast_connection()
 */  
void server_blast_seqpacket(void)
{
	int i =0;

	while (sblastSize[i] != 0) {
		server_blast_connection(SOCK_SEQPACKET, TS_BLAST_SREPS,
					sblastSize[i]);
		i++;
	}
}

/**
 * server_blast_stream - wrapper to call server_blast_connection()
 */  
void server_blast_stream(void)
{
	int i =0;

	while (sblastSize[i] != 0) {
		server_blast_connection(SOCK_STREAM, TS_BLAST_SREPS,
					sblastSize[i]);
		i++;
	}
}


/**
 * serverList - list of all server tests, must be in sync with: 
 * 		nameList[] in tipc_ts_common.c and 
 * 		clientList[] in tipc_ts_client.c and
 * 		TS_NUM in tipc_ts.h
 */ 

TSTEST serverList[] = {
	{ts_doAllTests, NULL,},
	{ts_dgram, server_test_dgram},
	{ts_rdm, server_test_rdm},
	{ts_conn_stream, server_test_conn_stream},
	{ts_conn_seqpacket, server_test_conn_seqpacket},
	{ts_shutdown_stream, server_test_shutdown_stream},
	{ts_shutdown_seqpacket, server_test_shutdown_seqpacket},
	{ts_messageLimits, server_test_messageLimits},
	{ts_importance, server_test_importance},
	{ts_socketOptions, server_test_socketOptions},
	{ts_connection_anc, server_test_anc_connection},
	{ts_connectionless_anc, server_test_anc_connectionless},
	{ts_multicast, server_test_multicast},
	{ts_stream, server_test_stream},
	{ts_bigStream, server_test_bigStream},
	{ts_sendto, server_test_sendto},
#if DO_BLAST
	{ts_blast_rdm, server_blast_rdm},
	{ts_blast_seqpacket, server_blast_seqpacket},
	{ts_blast_stream, server_blast_stream},
#endif
	{ts_lastSanityTest, NULL},      
	{ts_stress_rdm, server_stress_rdm},
	{ts_lastStressTest, NULL}       
};


/**
 * tipcTestServer   - run TIPC sanity tests for the server
 *                  	1 synchronize with the client  
 *                  	2 get the test number from the client
 *                  	3 call the test case to be run or quit
 * 			            4 loop back and get the next test 
 *                  
 */

void tipcTestServer(void)
{
	int test;	    /* this is the test passed from the Test Client process */
	int testIndex;	/* this is the index used to find the actual test we want to run*/
	int ntimes;	/* this is the number of times a test will be run */

	int res;	/* the return code from the recv() */
	int msgSize;	/* the size of the message */

	int sockfd_S;	/* the socket used to receive the test from the client */
	struct sockaddr_tipc addr; /* the TIPC address data structure */
	struct sockaddr_tipc from; /* the address data structure for the client */
	struct sockaddr_tipc self; /* the TIPC address data structure */
	int fromLen = sizeof(struct sockaddr_tipc); /* the length of the return address */
	int selfLen = sizeof(struct sockaddr_tipc); /* the length of the return address */
	__u32 domain;
	__u32 client_domain;

	debug("Starting Server\n");

	setServerAddr (&addr);
	msgSize = sizeof(test);

	while (1) {
		fflush (stdout);
		ntimes = TS_NUMTIMES;

		/* Get test identifier from client */

		sockfd_S = createSocketTIPC (SOCK_RDM);
		bindSocketTIPC (sockfd_S, &addr);
		sendSyncTIPC (TS_SYNC_WAITING_FOR_TEST_ID);
		res = recvfrom (sockfd_S, (char *)&test, msgSize, 0, (struct sockaddr *) &from, &fromLen);      
		if (res > 0)
			test = (int)ntohl(test);
		else
			test = TS_INVALID_TEST;

		if (getsockname (sockfd_S, (struct sockaddr *) &self, &selfLen) != 0)
			failTest ("getsockname() error");
		domain = self.addr.id.node;
		client_domain= from.addr.id.node;
		client_in_same_cluster =
		((tipc_zone(domain) == tipc_zone(client_domain)) 
		 && (tipc_cluster(domain) == tipc_cluster(client_domain)));

		closeSocketTIPC (sockfd_S);

		/* Validate test identifier */

		if (test == TS_KILL_SERVER) {
			break;
		}
		if ((test >= ts_lastStressTest)
		    || ((test >= ts_lastSanityTest) && (test <TS_FIRST_STRESS_TEST))) {
			printf("TIPC Server: Invalid test number (%d)\n", test);
			killme(1);
		}

		/* Run the specified test */

		info("Server says test # %d\n", test);

		recvSyncTIPC (TS_SYNC_FINISHED_TEST_ID);

		for (testIndex = 0; testIndex <= TS_NUMBER_OF_TESTS; testIndex++) {
			if (serverList[testIndex].testNum == test)
				do {
					info("TIPC Server %s test...STARTED!\n",testName(test));
					serverList[testIndex].test();
					info("TIPC Server %s test...PASSED!\n\n",testName(test));       
				}
				while ( --ntimes > 0 );
		}
	}

	printf("TIPC Server: Client told us to quit\n");
} 

