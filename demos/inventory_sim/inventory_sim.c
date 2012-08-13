/* ------------------------------------------------------------------------
 
Name: inventory_sim.c

Short description: TIPC distributed inventory simulation (Linux version)

Copyright (c) 2004-2008, 2010-2011 Wind River Systems, Inc.
All rights reserved.
Redistribution and use in source and binary forms, with or without 
modification, are permitted provided that the following conditions are met:

Redistributions of source code must retain the above copyright notice, this 
list of conditions and the following disclaimer.
Redistributions in binary form must reproduce the above copyright notice, 
this list of conditions and the following disclaimer in the documentation 
and/or other materials provided with the distribution.
Neither the names of the copyright holders nor the names of its 
contributors may be used to endorse or promote products derived from this 
software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE 
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
POSSIBILITY OF SUCH DAMAGE.


DESCRIPTION
This file is a demo program that illustrates how TIPC can be used to support
distributed applications.  It takes advantage of TIPC's socket-based
messaging, as well as its port naming and port name subscription capabilities. 

The demo is a simulation of a store that stocks a variety of numbered items
which are sold to customers upon request.  By default, items and customers
are created randomly (although there are limits on the number of each that
can be present in the store at any given time).  From time to time new items
are delivered to the store; independently of this, customers enter the store,
each looking for a specific item.  If a desired item can be obtained before a
customer gets tired of waiting for it, a sale is recorded; otherwise, a walk-out
is recorded.  If multiple customers attempt to obtain the same item, only one
succeeds; the others are notified of their failure and try again.

Each item and customer is implemented as a separate task.  An item makes itself
available by creating a socket whose port name identifies the item number.
A customer determines the availability of an item by creating a subscription
for the item name and waiting for notification that the item exists; the
customer then obtains the item by sending a message to the socket for the item
and receiving a successful reply.  A logging task is used to record all
significant events encountered by the item and customer tasks during the
simulation, as well as to display the simulation status and statistics on a
periodic basis.

This simulation is most effective when it is run on multiple CPUs at the same
time, which has the effect of creating a network of stores sharing a global
inventory.  Since the port name published by an item is visible throughout the
TIPC cluster, customers are able to obtain items from other CPUs if they are
not available locally.  The more CPUs involved in the simulation, the more
items are available and the less likely a customer will be to walk out of a
store because the item they desire is unavailable.

When a simulation is terminated the logging task waits for all customers to
leave, then creates a janitor task to dispose of the unsold items that remain.  
TIPC's name subscription capability allows the janitor to distinguish the items
belonging to its own store from those in other stores.

The simulation can be temporarily halted to permit inspection of the various
tasks used, or to assist in debugging problems.  This is achieved by
publishing the name of a virtual item (item 0), which instructs the logging
task on each CPU to temporarily suspend processing; this causes all customer
and item tasks on the CPU to become blocked when they next attempt to log
an event.  (Note: Pausing the simulation may result in warning messages from
customers about non-responsive items; this is normal behavior resulting from
the way the pause capability is implemented, and is no cause for concern.)

The demo gives the user control over the number of item and customer tasks
used in the simulation, as well as the ability to increase the speed of
processing and to control the amount of output generated.  By default, a
simulation running at normal speed displays all transaction info, while a
simulation running at an accelerated rate displays only the periodic summary.
The simulation can also be run in a deterministic manner by creating a store
having no items or customers, and then generating individual item and customer
tasks manually.

Building the demo
-----------------
The demo is built as a normal TIPC application.  The accompanying Makefile
builds a "inventory_sim" executable and creates softlinks to a set of aliases 
(eg. newSim, killSim, ...) that are used to invoke the main executable to do
specific operations.

Running the demo
----------------
The simulation is run by issuing the following shell commands:

newSim [#items [#customers [speed [verbosity]]]]
                       - open store
killSim                - close store and clean up
stopSim                - pause simulation of all stores
startSim               - unpause simulation of all stores
newItem [item [count]] - create item (repeat as specified)
newCust [item [count]] - create customer for item (repeat as specified)
helpSim                - display this menu

A missing (or 0) argument value causes the simulation to choose an appropriate
default.  In certain cases, an argument value that is < 0 can also be used
to disable a specific feature of a command.

The output messages generated by the simulation should be self-explanatory.

Examples
--------
1) Run default simulation:		./newSim

2) Run simulation at 10x normal speed:	./newSim 0 0 10

3) Run simulation manually:		./newSim -1 -1   [create empty store]
					./newItem 3      [create item]
					./newCust 3      [create customer]
*/

/* includes */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <linux/tipc.h>

/* defines */

#define TIPC_INV_SIM_TYPE 75		/* TIPC type # used by items in demo */

#define MSG_SIZE_MAX 50			/* maximum message size (in bytes) */

#define DEMO_ITEM_ID_MIN 1		/* minimum item ID */
#define DEMO_ITEM_ID_MAX 10		/* maximum item ID */
#define NUM_DEMO_ITEMS (DEMO_ITEM_ID_MAX - DEMO_ITEM_ID_MIN + 1)

#define AUTO_ITEM_DEFAULT 10		/* default # of items in store */
#define AUTO_CUST_DEFAULT 10		/* default # of customers in store */

#define CUSTOMER_WAIT_MIN   5000	/* min time a customer will wait (ms) */
#define CUSTOMER_WAIT_MAX   20000	/* max time a customer will wait (ms) */
#define NEW_CUST_WAIT_MIN   1000	/* min time before new customer (ms) */
#define NEW_CUST_WAIT_MAX   5000	/* max time before new customer (ms) */
#define NEW_ITEM_WAIT_MIN   0		/* min time before new item (ms) */
#define NEW_ITEM_WAIT_MAX   8000	/* max time before new item (ms) */
#define SIM_STATUS_INTERVAL 10000	/* time between status displays (ms) */

#define TIPC_BOGUS_SUBSCR_TYPE TIPC_TOP_SRV
#define TIPC_BOGUS_SUBSCR_INST 0

enum {					/* simulation state values */
	STORE_OPEN      = 0,
	STORE_CLOSING   = 1,
	STORE_CLOSED    = 2,
	STORE_EMPTY     = 3
};

enum {					/* logging task transaction codes */
	SIM_DONE        = -4,
	SIM_KILL        = -3,
	SIM_ERROR       = -2,
	SIM_WARN        = -1,
	SIM_INFO        = 0,
	SIM_ITEM_IN     = 1,
	SIM_ITEM_OUT    = 2,
	SIM_CUST_IN     = 3,
	SIM_CUST_OUT    = 4
};

