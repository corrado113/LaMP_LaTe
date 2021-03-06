#ifndef LATENCYTEST_OPTIONS_H_INCLUDED
#define LATENCYTEST_OPTIONS_H_INCLUDED

#include <stdint.h>
#include <netinet/in.h>
#include "rawsock_lamp.h" // In order to import the definition of protocol_t

#define VALID_OPTS "hut:n:c:df:svlmop:reA:BC:FM:P:UL:I:W:0"
#define SUPPORTED_PROTOCOLS "[-u]"
#define INIT_CODE 0xAB

#define REPORT_RETRY_INTERVAL_MS 200
#define REPORT_RETRY_MAX_ATTEMPTS 15
#define INIT_RETRY_INTERVAL_MS 100
#define INIT_RETRY_MAX_ATTEMPTS 30
#define FOLLOWUP_CTRL_RETRY_INTERVAL_MS 100
#define FOLLOWUP_CTRL_RETRY_MAX_ATTEMPTS 30
#define CLIENT_SRCPORT 46772
#define DEFAULT_UDP_PORT 46000
#define MAX_PAYLOAD_SIZE_UDP_LAMP 1448 // Set to 1448 B since: 20 B (IP hdr) + 8 B (UDP hdr) + 24 B (LaMP hdr) + 1448 B (payload) = 1500 B (MTU)
#define RAW_RX_PACKET_BUF_SIZE (ETHERMTU+14) // Ethernet MTU (1500 B) + 14 B of struct ether_header
#define MIN_TIMEOUT_VAL_S 1000 // Minimum timeout value for the server (in ms)
#define MIN_TIMEOUT_VAL_C 3000 // Minimum timeout value for the client (in ms)
#define POLL_ERRQUEUE_WAIT_TIMEOUT 100 // Timeout for pollErrqueueWait() in common_socket_man.h/.c (in ms)

// Default client interval/server timeout values
#define CLIENT_DEF_INTERVAL 100 // [ms]
#define SERVER_DEF_TIMEOUT 4000 // [ms]

// Default number of packets
#define CLIENT_DEF_NUMBER 600 // [#]

// Number of decimal digits to be reported in the CSV file when in "-W" mode
#define W_DECIMAL_DIGITS 3 // [#]

// Default confidence interval mask
#define DEF_CONFIDENCE_INTERVAL_MASK 2

// Latency types
typedef enum {
	UNKNOWN,	// Unset latency type
	USERTOUSER,	// Userspace timestamps
	KRT,	  	// Kernel receive timestamps
	SOFTWARE, 	// Kernel receive and transmit timestamps
	HARDWARE 	// Hardware timestamps
} latencytypes_t;

typedef enum {
	UNSET_MCS,
	CLIENT,
	LOOPBACK_CLIENT,
	SERVER,
	LOOPBACK_SERVER
} modecs_t;

typedef enum {
	UNSET_MUB,
	PINGLIKE,
	UNIDIR
} modeub_t;

typedef enum {
	FOLLOWUP_OFF,
	FOLLOWUP_ON_APP,	 // Application level timestamps
	FOLLOWUP_ON_KRN_RX,	 // KRT timestamps
	FOLLOWUP_ON_KRN,	 // Kernel rx and tx timestamps -> reserved for future use (not yet implemented)
	FOLLOWUP_ON_HW		 // Hardware timestamps
} modefollowup_t;

typedef enum {
	NON_RAW,
	RAW
} moderaw_t;

struct options {
	uint8_t init;
	protocol_t protocol;
	modecs_t mode_cs;
	modeub_t mode_ub;
	moderaw_t mode_raw;
	uint64_t interval;
	uint64_t number;
	uint16_t payloadlen; // uint16_t because the LaMP len field is 16 bits long
	int macUP;
	char *filename; // Filename for the -f mode
	uint8_t overwrite; // In '-f' mode, overwrite will be = 1 if '-o' is specified (overwrite and do not append to existing file), otherwise it will be = 0 (default = 0)
	uint8_t dmode; // Set with '-d': = 1 if continuous server mode is selected, = 0 otherwise (default = 0)
	char latencyType; // Set with the option '-L': can be 'u' (user-to-user, gettimeofday() - default), 'r' (KRT, gettimeofday()+ancillary data), 'h' (hardware timestamps when supported)
	uint8_t nonwlan_mode; // = 0 if the program should bind to wireless interfaces, = 1 otherwise (default: 0)
	long if_index; // Interface index to be used (default: 0)
	uint8_t confidenceIntervalMask; // Confidence interval mask: a user shall specify xx1 to print the .90 intervals, x1x for the .95 ones and 1xx for the .99 ones (default .95 only)
	modefollowup_t followup_mode; // = FOLLOWUP_OFF if no follow-up mechanism should be used, = FOLLOWUP_ON_* otherwise (default: 0)
	uint8_t refuseFollowup; // Server only. =1 if the server should deny any follow-up request coming the client, =0 otherwise (default: 0)
	char *Wfilename; // Filename for the -W mode

	// Consider adding a union here when other protocols will be added...
	struct in_addr destIPaddr;
	unsigned long port;
	uint8_t destmacaddr[6];
};

void options_initialize(struct options *options);
unsigned int parse_options(int argc, char **argv, struct options *options);
void options_free(struct options *options);
void options_set_destIPaddr(struct options *options, struct in_addr destIPaddr);
const char * latencyTypePrinter(latencytypes_t latencyType);

#endif