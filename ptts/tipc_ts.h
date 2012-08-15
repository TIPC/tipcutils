/* ------------------------------------------------------------------------
 *
 * tipc_ts.h
 *
 * Short description: Portable TIPC Test Suite -- common declarations
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

#ifndef _PTTS_h_
#define _PTTS_h_


/*
 * include OS-specific declarations (may be empty file if none are required)
 */

#include "tipc_ts_adapt.h"


/*
 * set DO_BLAST to 1 to build test suite with blaster tests
 * (these are helpful in determining TIPC's performance)
 */

#define DO_BLAST 0

/*
 * declare items used in test routines that must be supplied by wrapper code
 */

extern int verbose;

static inline void killme(int exit_code);

/*
 * declare OS-independent items used in test routines
 */
#define TS_KILL_SERVER -1       /* Test Number sent to server to kill it */
#define TS_INVALID_TEST -2      /* INVALID Test Number */


#define TS_MSGINC    600	/* message size increment value */
#define TS_ANCBUFSZ  2048      	/* size of ancillary data area */

#define TS_SCOPE TIPC_ZONE_SCOPE;  /* or TIPC_CLUSTER_SCOPE */

#define TS_SYNCRO_TYPE 73	/* used for client-server synchronization */
#define TS_TEST_TYPE   72	/* used for client-server control messages */
#define TS_TEST_INST 1000	/* used for client-server control messages */
#define TS_LOWER 900		/* used for client-server data messages */
#define TS_UPPER 1100		/* used for client-server data messages */

#define TS_BBUF_SIZE 75000   /* size of the buffer for big stream test */
#define TS_BBUF_DATA 0x1A    /* data pattern used in big stream test */


#define debug(arg...) do {if (verbose == 2) printf(arg);} while (verbose < 0)
#define info(arg...) do {if (verbose >= 1) printf(arg);} while (verbose < 0)

#ifndef VOIDFUNCPTR
typedef void	(*VOIDFUNCPTR) ();	/* pfunction returning void */
#endif

#define TS_SYNC_ID_1	 1   /* ids to use as sync between client and server */
#define TS_SYNC_ID_2	 2
#define TS_SYNC_ID_3	 3
#define TS_SYNC_ID_4	 4
#define TS_SYNC_ID_5	 5
#define TS_SYNC_ID_6	 6
#define TS_SYNC_ID_7	 7
#define TS_SYNC_WAITING_FOR_TEST_ID 99
#define TS_SYNC_FINISHED_TEST_ID 100


#define TS_NUMTIMES  1				/* number of times the tests will loop */
#define TS_FIRST_SANITY_TEST  1	    /* start of the sanity tests */
#define TS_FIRST_STRESS_TEST  1000	/* start of the stress tests */

/**
 * TS_NUM - 	enumerated list of all valid tests, must be in sync with:
 * 		nameList[] in tipc_ts_common.c and
 * 		clientList[] in tipc_ts_client.c and
 * 		serverList[] in tipc_ts_server.c
 */

enum TS_NUM {
	ts_doAllTests = 0,
	/* add all of the sanity tests from here to ts_lastSanityTest */
	ts_dgram = TS_FIRST_SANITY_TEST,
	ts_rdm,
	ts_conn_stream,
	ts_conn_seqpacket,
	ts_shutdown_stream,
	ts_shutdown_seqpacket,
	ts_messageLimits,
	ts_importance,
	ts_socketOptions,
	ts_connection_anc,
	ts_connectionless_anc,
	ts_multicast,
	ts_stream,
	ts_bigStream,
	ts_sendto,
#if DO_BLAST
	ts_blast_rdm,
	ts_blast_seqpacket,
	ts_blast_stream,
#endif
	ts_lastSanityTest, /* must be the last sanity test */
	/* add all of the stress tests from here to ts_lastStressTest */
	ts_stress_rdm = TS_FIRST_STRESS_TEST,
	ts_lastStressTest  /* must be last */

};

/* this is the number of enumerated tests that we have defined */
#define TS_NUMBER_OF_TESTS (ts_lastSanityTest + (ts_lastStressTest - TS_FIRST_STRESS_TEST))

typedef struct {
	enum TS_NUM testNum;
	char name[50];
} TSTESTNAME;

typedef struct {
	enum TS_NUM testNum;
	VOIDFUNCPTR test;
} TSTEST;

extern TSTESTNAME nameList[];
extern TSTEST clientList[];
extern TSTEST serverList[];
extern char * testName(int test);


#endif