/* type definitions */

struct trans_msg {			/* logging task message format */
	int code;			/* transaction type identifier */
	char string_area[100];		/* transaction content */
};

/* locals */

static int simState;			/* state of simulated store */
static int simErrors;			/* error counter */
static int simWarnings;			/* warning counter */

static int itemCount[NUM_DEMO_ITEMS];	/* # of items in stock */
static int itemsInStore = 0;		/* total # items currently in store */
static int itemsSold[NUM_DEMO_ITEMS];	/* record of # of items sold */

static int customerCount[NUM_DEMO_ITEMS];/* # customers waiting for an item */
static int customersInStore = 0;	/* total # customers currently in store */
static int customerTotal = 0;		/* grand total # of customers processed */
static int customerSales;		/* # customers who left with item */
static int customerExits;		/* # customers who left w/o item */
static int customerRetries;		/* # customers who had to retry */

/* Note: total customer sales may not match total items sold
   because customers can buy items from other locations! */


/*******************************************************************************
 *
 * randomGet - return a random integer in the specified range (inclusive)
 */

int randomGet(int minValue, int maxValue)
{
	return(rand() % (maxValue - minValue + 1)) + minValue;
}

/*******************************************************************************
 *
 * childSpawn - start up a child process
 *
 * This routine forks off a child process.  The child primes its own random # 
 * generator and invokes its mainline function; when the mainline returns
 * the child exits.  The parent simply returns once child is created.
 *
 */

void childSpawn(void *func, void *arg, int argSize, char *taskName)
{
	pid_t pid;

	fflush(stdout);
	pid = fork();
	if (pid < 0) {
		perror("Fork failed\n");
		exit(1);
	}

	if (pid == 0) {
		srand((int)getpid());
		((void (*)(void *))func)(arg);
		exit(0);
	}
}

/*******************************************************************************
 *
 * simDelay - sleep for a specified interval
 *
 * This routine delays the invoking task by using a bogus subscription
 * to TIPC's topology server to generate the necessary timeout. 
 *
 */

void simDelay(int delay_in_msecs)
{
	int sockfd;
	struct sockaddr_tipc topsrv;
	struct tipc_subscr subscr;
	struct tipc_event event;

	sockfd = socket(AF_TIPC, SOCK_SEQPACKET, 0);
	if (sockfd < 0) {
		printf("simDelay: Can't create socket\n");
		return;
	}

	topsrv.family = AF_TIPC;
	topsrv.addrtype = TIPC_ADDR_NAME;
	topsrv.addr.name.name.type = TIPC_TOP_SRV;
	topsrv.addr.name.name.instance = TIPC_TOP_SRV;
	topsrv.addr.name.domain = 0;

	subscr.seq.type  = htonl(TIPC_BOGUS_SUBSCR_TYPE);
	subscr.seq.lower = htonl(TIPC_BOGUS_SUBSCR_INST);
	subscr.seq.upper = htonl(TIPC_BOGUS_SUBSCR_INST);
	subscr.filter    = htonl(TIPC_SUB_SERVICE);
	subscr.timeout   = htonl(delay_in_msecs);

	if (sendto(sockfd, &subscr, sizeof(subscr), 0,
		   (struct sockaddr *)&topsrv, sizeof(topsrv)) < 0) {
		printf("simDelay: Can't issue timeout subscription\n");
		goto exit;
	}

	if (recv(sockfd, &event, sizeof(event), 0) < 0) {
		printf("simDelay: Error receiving subscription event\n");
		goto exit;
	}

	if (event.event != htonl(TIPC_SUBSCR_TIMEOUT)) {
		printf("simDelay: Unexpected subscription event received\n");
		goto exit;
	}
exit:
	close(sockfd);
}

/*******************************************************************************
 *
 * simFind - determine if simulation exists on this CPU
 *
 * RETURNS: 1 if simulation exists, otherwise 0
 *
 */

int simFind(void)
{
	struct tipc_event event;
	struct tipc_subscr subscr;
	struct sockaddr_tipc topsrv;
	int sockfd_s;
	int res = 0;

	/*
	 * Use subscription to look for port name published by logging task;
	 * since it is published with "node" scope, don't have to worry about
	 * detecting names published by logging tasks on other CPUs ...
	 */

	sockfd_s = socket(AF_TIPC, SOCK_SEQPACKET, 0);
	if (sockfd_s < 0) {
		printf("can't create socket\n");
		goto exit;
	}

	topsrv.family = AF_TIPC;
	topsrv.addrtype = TIPC_ADDR_NAME;
	topsrv.addr.name.name.type = TIPC_TOP_SRV;
	topsrv.addr.name.name.instance = TIPC_TOP_SRV;
	topsrv.addr.name.domain = 0;

	subscr.seq.type = htonl(TIPC_INV_SIM_TYPE);
	subscr.seq.lower = htonl(~0);
	subscr.seq.upper = htonl(~0);
	subscr.timeout = htonl(10);	/* nominal delay */
	subscr.filter = htonl(TIPC_SUB_SERVICE);

	if (sendto(sockfd_s, &subscr, sizeof(subscr), 0,
		   (struct sockaddr *)&topsrv, sizeof(topsrv)) < 0) {
		printf("subscription failure\n");
		goto exit;
	}

	if (recv(sockfd_s, &event, sizeof(event), 0) < 0) {
		printf("subscription event failure\n");
		goto exit;
	}

	res = (event.event == htonl(TIPC_PUBLISHED));
exit:
	close(sockfd_s);

	return res;
}

/*******************************************************************************
 *
 * simLog - send transaction message to simulation logging task
 *
 * Can safely use host endianness in messages, since logging task is on
 * this node.  Logging task replies with a transaction acknowledgement;
 * a negative acknowledgement value indicates that the sender should terminate
 * (either because the simulation has ended or an error has occurred.)
 *
 * RETURNS: transaction acknowledgement (value < 0 means caller should exit)
 */

