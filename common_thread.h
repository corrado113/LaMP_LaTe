#ifndef LATENCYTEST_COMMONTHREAD_H_INCLUDED
#define LATENCYTEST_COMMONTHREAD_H_INCLUDED

/* ----------------- Common thread definitions ----------------- */

#include "Rawsock_lib/rawsock.h"
#include "Rawsock_lib/rawsock_lamp.h"
#include "common_socket_man.h"

#define DBG_PRINT_FLUSH(text) fprintf(stdout,"%s\n",text); \
								fflush(stdout);

#define saferecvfrom(rcvbytes,sFd,buf,n,flags,addr,addr_len) while((rcvbytes=recvfrom(sFd,buf,n,flags,addr,addr_len))==-1 && errno==EINTR)
#define saferecvmsg(rcvbytes,sFd,msghdr,flags) while((rcvbytes=recvmsg(sFd,msghdr,flags))==-1 && errno==EINTR)

// Common thread structure both for Rx and Tx
typedef struct arg_struct {
	struct lampsock_data sData;
	struct options *opts;
	char *devname;
	macaddr_t srcMAC;
	struct in_addr srcIP;
} arg_struct;

// Common thread structure for Rx
typedef struct arg_struct_rx {
	struct lampsock_data sData;
	struct options *opts;
	struct in_addr srcIP;
} arg_struct_rx;

typedef struct arg_struct_udp {
	struct lampsock_data sData;
	struct options *opts;
} arg_struct_udp;


typedef enum {
	NO_ERR,
	ERR_UNKNOWN,
	ERR_TIMERCREATE,
	ERR_SETTIMER,
	ERR_IOCTL,
	ERR_SEND,
	ERR_TIMEOUT,
	ERR_SEND_INIT,
	ERR_TIMEOUT_ACK,
	ERR_TIMEOUT_INIT,
	ERR_SEND_ACK,
	ERR_INVALID_ARG_CMONUDP,
	ERR_MALLOC,
	ERR_RECVFROM_GENERIC
} t_error_types;

void thread_error_print(const char *name, t_error_types err);

#endif