/*
 * tipc_hello_server.c: TIPC "hello world" server module
 *
 * Copyright (c) 2003-2006, Ericsson AB
 * Copyright (c) 2005-2006, Wind River Systems
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the names of the copyright holders nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
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
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/uio.h>

#include <net/tipc/tipc.h>

#define SERVER_TYPE  18888
#define SERVER_INST  17

static u32 user_ref = 0;

/**
 * named_msg_event - handle incoming message
 */

static void named_msg_event(void *usr_handle,
			    u32 port_ref,
			    struct sk_buff **buf,
			    unsigned char const *data,
			    unsigned int size,
			    unsigned int importance, 
			    struct tipc_portid const *orig,
			    struct tipc_name_seq const *dest)
{
	char outbuf[] = "Uh ?";
	struct iovec my_iov;

	/* Display contents of incoming message */

	printk("Server: Message received: %s !\n", data);

	/* Send reply back to originator */

	my_iov.iov_base = outbuf;
	my_iov.iov_len = sizeof(outbuf);
	tipc_send2port(port_ref, orig, 1, &my_iov);
}

/**
 * tipc_hello_init - initialization code for "hello world" server
 * 
 * Registers as a new TIPC user, then creates a port and binds a name to it.
 * If errors occur, simply bails out; cleanup is done by exit routine.
 */

static int __init tipc_hello_init(void)
{
	struct tipc_name_seq seq;
	u32 port_ref;
	int res;

	res = tipc_attach(&user_ref, NULL, NULL);
	if (res)
		return res;

	res = tipc_createport(user_ref, NULL, TIPC_LOW_IMPORTANCE,
			      NULL, NULL, NULL,
			      NULL, named_msg_event, NULL,
			      NULL, &port_ref);
	if (res)
		return res;

	seq.type = SERVER_TYPE;
	seq.lower = seq.upper = SERVER_INST;
	res = tipc_publish(port_ref, TIPC_CLUSTER_SCOPE, &seq);

	return res;
}

/**
 * tipc_hello_exit - termination code for "hello world" server
 * 
 * Simply deregisters as TIPC user, thereby deleting all associated ports.
 * Automatically handles cases where user wasn't actually registered during
 * module initialization, or port creation failed.
 */

static void __exit tipc_hello_exit(void)
{
	tipc_detach(user_ref);
}

/**
 * module info
 */

module_init(tipc_hello_init);
module_exit(tipc_hello_exit);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Al Stephens <allan.stephens@windriver.com>");
MODULE_DESCRIPTION("TIPC Hello World Server");
MODULE_VERSION("1.0");