int simLog(int code, const char *fmt, ...)
{
	int sockfd;
	struct sockaddr_tipc trans_srv_addr;
	struct trans_msg msg;
	int len;
	va_list args;
	int res;

	/* Create transaction message */

	va_start(args, fmt);
	len = vsprintf(msg.string_area, fmt, args);
	va_end(args);

	msg.string_area[len++] = '\0';
	msg.code = code;

	/* Send transaction to logging task */

	sockfd = socket(AF_TIPC, SOCK_SEQPACKET, 0);
	if (sockfd < 0) {
		printf("Can't create socket to send transactions\n");
		return SIM_ERROR;
	}

	trans_srv_addr.family = AF_TIPC;
	trans_srv_addr.addrtype = TIPC_ADDR_NAME;
	trans_srv_addr.addr.name.name.type = TIPC_INV_SIM_TYPE;
	trans_srv_addr.addr.name.name.instance = ~0;
	trans_srv_addr.addr.name.domain = 0;

	if (sendto(sockfd, &msg, (sizeof(code) + len), 0,
		   (struct sockaddr *)&trans_srv_addr,
		   sizeof(trans_srv_addr)) < 0) {
		printf("Can't send transaction\n");
		res = SIM_ERROR;
		goto exit;
	}

	/* Wait for logging task to acknowledge transaction */

	if (recv(sockfd, &res, sizeof(res), 0) != sizeof(res)) {
		printf("Can't receive transaction acknowledgement\n");
		printf("Transaction was %d, %s\n", msg.code, msg.string_area);
		res = SIM_ERROR;
		goto exit;
	}

exit:
	close(sockfd);
	return res;
}

/*******************************************************************************
 *
 * simJanitorTask - mainline for the simulation janitor task
 *
 * This routine disposes of any items remaining in a closed store.
 *
 */

void simJanitorTask(void *arg)
{
	struct tipc_subscr subscr;
	struct tipc_event event;
	struct sockaddr_tipc topsrv;
	struct sockaddr_tipc self;
	struct sockaddr_tipc item;
	socklen_t addrlen;
	int sockfd_s;
	int sockfd_c;
	char msg = '\0';

	/* Consume remaining items in store (ignore items in other stores) */

	sockfd_s = socket(AF_TIPC, SOCK_SEQPACKET, 0);
	if (sockfd_s < 0) {
		simLog(SIM_ERROR, "janitor can't create socket\n");
		goto exit;
	}

	addrlen = sizeof(self);
	if (0 > getsockname(sockfd_s, (struct sockaddr *)&self, &addrlen)) {
		simLog(SIM_ERROR, "janitor can't determine own address\n");
		goto exit;
	}

	topsrv.family = AF_TIPC;
	topsrv.addrtype = TIPC_ADDR_NAME;
	topsrv.addr.name.name.type = TIPC_TOP_SRV;
	topsrv.addr.name.name.instance = TIPC_TOP_SRV;
	topsrv.addr.name.domain = 0;

	subscr.seq.type = htonl(TIPC_INV_SIM_TYPE);
	subscr.seq.lower = htonl(DEMO_ITEM_ID_MIN);
	subscr.seq.upper = htonl(DEMO_ITEM_ID_MAX);
	subscr.timeout = htonl(10);	/* nominal delay */
	subscr.filter = htonl(TIPC_SUB_PORTS);

	if (sendto(sockfd_s, &subscr, sizeof(subscr), 0,
		   (struct sockaddr *)&topsrv, sizeof(topsrv)) < 0) {
		simLog(SIM_ERROR, "janitor subscription failure\n");
		goto exit;
	}

	while (1) {
		if (recv(sockfd_s, &event, sizeof(event), 0) < 0) {
			simLog(SIM_ERROR, "janitor subscription event failure\n");
			break;
		}

		if (event.event == htonl(TIPC_SUBSCR_TIMEOUT))
			break;	/* all done */

		if ((event.event == htonl(TIPC_PUBLISHED)) &&
		    (event.port.node == htonl(self.addr.id.node))) {

			/*
			 * Fake a purchase request to trigger item
			 * self-destruction, but don't worry about 
			 * whether item is really obtained or not
			 */

			item.family = AF_TIPC;
			item.addrtype = TIPC_ADDR_ID;
			item.addr.id.node = ntohl(event.port.node);
			item.addr.id.ref = ntohl(event.port.ref);
			addrlen = sizeof(item);

			sockfd_c = socket(AF_TIPC, SOCK_SEQPACKET, 0);
			sendto(sockfd_c, &msg, sizeof(msg), 0,
			       (struct sockaddr *)&item, addrlen);
			close(sockfd_c);
		} else {
			/* ignore withdraw event */
		}
	}

	/* 
	 * Delay a bit to allow items to finish cleaning themselves up,
	 * then tell logging task to shut down 
	 */

	simDelay(100);
exit:
	close(sockfd_s);

	simLog(SIM_DONE, "all done\n");
}

/*******************************************************************************
 *
 * showSim - show the status of the simulation
 *
 * This routine prints out the status of the simulated store on this node.
 *
 */

void showSim(void)
{
	int i;

	printf("\nItem #   :");
	for (i = 0; i < NUM_DEMO_ITEMS; i++)
		printf(" %3d", DEMO_ITEM_ID_MIN + i);
	printf("\n----------");
	for (i = 0; i < NUM_DEMO_ITEMS; i++)
		printf("----");
	printf("\nSold     :");
	for (i = 0; i < NUM_DEMO_ITEMS; i++)
		printf(" %3d", itemsSold[i]);
	printf("\nIn Stock :");
	for (i = 0; i < NUM_DEMO_ITEMS; i++)
		printf(" %3d", itemCount[i]);
	printf("\nCustomers:");
	for (i = 0; i < NUM_DEMO_ITEMS; i++)
		printf(" %3d", customerCount[i]);

	printf("\n\nCustomer totals: sales = %d, walkouts = %d, retries = %d\n\n",
	       customerSales, customerExits, customerRetries);
	printf("Simulation totals: errors = %d, warnings = %d\n\n",
	       simErrors, simWarnings);
}

/*******************************************************************************
 *
 * logTransaction - process a transaction report from a simulation task
 *
 */

