#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <sys/param.h>
#include <sys/poll.h>
#include <netdb.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/tipc.h>

#define SERVER_TYPE 18888
#define TIMEOUT 500000
#define FOREVER ~0

static void print_sub(const char* str, struct tipc_subscr* s)
{
        printf ("%s Subscription:<%u,%u,%u>, timeout %u, user ref: %x%x\n",
                str,s->seq.type,s->seq.lower,s->seq.upper,
                s->timeout,((uint*)s->usr_handle)[0],((uint*)s->usr_handle)[1]);
}

static void print_evt(const char* str,struct tipc_event* evt)
{
        printf("%s Event: ",str);
        if (evt->event == TIPC_PUBLISHED)   
                printf("Published: ");
        else if (evt->event == TIPC_WITHDRAWN)
                printf("Withdrawn: ");
        else if (evt->event == TIPC_SUBSCR_TIMEOUT)
                printf("Timeout: ");
        else 
                printf("Unknown, evt = %i ",evt->event);

        printf (" <%u,%u,%u> port id <%x:%u>\n",
                evt->s.seq.type,evt->found_lower,
                evt->found_upper,evt->port.node,
                evt->port.ref);
        print_sub("Original ",&evt->s);
        if (evt->s.seq.type == 0)
                printf(" ...For node %x \n",evt->found_lower);
}

uint own_node(int sd)
{
        struct sockaddr_tipc addr;
        socklen_t sz = sizeof(addr);

        if (0 > getsockname(sd,(struct sockaddr*)&addr,&sz)){
                perror("Failed to get sock address\n");
                exit(1);
        }
        return addr.addr.id.node;
}

int main(int argc, char* argv[], char* dummy[])
{
        int sd;

        struct tipc_subscr subscr = {{SERVER_TYPE,0,100},
                                     TIMEOUT,
                                     TIPC_SUB_SERVICE,
                                     {2,2,2,2,2,2,2,2}};
        struct tipc_subscr net_subscr = {{0,0,~0},
                                         FOREVER,
                                         TIPC_SUB_PORTS,
                                         {3,3,3,3,3,3,3,3}};
        struct tipc_event event;
        struct sockaddr_tipc topsrv;

        memset(&topsrv, 0, sizeof(topsrv));
	topsrv.family = AF_TIPC;
        topsrv.addrtype = TIPC_ADDR_NAME;
        topsrv.addr.name.name.type = TIPC_TOP_SRV;
        topsrv.addr.name.name.instance = TIPC_TOP_SRV;

	sd = socket (AF_TIPC, SOCK_SEQPACKET,0);
        assert(sd >= 0);
        
	printf("TIPC Topology subscriber started\n");

        if (argc>1)
                subscr.seq.type = atoi(argv[1]);
        if (argc>2)
                subscr.timeout = atoi(argv[2]);
        if (argc>3)
                   *(uint*)subscr.usr_handle=atoi(argv[3]);

        /* Connect to topology server: */

        if (0 > connect(sd,(struct sockaddr*)&topsrv,sizeof(topsrv))){
                perror("failed to connect to topology server");
                exit(1);
        }
        printf("Connected to topology server\n");

        /* Name subscription: */

        if (send(sd,&subscr,sizeof(subscr),0) != sizeof(subscr)){
                perror("failed to send subscription");
        }
        print_sub("Sent ",&subscr);

        /* Network subscripton: */

        if (send(sd,&net_subscr,sizeof(net_subscr),0) != sizeof(net_subscr)){
                perror("failed to send network subscription");
        }
        print_sub("Sent Network ",&net_subscr);

        /* Now wait for the subscriptions to fire: */
        while(recv(sd,&event,sizeof(event),0) == sizeof(event)){
                print_evt("Received ",&event);
        }
        perror("Failed to receive event");
	exit(0);
}
