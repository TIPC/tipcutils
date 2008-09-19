/* ------------------------------------------------------------------------
 *
 * tipc_ts_client_linux.c
 *
 * Short description: Portable TIPC Test Suite -- client wrapper for Linux
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

#include "tipc_ts_common.c"
#include "tipc_ts_client.c"


int verbose;       /* global controlling the no/info/debug (0/1/2) print level */


static struct option options[] = {
	{"verbose", 0, 0, 'v'}, 
	{"kill", 0, 0, 'k'},
	{"help", 0, 0, 'h'},
	{0, 0, 0, 0}
};

char usage[] = 
	"Usage: %s [-v -k] <test>\n"
	"      -v      verbose (repeat to increase detail)\n"
	"      -k      kill servers\n"
	"      -h      print help\n"
	"      <test>  test number to run (0 => run all tests)\n";


int main(int argc, char* argv[], char* dummy[])
{
	int c;		           /* option being processed */
	int test = -1;	       /* test number set to a value we will never get */
	int killServers = 0;   /* flag for killing the server(s) default to "do not kill" */

	verbose = 0;	/* default */

	while ((c = getopt_long_only(argc, argv, "h", options, NULL)) != EOF){
		switch (c){
			case 'v':
				verbose += 1;
				break;
			case 'k':
				killServers = 1;
				break;
			case 'h':
				printf(usage, argv[0]);
				tipcTestSuiteHelp();
				exit(0);
			default:
				printf(usage, argv[0]);
				tipcTestSuiteHelp();
				exit(1);
		}
	}

	while (optind < argc) {
		test = atoi(argv[optind]);
		debug(" setting test = %d for optind = %d\n\n", test, optind);

		if (test < 0) {
			tipcTestSuiteHelp();
			exit(1);
		}

		tipcTestClient(test);
		optind++;
	}

	if (killServers) {
		tipcTestClient(TS_KILL_SERVER);
	}

	printf("TIPC test suite finished\n");

	exit(0);
}