void logTransaction(int sockfd_t, int printTrans)
{
	int sockfd_n;
	struct trans_msg msg;
	char *marker;
	int customerID;
	int itemID;
	int numRetries;
	int waitTime;
	int ack_code;
	int res;

	/* Create connection to transaction sender & receive message */

	sockfd_n = accept(sockfd_t, NULL, NULL);
	if (sockfd_n < 0) {
		printf("connection failure\n");
		return;
	}
	if ((res = recv(sockfd_n, &msg, sizeof(msg), MSG_DONTWAIT)) < 0) {
		printf("Error receiving transaction\n");
		goto exit;
	}

	/* Process transaction message */

	ack_code = 0;

	switch (msg.code) {
	case SIM_DONE:
		simState = STORE_EMPTY;
		break;
	case SIM_KILL:
		printf("Closing simulated store\n");
		simState = STORE_CLOSING;
		if (customersInStore > 0)
			printf("Waiting for %d customers to leave\n",
			       customersInStore);
		break;
	case SIM_ERROR:
		simErrors++;
		printf("   ERROR: %s", msg.string_area);
		break;
	case SIM_WARN:
		simWarnings++;
		printf("   WARNING: %s", msg.string_area);
		break;
	case SIM_INFO:
		if (printTrans)
			printf("%s", msg.string_area);
		break;
	case SIM_ITEM_IN:
		if (simState != STORE_OPEN) {
			ack_code = SIM_KILL;
			break;
		}
		itemsInStore++;
		sscanf(msg.string_area, "%d", &itemID);
		itemCount[itemID - DEMO_ITEM_ID_MIN]++;
		if (printTrans)
			printf("Item %s created\n", msg.string_area);
		break;
	case SIM_ITEM_OUT:
		itemsInStore--;
		if (simState == STORE_CLOSED) {
			/* don't process sales reports caused by janitor cleanup */
			ack_code = SIM_KILL;
			break;
		}
		marker = strchr(msg.string_area, ' ');
		*marker = '\0';
		sscanf(msg.string_area, "%d", &itemID);
		itemCount[itemID - DEMO_ITEM_ID_MIN]--;
		itemsSold[itemID - DEMO_ITEM_ID_MIN]++;
		if (printTrans)
			printf("Item %s given to customer from %s\n",
			       msg.string_area, marker + 1);
		if (simState != STORE_OPEN)
			ack_code = SIM_KILL;
		break;
	case SIM_CUST_IN:
		if (simState != STORE_OPEN) {
			ack_code = SIM_KILL;
			break;
		}
		customersInStore++;
		sscanf(msg.string_area, "%d %d", &itemID, &waitTime);
		customerCount[itemID - DEMO_ITEM_ID_MIN]++;
		customerID = ++customerTotal;
		if (printTrans)
			printf("Customer %d wants item %d within %d ms\n",
			       customerID, itemID, waitTime);
		ack_code = customerID;
		break;
	case SIM_CUST_OUT:
		customersInStore--;
		sscanf(msg.string_area, "%d %d %d %d",
		       &customerID, &itemID, &numRetries, &waitTime);
		marker = strchr(msg.string_area, '<');
		customerCount[itemID - DEMO_ITEM_ID_MIN]--;
		if (numRetries != 0)
			customerRetries++;
		if (marker != NULL) {
			customerSales++;
			if (printTrans)
				printf("Customer %d got item %d from %s\n",
				       customerID, itemID, marker);
		} else {
			customerExits++;
			if (printTrans)
				printf("Customer %d left without item %d "
				       "after %d ms\n",
				       customerID, itemID, waitTime);
		}
		if (simState != STORE_OPEN)
			ack_code = SIM_KILL;
		break;
	default:
		printf("Unrecognized transaction: code=%d, data='%s'\n",
		       msg.code, msg.string_area);
		ack_code = SIM_ERROR;
	}

	/* Acknowledge transaction */

	if (send(sockfd_n, &ack_code, sizeof(ack_code), 0) < 0) {
		printf("couldn't acknowledge transaction\n");
	}
exit:
	close(sockfd_n);
}

/*******************************************************************************
 *
 * simLogTask - mainline for the simulation logging task
 *
 * This routine provides a central point of control for the simulation.
 * It keeps track of the state of the simulation, and ensures that output
 * generated by the various tasks involved in the simulation do not get
 * interleaved.
 *
 */

void simLogTask(void *arg)
{
	int sockfd_t = -1;
	int sockfd_w = -1;
	int sockfd_max;
	struct sockaddr_tipc trans_srv_addr;
	struct sockaddr_tipc topsrv;
	struct tipc_subscr subscr;
	fd_set readFds;
	int printTrans = *(int *)arg;
	int i;
	int res;

	printf("Creating simulated store\n");

	/* Create socket to handle messages from other simulation tasks */

	sockfd_t = socket(AF_TIPC, SOCK_SEQPACKET, 0);
	if (sockfd_t < 0) {
		printf("Can't create socket to handle transactions\n");
		goto exit;
	}

	trans_srv_addr.family = AF_TIPC;
	trans_srv_addr.addrtype = TIPC_ADDR_NAME;
	trans_srv_addr.addr.name.name.type = TIPC_INV_SIM_TYPE;
	trans_srv_addr.addr.name.name.instance = ~0;
	trans_srv_addr.scope = TIPC_NODE_SCOPE;

	res = bind(sockfd_t, (struct sockaddr *)&trans_srv_addr,
		   sizeof(trans_srv_addr));
	if (res < 0) {
		printf("Can't bind to transaction socket\n");
		goto exit;
	}

	res = listen(sockfd_t, 5);
	if (res < 0) {
		printf("Can't listen on transaction socket\n");
		goto exit;
	}

	/* Establish connection to TIPC topology server */

	sockfd_w = socket(AF_TIPC, SOCK_SEQPACKET, 0);
	if (sockfd_w < 0) {
		printf("Can't create socket to watch for item 0\n");
		goto exit;
	}

	topsrv.family = AF_TIPC;
	topsrv.addrtype = TIPC_ADDR_NAME;
	topsrv.addr.name.name.type = TIPC_TOP_SRV;
	topsrv.addr.name.name.instance = TIPC_TOP_SRV;
	topsrv.addr.name.domain = 0;   /* TODO: should really be own node */

	if (connect(sockfd_w, (struct sockaddr *)&topsrv, sizeof(topsrv)) < 0) {
		printf("Can't connect to TIPC topology server\n");
		goto exit;
	}

	/* Subscribe to watch for item 0 */

	subscr.seq.type  = htonl(TIPC_INV_SIM_TYPE);
	subscr.seq.lower = htonl(0);
	subscr.seq.upper = htonl(0);
	subscr.timeout   = htonl(TIPC_WAIT_FOREVER);
	subscr.filter    = htonl(TIPC_SUB_SERVICE);
	subscr.usr_handle[0] = 0;

	if (send(sockfd_w, &subscr, sizeof(subscr), 0) < 0) {
		printf("Can't send bogus request to topology server\n");
		goto exit;
	}

	/* Display initial simulation state/statistics */

	for (i = 0; i < NUM_DEMO_ITEMS; i++) {
		itemsSold[i] = 0;
		itemCount[i] = 0;
		customerCount[i] = 0;
	}
	customerSales = 0;
	customerExits = 0;
	customerRetries = 0;

	simErrors = 0;
	simWarnings = 0;
	simState = STORE_OPEN;

	showSim();

	/* Now loop endlessly */

	while (1) {
		struct tipc_event event;

		/* Issue bogus subscription to force timeout event */

		subscr.seq.type  = htonl(TIPC_BOGUS_SUBSCR_TYPE);
		subscr.seq.lower = htonl(TIPC_BOGUS_SUBSCR_INST);
		subscr.seq.upper = htonl(TIPC_BOGUS_SUBSCR_INST);
		subscr.filter    = htonl(TIPC_SUB_SERVICE);
		subscr.timeout   = htonl(SIM_STATUS_INTERVAL);
		subscr.usr_handle[0]++;

		if (send(sockfd_w, &subscr, sizeof(subscr), 0) < 0) {
			printf("Can't watch for item 0\n");
			goto exit;
		}

		/* Loop until item 0 appears or it's time to print status */

		while (1) {
			/* Wait for input on either or both sockets */

			FD_ZERO(&readFds);
			FD_SET(sockfd_t, &readFds);
			FD_SET(sockfd_w, &readFds);
			sockfd_max = (sockfd_t > sockfd_w) ? sockfd_t : sockfd_w;
			if (select(sockfd_max + 1, &readFds, NULL, NULL, NULL)
			    < 0) {
				printf("Select error\n");
				goto exit;
			}

			/* Process transaction, if present */

			if (FD_ISSET(sockfd_t, &readFds)) {
				logTransaction(sockfd_t, printTrans);

				switch (simState) {
				case STORE_OPEN:
					/* continue processing normally */
					break;
				case STORE_CLOSING:
					/* create janitor once store is empty */
					if (customersInStore == 0) {
						printf("Store closed\n");
						simState = STORE_CLOSED;
						/* display final stats */
						showSim();
						childSpawn(simJanitorTask,
							   (void *)0, 0, "Janitor");
					}
					break;
				case STORE_CLOSED:
					/* wait for janitor to finish cleaning */
					break;
				case STORE_EMPTY:
					/* all done */
					printf("Store cleanup completed\n");
					if (itemsInStore != 0) {
						printf("WARNING: %d items left in store\n",
						       itemsInStore);
					}
					goto exit;
					break;
				}
			}

			/* 
			 * Process item 0 or timeout, if present
			 * (ignore withdraw events and stale timeouts;
			 * also ignore events when store is closing)
			 */

			if (!FD_ISSET(sockfd_w, &readFds))
				continue;

			if ((res = recv(sockfd_w, &event, sizeof(event), 0)) 
			    < 0) {
				printf("Error recv subscription on item 0\n");
				goto exit;
			}

			if (simState != STORE_OPEN)
				continue;

			if (event.event == htonl(TIPC_PUBLISHED))
				break;
			if ((event.event == htonl(TIPC_SUBSCR_TIMEOUT)) &&
			    (event.s.usr_handle[0] == subscr.usr_handle[0]))
				break;
		}

		/* Print simulation status */

		showSim();

		/* Halt as long as item 0 exists */

		if (event.event == htonl(TIPC_PUBLISHED)) {
			printf ("\nSimulation halted\n");
			while (1) {
				res = recv(sockfd_w, &event, sizeof(event), 0);
				if (res < 0) {
					printf("Error recv subscription"
					       " on item 0\n");
					goto exit;
				}
				if (event.event == htonl(TIPC_WITHDRAWN)) {
					break;
				}
			}
			printf("\nSimulation resumed\n");
		}
	}

exit:
	close(sockfd_w);
	close(sockfd_t);

	printf("Simulation terminated\n");
}

/*******************************************************************************
 *
 * simItem - simulated item
 *
 * This routine handles the life cycle of an item.
 *
 * RETURNS: negative value if caller should exit simulation
 *
 */

int simItem(int itemID, int lagTime, int speed)
{
	int sockfd_l;
	int sockfd_s;
	struct sockaddr_tipc addr;
	struct sockaddr_tipc self;
	socklen_t addrlen; 
	char inMsg[MSG_SIZE_MAX];
	char outMsg[MSG_SIZE_MAX];
	char *marker;
	int msgSize;
	uint zone;
	uint cluster;
	uint node;
	char itemName[8];
	int haveItem;
	int res;
	int simRes = SIM_ERROR;

	/* Select random item ID, if none specified */

	if (itemID == 0)
		itemID = randomGet(DEMO_ITEM_ID_MIN, DEMO_ITEM_ID_MAX);

	sprintf(itemName, "Item %d", itemID);

	/* Select random delay before delivering item to store, if none specified */

	if (lagTime == 0) {
		simDelay(randomGet(NEW_ITEM_WAIT_MIN, NEW_ITEM_WAIT_MAX)/speed);
	} else if (lagTime > 0)
		simDelay(lagTime/speed);

	/* Create socket to represent item */

	sockfd_l = socket(AF_TIPC, SOCK_SEQPACKET, 0);
	if (sockfd_l < 0) {
		simLog(SIM_ERROR, "%s can't create socket\n", itemName);
		return SIM_ERROR;
	}

	/* Determine own node address */

	addrlen = sizeof(addr);
	if (0 > getsockname(sockfd_l, (struct sockaddr *)&self, &addrlen)) {
		simLog(SIM_ERROR, "%s can't determine own address\n", itemName);
		goto exit;
	}

	zone = tipc_zone(self.addr.id.node);
	cluster = tipc_cluster(self.addr.id.node);
	node = tipc_node(self.addr.id.node);

	/* Publish item name */

	addr.family = AF_TIPC;
	addr.addrtype = TIPC_ADDR_NAME;
	addr.addr.name.name.type = TIPC_INV_SIM_TYPE;
	addr.addr.name.name.instance = itemID;
	addr.scope = TIPC_ZONE_SCOPE;
	addrlen = sizeof(addr);

	res = bind(sockfd_l, (struct sockaddr *) &addr, addrlen);
	if (res < 0) {
		simLog(SIM_ERROR, "%s can't publish its name\n", itemName);
		goto exit;
	}

	res = listen(sockfd_l, 5);
	if (res < 0) {
		simLog(SIM_ERROR, "%s can't listen for customers\n", itemName);
		goto exit;
	}

	/* Log item arrival; quit if simulation has ended */

	if (simLog(SIM_ITEM_IN, "%d", itemID) < 0)
		goto exit;

	/* Try selling item */

	haveItem = 1;
	while (haveItem) {

		/* Wait for a customer to request item */

		sockfd_s = accept(sockfd_l, (struct sockaddr *) &addr, &addrlen);
		if (sockfd_s < 0) {
			simLog(SIM_ERROR, "%s connection failure\n", itemName);
			break;
		}

		msgSize = recv(sockfd_s, inMsg, sizeof(inMsg), 0);
		if (msgSize <= 0) {
			simLog(SIM_WARN, "%s didn't get customer request\n",
			       itemName);
			/* Try again if customer fails to send request */
		} else {
			sprintf(outMsg, "<%d.%d.%d>%s", zone, cluster, node, inMsg);
			msgSize = strlen(outMsg) + 1;

			res = send(sockfd_s, outMsg, msgSize, 0);
			if (res != msgSize) {
				simLog(SIM_WARN, "%s couldn't reply to customer\n",
				       itemName);
				/* Try again if can't send reply to customer */
			} else {
				if ((marker = strchr(inMsg, '>')) != NULL)
					*(marker + 1) = '\0';
				simRes = simLog(SIM_ITEM_OUT, "%d %s", itemID, inMsg);
				haveItem = 0;
			}
		}

		close(sockfd_s);
	}

exit:
	close(sockfd_l);

	return simRes;
}

struct item_task_arg {
	int itemID;	/* desired item (0 = random choice) */
	int repeats;	/* # items to create (0 = once, <0 = infinite) */
	int lagTime;	/* delivery delay in ms (0 = random, <0 = immediate) */
	int speed;	/* speed multiplier */
	int taskID;	/* unique ID for item task */
};

/*******************************************************************************
 *
 * simItemTask - mainline for simulated item task
 *
 * This routine repeatedly creates items until told to stop.
 *
 */

void simItemTask(struct item_task_arg *arg)
{
	int simRes;

	do {
		simRes = simItem(arg->itemID, arg->lagTime, arg->speed);
		if (simRes < 0)
			break;
	} while ((arg->repeats < 0) || (--arg->repeats > 0));
}

/*******************************************************************************
 *
 * newItem - manually generate an item task
 *
 */

void newItem(int itemID, int repeats)
{
	struct item_task_arg arg;

	if (!simFind()) {
		printf("Simulation is not active!\n");
		return;
	}

	arg.itemID = itemID;
	arg.repeats = repeats;
	arg.lagTime = -1;	/* deliver item right away ... */
	arg.speed = 1;		/* run at normal speed */
	arg.taskID = 0;		/* associate with no specific task */

	if (arg.itemID == 0) {
		/* use random item # */
	} else if ((arg.itemID < 0) ||
	    (arg.itemID < DEMO_ITEM_ID_MIN) || 
	    (arg.itemID > DEMO_ITEM_ID_MAX)) {
		printf("Invalid item number specified\n");
		return;
	}

	childSpawn(simItemTask, &arg, sizeof(arg), "Item");
}

/*******************************************************************************
 *
 * simCust - simulated customer
 *
 * This routine handles the life cycle of a customer.
 *
 * RETURNS: negative value if caller should exit simulation
 *
 */

int simCust(int itemID, int lagTime, int waitTime, int speed, int taskID)
{
	int sockfd_c;
	int sockfd_s;
	struct tipc_subscr subscr;
	struct tipc_event event;
	struct sockaddr_tipc addr;
	struct sockaddr_tipc self;
	socklen_t addrlen; 
	char msg[MSG_SIZE_MAX];
	char *marker;
	int msgSize;
	uint zone;
	uint cluster;
	uint node;
	char custName[30];
	int transactionID;
	int needItem;
	fd_set readFds;
	struct timeval timeLimit;
	int customerID;
	int res;
	int simRes = SIM_ERROR;

	sprintf(custName, "Customer task %d", taskID);

	/* Select random item ID, if none specified */

	if (itemID == 0)
		itemID = randomGet(DEMO_ITEM_ID_MIN, DEMO_ITEM_ID_MAX);

	/* Select random delay before entering store, if none specified */

	if (lagTime == 0) {
		simDelay(randomGet(NEW_CUST_WAIT_MIN, NEW_CUST_WAIT_MAX)/speed);
	} else if (lagTime > 0)
		simDelay(lagTime/speed);

	/* Select random delay before leaving store, if none specified */

	if (waitTime == 0) {
		waitTime = randomGet(CUSTOMER_WAIT_MIN, CUSTOMER_WAIT_MAX)/speed;
	} else if (waitTime < 0)
		waitTime = 0;

	/* Log customer arrival; quit if simulation ended */

	customerID = simLog(SIM_CUST_IN, "%d %d", itemID, waitTime);
	if (customerID < 0)
		return customerID;

	/* Create socket to handle customer subscriptions to topology server */

	sockfd_s = socket(AF_TIPC, SOCK_SEQPACKET, 0);
	if (sockfd_s < 0) {
		simLog(SIM_ERROR, "%s can't create subscr socket\n", custName);
		return SIM_ERROR;
	}

	/* Determine own node address */

	addrlen = sizeof(self);
	if (getsockname(sockfd_s, (struct sockaddr *)&self, &addrlen) < 0) {
		simLog(SIM_ERROR, "%s can't determine own address\n",
		       custName);
		goto exit;
	}

	zone = tipc_zone(self.addr.id.node);
	cluster = tipc_cluster(self.addr.id.node);
	node = tipc_node(self.addr.id.node);

	/* Subscribe to item name */

	subscr.seq.type = htonl(TIPC_INV_SIM_TYPE);
	subscr.seq.lower = htonl(itemID);
	subscr.seq.upper = htonl(itemID);
	subscr.timeout = htonl(waitTime);
	subscr.filter = htonl(TIPC_SUB_PORTS);

	memset(&addr, 0, sizeof(addr));
	addr.family = AF_TIPC;
	addr.addrtype = TIPC_ADDR_NAME;
	addr.addr.name.name.type = TIPC_TOP_SRV;
	addr.addr.name.name.instance = TIPC_TOP_SRV;

	if (sendto(sockfd_s, &subscr, sizeof (subscr), 0,
		   (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		simLog(SIM_ERROR, "%s can't connect to TOP server\n", custName);
		goto exit;
	}

	/* Try acquiring item */

	transactionID = 0;
	needItem = 1;

	while (needItem) {

		/* Wait for desired item to appear */

		if (recv(sockfd_s, &event, sizeof(event), 0) != sizeof(event)) {
			simLog(SIM_ERROR, "%s subscription failure\n", custName);
			break;
		}
		if (event.event == htonl(TIPC_SUBSCR_TIMEOUT)) {
			simRes = simLog(SIM_CUST_OUT, "%d %d %d %d",
					customerID, itemID, transactionID, waitTime);
			break;
		}
		if (event.event == htonl(TIPC_WITHDRAWN)) {
			/* ignore withdraw events */
			continue;
		}

		/* Try to acquire item (using specified port id) */

		sockfd_c = socket(AF_TIPC, SOCK_SEQPACKET, 0);
		if (sockfd_c < 0) {
			simLog(SIM_ERROR, "%s can't create socket\n", custName);
			break;
		}

		addr.family = AF_TIPC;
		addr.addrtype = TIPC_ADDR_ID;
		addr.addr.id.node = ntohl(event.port.node);
		addr.addr.id.ref = ntohl(event.port.ref);
		addrlen = sizeof(addr);

		sprintf(msg, "<%d.%d.%d>[%d:%d]", zone, cluster, node,
			customerID, ++transactionID);
		msgSize = strlen(msg) + 1;

		res = sendto(sockfd_c, msg, msgSize, 0,
			     (struct sockaddr *)&addr, addrlen);
		if (res != msgSize) {
			simLog(SIM_ERROR, "%s unable to send request to item %d\n",
			       custName, itemID);
			break;
		}

		FD_ZERO(&readFds);
		FD_SET(sockfd_c, &readFds);
		timeLimit.tv_sec = (waitTime / 1000);
		timeLimit.tv_usec = (waitTime % 1000) * 1000;
		if (select(sockfd_c + 1, &readFds, NULL, NULL,
			   &timeLimit) == 0) {
			simLog(SIM_WARN, "Customer %d missed item %d "
			       "(no reply from item)\n",
			       customerID, itemID);
		} else {
			msgSize = recv(sockfd_c, msg, sizeof(msg), 0);
			if (msgSize <= 0) {
				simLog(SIM_INFO, "Customer %d missed item %d "
				       "(item rejected request)\n",
				       customerID, itemID);
			} else {
				if ((marker = strchr(msg, '>')) != NULL)
					*(marker + 1) = '\0';
				simRes = simLog(SIM_CUST_OUT, "%d %d %d %s",
						customerID, itemID,
						(transactionID - 1), msg);
				needItem = 0;
			}
		}

		close(sockfd_c);

		/* 
		 * Wait 0.5s before retrying to allow item name to be withdrawn
		 * from TIPC name table (in case item was on a slow CPU)
		 */

		if (needItem)
			simDelay(500);
	}

exit:
	close(sockfd_s);

	return simRes;
}

struct cust_task_arg {
	int itemID;     /* desired item (0 = random choice) */
	int repeats;    /* # customers to create (0 = once, <0 = infinite) */
	int lagTime;    /* arrival delay in ms (0 = random, <0 = immediate) */
	int waitTime;   /* time to wait before exiting store (in ms) */
	int speed;	/* speed multiplier */
	int taskID;     /* unique task ID for customer */
};

/*******************************************************************************
 *
 * simCustTask - mainline for simulated customer task
 *
 * This routine repeatedly creates customers until told to stop.
 *
 */

void simCustTask(struct cust_task_arg *arg)
{
	int simRes;

	do {
		simRes = simCust(arg->itemID, arg->lagTime, arg->waitTime,
				 arg->speed, arg->taskID);
		if (simRes < 0)
			break;
	} while ((arg->repeats < 0) || (--arg->repeats > 0));
}

/*******************************************************************************
 *
 * newCust - manually generate a customer task
 *
 */

void newCust(int itemID, int repeats)
{
	struct cust_task_arg arg;

	if (!simFind()) {
		printf("Simulation is not active!\n");
		return;
	}

	arg.itemID = itemID;
	arg.repeats = repeats;
	arg.lagTime = -1;	/* barge right in ... */
	arg.waitTime = 0;	/* wait random time before leaving */
	arg.speed = 1;		/* run at normal speed */
	arg.taskID = 0;		/* associate with no specific task */

	if (arg.itemID == 0) {
		/* use random item # */
	} else if ((arg.itemID < 0) ||
	    (arg.itemID < DEMO_ITEM_ID_MIN) || 
	    (arg.itemID > DEMO_ITEM_ID_MAX)) {
		printf("Invalid item number specified\n");
		return;
	}

	childSpawn(simCustTask, &arg, sizeof(arg), "Cust");
}

/*******************************************************************************
 *
 * newSim - start up a new inventory simulation on this node
 *
 */

void newSim(int numItems, int numCusts, int speed, int verbosity)
{
	int printTrans;		/* should log task print transactions? */
	int i;

	if (simFind()) {
		printf("Simulation already active!\n");
		return;
	}

	if (numItems == 0) {
		numItems = AUTO_ITEM_DEFAULT;
	}
	if (numCusts == 0) {
		numCusts = AUTO_CUST_DEFAULT;
	}

	if (speed <= 0) {
		speed = 1;
	}

	if (verbosity == 0) {
		printTrans = (speed == 1);
	} else {
		printTrans = (verbosity > 0);
	}

	/* Create logging task, then auto-generated item and customer tasks */

	childSpawn(simLogTask, &printTrans, sizeof(printTrans), "Log");

	for (i = 0; i < numItems; i++) {
		struct item_task_arg arg;

		arg.repeats = -1;	/* repeat forever */
		arg.itemID = 0;		/* create items randomly */
		arg.lagTime = 0;	/* choose pre-entry delay randomly */
		arg.speed = speed;	/* run at specified speed */
		arg.taskID = i + 1;	/* associate with a specific task */
		childSpawn(simItemTask, &arg, sizeof(arg), "Item");
	}

	for (i = 0; i < numCusts; i++) {
		struct cust_task_arg arg;

		arg.repeats = -1;	/* repeat forever */
		arg.itemID = 0;		/* choose items randomly */
		arg.lagTime = 0;	/* choose pre-entry delay randomly */
		arg.waitTime = 0;	/* set "impatience" level randomly */
		arg.speed = speed;	/* run at specified speed */
		arg.taskID = i + 1;	/* associate with a specific task */
		childSpawn(simCustTask, &arg, sizeof(arg), "Cust");
	}
}

/*******************************************************************************
 *
 * killSim - terminate simulation on this node
 *
 */

void killSim(void)
{
	if (!simFind()) {
		printf("Simulation is not active!\n");
		return;
	}

	simLog(SIM_KILL, "");
}

/*******************************************************************************
 *
 * itemZeroTask - mainline for the simulation halting task
 *
 * This routine temporarily halts the simulation on all nodes.
 *
 */

void itemZeroTask(void *arg)
{
	struct sockaddr_tipc addr;
	int addrlen;
	int sockfd_i;
	char msg;
	int res;

	/* Publish "item 0" name */
	
	sockfd_i = socket(AF_TIPC, SOCK_RDM, 0);
	if (sockfd_i < 0) {
		printf("Unable to create socket to halt simulation\n");
		return;
	}

	addr.family = AF_TIPC;
	addr.addrtype = TIPC_ADDR_NAME;
	addr.scope = TIPC_ZONE_SCOPE;
	addr.addr.name.name.type = TIPC_INV_SIM_TYPE;
	addr.addr.name.name.instance = 0;
	addr.addr.name.domain = 0;
	addrlen = sizeof(addr);

	res = bind(sockfd_i, (struct sockaddr *)&addr, addrlen);
	if (res < 0) {
		printf("Unable to publish name to halt simulation\n");
		close(sockfd_i);
		return;
	}

	/* Wait until "item 0" receives a message, then destroy item */

	res = recv(sockfd_i, &msg, sizeof(msg), 0);
	if (res < 0) {
		printf("Unable to detect request to restart simulation\n");
	}

	close(sockfd_i);
}

/*******************************************************************************
 *
 * stopSim - temporarily halt simulation on all nodes
 *
 */

void stopSim(void)
{
	/* Spawn task that halts simulation */

	childSpawn(itemZeroTask, (void *)0, 0, "Item0");
}

/*******************************************************************************
 *
 * startSim - resume simulation on all nodes
 *
 */

void startSim(void)
{
	struct sockaddr_tipc addr;
	int addrlen;
	int sockfd_c;
	char msg = '\0';
	int res;

	/* Send message to "item 0" name to trigger simulation resumption */
	
	sockfd_c = socket(AF_TIPC, SOCK_RDM, 0);
	if (sockfd_c < 0) {
		printf("Unable to create socket to resume simulation\n");
		return;
	}

	addr.family = AF_TIPC;
	addr.addrtype = TIPC_ADDR_NAME;
	addr.addr.name.name.type = TIPC_INV_SIM_TYPE;
	addr.addr.name.name.instance = 0;
	addr.addr.name.domain = 0;
	addrlen = sizeof(addr);

	res = sendto(sockfd_c, &msg, sizeof(msg), 0,
		     (struct sockaddr *)&addr, addrlen);
	if (res < 0) {
		printf("Unable to resume simulation\n");
	}

	close(sockfd_c);
}

/*******************************************************************************
 *
 * helpSim - provide simulation usage info
 *
 */

static char usage[] =
"\n"
"newSim [#items [#customers [speed [verbosity]]]]\n"
"                       - open store\n"
"killSim                - close store and clean up\n"
"stopSim                - pause simulation\n"
"startSim               - unpause simulation\n"
"newItem [item [count]] - create item (repeat as specified)\n"
"newCust [item [count]] - create customer for item (repeat as specified)\n"
"helpSim                - display this menu\n"
"\n"
"Argument missing or 0 => use default value; argument < 0 => disable\n"
"\n"
;

void helpSim(void)
{
	printf("%s", usage);
}


/*******************************************************************************
 *
 * main - mainline invokes appropriate simulation routine
 *
 */

int main(int argc, char **argv)
{
	enum {
		eNewSimCommand,
		eKillSimCommand,
		eStopSimCommand,
		eStartSimCommand,
		eNewItemCommand,
		eNewCustCommand
	};
	
	struct cmdInfo {
		int value;	/* command value */
		char *text;	/* command name */
		int maxArgs;	/* maximum # of arguments (including name) */
		};

	static struct cmdInfo cmdList[] = {
		{ eNewSimCommand, "newSim", 5 },
		{ eKillSimCommand, "killSim", 1 },
		{ eStopSimCommand, "stopSim", 1 },
		{ eStartSimCommand, "startSim", 1 },
		{ eNewItemCommand, "newItem", 3 },
		{ eNewCustCommand, "newCust", 3 },
		{ -1, NULL, -1 }
		};
	
	int cmd;

	/* Find out which command was called */

	for (cmd = 0; cmdList[cmd].text != NULL; cmd++) {
		if (strstr(argv[0], cmdList[cmd].text) != NULL)
			break;
	}

	/* Display help if command not found or too many arguments supplied */

	if (argc > cmdList[cmd].maxArgs) {
		helpSim();
		exit(1);
	}

	/* Process arguments & call associated processing function */

	switch (cmdList[cmd].value) {
	case eNewSimCommand:
		newSim((argc > 1) ? atoi(argv[1]) : 0,
		       (argc > 2) ? atoi(argv[2]) : 0,
		       (argc > 3) ? atoi(argv[3]) : 0,
		       (argc > 4) ? atoi(argv[4]) : 0);
		break;

	case eKillSimCommand:
		killSim();
		break;

	case eStopSimCommand:
		stopSim();
		break;

	case eStartSimCommand:
		startSim();
		break;

	case eNewItemCommand:
		newItem((argc > 1) ? atoi(argv[1]) : 0,
			(argc > 2) ? atoi(argv[2]) : 0);
		break;

	case eNewCustCommand:
		newCust((argc > 1) ? atoi(argv[1]) : 0,
			(argc > 2) ? atoi(argv[2]) : 0);
		break;
	}

	exit(0);
}
