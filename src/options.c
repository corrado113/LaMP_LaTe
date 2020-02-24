#include "options.h"
#include "version.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <arpa/inet.h>
#include <inttypes.h>
#include "rawsock.h"

#define CSV_EXTENSION_LEN 4 // '.csv' length
#define CSV_EXTENSION_STR ".csv"
#define TXRX_STR_LEN 3 // '_tx/_rx' length
#define TX_STR ".tx"
#define RX_STR ".rx"

static const char *latencyTypes[]={"Unknown","User-to-user","KRT","Software (kernel) timestamps","Hardware timestamps"};

static void print_long_info(void) {
	fprintf(stdout,"\nUsage: %s [-c <destination address> [mode] | -l [mode] | -s | -m] [protocol] [options]\n"
		"%s [-h]: print help and information about available interfaces (including indeces)\n"
		"%s [-v]: print version information\n\n"
		"-c: client mode\n"
		"-s: server mode\n"
		"-l: loopback client mode (send packets to first loopback interface - does not support raw sockets)\n"
		"-m: loopback server mode (binds to the loopback interface - does not support raw sockets)\n\n"
		"<destination address>:\n"
		"  This is the destination address of the server. It depends on the protocol.\n"
		"  UDP: <destination address> = <destination IP address>\n"
		#if AMQP_1_0_ENABLED
		"  AMQP 1.0: <destination address> = <broker address>\n"
		#endif
		"\n"

		"[mode]:\n"
		"  Client operating mode (the server will adapt its mode depending on the incoming packets).\n"
		"  -B: ping-like bidirectional mode\n"
		"  -U: unidirectional mode (requires clocks to be perfectly synchronized - highly experimental\n"
		"\t  - use at your own risk!)\n"
		#if AMQP_1_0_ENABLED
		"\n"
		"  When using AMQP 1.0, only -U is supported. The LaMP client will act as producer and the LaMP\n"
		"\t  server as consumer. The only supported latency type is User-to-user, at the moment.\n"
		#endif
		"\n"

		"[protocol]:\n"
		"  Protocol to be used, in which the LaMP packets will be encapsulated.\n"
		"  -u: UDP\n"
		#if AMQP_1_0_ENABLED
		"  -a: AMQP 1.0 (using the Qpid Proton C library) - non raw sockets only - no loopback\n"
		#endif
		"\n"

		"[options] - Mandatory client options:\n"
		"  -M <destination MAC address>: specifies the destination MAC address.\n"
		"\t  Mandatory only if socket is RAW ('-r' is selected) and protocol is UDP.\n"
		#if AMQP_1_0_ENABLED
		"  -q <queue name>: name of an AMQP queue or topic (prepend with topic://) to be used. Client and server\n"
		"\t  shall use the same queue name. This options applies only when AMQP 1.0 is used as a protocol.\n"
		"\t  Two queues will be created: one for producer-to-consumer communication (.tx will be appended) and\n"
		"\t  one from consumer-to-producer communication (.rx will be appended.\n"
		#endif
		"\n"

		"[options] - Optional client options:\n"
		"  -n <total number of packets to be sent>: specifies how many packets to send (default: %d).\n"
		"  -t <time interval in ms>: specifies the periodicity, in milliseconds, to send at (default: %d ms).\n"
		"  -f <filename, without extension>: print the report to a CSV file other than printing\n"
		"\t  it on the screen.\n"
		"\t  The default behaviour will append to an existing file; if the file does not exist,\n" 
		"\t  it is created.\n"
		"  -o: valid only with '-f'; instead of appending to an existing file, overwrite it.\n"
		"  -P <payload size in B>: specify a LaMP payload length, in bytes, to be included inside\n"
		"\t  the packet (default: 0 B).\n"
		"  -r: use raw sockets, if supported for the current protocol.\n"
		"\t  When '-r' is set, the program tries to insert the LaMP timestamp in the last \n"
		"\t  possible instant before sending. 'sudo' (or proper permissions) is required in this case.\n"
		"  -A <access category: BK | BE | VI | VO>: forces a certain EDCA MAC access category to\n"
		"\t  be used (patched kernel required!).\n"
		"  -L <latency type: u | r | s | h>: select latency type: user-to-user, KRT (Kernel Receive Timestamp),\n"
		"\t  software kernel transmit and receive timestamps (only when supported by the NIC) or hardware\n"
		"\t  timestamps (only when supported by the NIC)\n"
		"\t  Default: u. Please note that the client supports this parameter only when in bidirectional mode.\n"
		"  -I <interface index>: instead of using the first wireless/non-wireless interface, use the one with\n"
		"\t  the specified index. The index must be >= 0. Use -h to print the valid indeces. Default value: 0.\n"
		#if AMQP_1_0_ENABLED
		"\t  This option cannot be used for AMQP 1.0 as we have no control over the binding mechanism of Qpid Proton.\n"
		#endif
		"  -e: use non-wireless interfaces instead of wireless ones. The default behaviour, without -e, is to\n"
		"\t  look for available wireless interfaces and return an error if none are found.\n"
		#if AMQP_1_0_ENABLED
		"\t  This option cannot be used for AMQP 1.0 as we have no control over the binding mechanism of Qpid Proton.\n"
		#endif
		"  -p <port>: specifies the port to be used. Can be specified only if protocol is UDP (default: %d) or AMQP.\n"
		"  -C <confidence interval mask>: specifies an integer (mask) telling the program which confidence\n"
		"\t  intervals to display (0 = none, 1 = .90, 2 = .95, 3 = .90/.95, 4= .99, 5=.90/.99, 6=.95/.99\n"
		"\t  7=.90/.95/.99 (default: %d).\n"
		"  -F: enable the LaMP follow-up mechanism. At the moment only the ping-like mode is supporting this.\n"
		"\t  This mechanism will send an additional follow-up message after each server reply, containing an\n"
		"\t  estimate of the server processing time, which is computed depending on the chosen latency type.\n"
		"  -T <time in ms>: Manually set a client timeout. The client timeout is always computed as:\n"
		"\t  (%d + x) ms if |-t value| <= %d ms or (|-t value| + x) ms if |-t value| > %d ms; with -T you can\n"
		"\t  set the 'x' value, in ms (default: %d)\n"
		"  -W <filename, without extension>: write, for the current test only, the single packet latency\n"
		"\t  measurement data to the specified CSV file. If the file already exists, data will be appended\n"
		"\t  to the file, with a new header line. Warning! This option may negatively impact performance.\n"
		"  -V: turn on verbose mode; this is currently work in progress but specifying this option will print\n"
		"\t  additional information when each test is performed. Not all modes/protocol will print more information\n"
		"\t  when this mode is activated.\n"
		"\n"

		"[options] - Mandatory server options:\n"
		#if AMQP_1_0_ENABLED
		"  -H: specify the address of the AMQP 1.0 broker.\n"
		#else
		"   <none>"
		#endif
		"\n"

		"[options] - Optional server options:\n"
		"  -t <timeout in ms>: specifies the timeout after which the connection should be\n"
		"\t  considered lost (minimum value: %d ms, otherwise %d ms will be automatically set - default: %d ms).\n"
		"  -r: use raw sockets, if supported for the current protocol.\n"
		"\t  When '-r' is set, the program tries to insert the LaMP timestamp in the last \n"
		"\t  possible instant before sending. 'sudo' (or proper permissions) is required in this case.\n"
		"  -A <access category: BK | BE | VI | VO>: forces a certain EDCA MAC access category to\n"
		"\t  be used (patched kernel required!).\n"
		"  -d: set the server in 'continuous daemon mode': as a session is terminated, the server\n"
		"\t  will be restarted and will be able to accept new packets from other clients.\n"
		"  -L <latency type: u | r>: select latency type: user-to-user or KRT (Kernel Receive Timestamp).\n"
		"\t  Default: u. Please note that the server supports this parameter only when in unidirectional mode.\n"
		"\t  If a bidirectional INIT packet is received, the mode is completely ignored.\n"
		"  -I <interface index>: instead of using the first wireless/non-wireless interface, use the one with\n"
		"\t  the specified index. The index must be >= 0. Use -h to print the valid indeces. Default value: 0.\n"
		"  -e: use non-wireless interfaces instead of wireless ones. The default behaviour, without -e, is to\n"
		"\t  look for available wireless interfaces and return an error if none are found.\n"
		"  -p <port>: specifies the port to be used. Can be specified only if protocol is UDP (default: %d) or AMQP.\n"
		"  -0: force refusing follow-up mode, even when a client is requesting to use it.\n"
		"  -1: force printing that a packet was received after sending the corresponding reply, instead of as soon as\n"
		"\t  a packet is received from the client; this can help reducing the server processing time a bit as no\n"
		"\t  'printf' is called before sending a reply.\n"
		"  -V: turn on verbose mode; this is currently work in progress but specifying this option will print\n"
		"\t  additional information when each test is performed. Not all modes/protocol will print more information\n"
		"\t  when this mode is activated.\n"
		"\n"

		"Example of usage:\n"
		"Non-RAW sockets and UDP:\n"
		"  Client (port 7000, ping-like, 200 packets, one packet every 50 ms, LaMP payload size: 700 B, user-to-user):\n"
		"\t./%s -c 192.168.1.180 -p 7000 -B -u -t 50 -n 200 -P 700\n"
		"  Server (port 7000, timeout: 5000 ms):\n"
		"\t./%s -s -p 7000 -t 5000 -u\n\n"
		"RAW sockets and UDP:\n"
		"  Client (port 7000, ping-like, 200 packets, one packet every 50 ms, LaMP payload size: 700 B, user-to-user):\n"
		"\t./%s -c 192.168.1.180 -p 7000 -B -u -t 50 -n 200 -M D8:61:62:04:9C:A2 -P 700 -r\n"
		"  Server (port 7000, timeout: 5000 ms):\n"
		"\t./%s -s -p 7000 -t 5000 -u -r\n\n"
		"Non-RAW sockets and UDP, over loopback, with default options:\n"
		"  Client (port %d, ping-like, %d packets, one packet every %d ms, LaMP payload size: 0 B, user-to-user):\n"
		"\t./%s -l -B -u\n"
		"  Server (port %d, timeout: %d ms):\n"
		"\t./%s -m -u\n\n"

		"The source code is available at:\n"
		"%s\n",
		PROG_NAME_SHORT,PROG_NAME_SHORT,PROG_NAME_SHORT, // Basic help
		CLIENT_DEF_NUMBER, // Optional client options
		CLIENT_DEF_INTERVAL, // Optional client options
		DEFAULT_LATE_PORT,DEF_CONFIDENCE_INTERVAL_MASK,MIN_TIMEOUT_VAL_C,MIN_TIMEOUT_VAL_C,MIN_TIMEOUT_VAL_C,CLIENT_DEF_TIMEOUT, // Optional client options
		MIN_TIMEOUT_VAL_S,MIN_TIMEOUT_VAL_S,SERVER_DEF_TIMEOUT, // Optional server options
		DEFAULT_LATE_PORT, // Optional server options
		PROG_NAME_SHORT,PROG_NAME_SHORT,PROG_NAME_SHORT,PROG_NAME_SHORT, // Example of usage
		DEFAULT_LATE_PORT,CLIENT_DEF_NUMBER,CLIENT_DEF_INTERVAL,PROG_NAME_SHORT, // Example of usage
		DEFAULT_LATE_PORT,SERVER_DEF_TIMEOUT,PROG_NAME_SHORT, // Example of usage
		GITHUB_LINK); // Source code link

		fprintf(stdout,"\nAvailable interfaces (use -I <index> to bind to a specific WLAN interface,\n"
			"or -I <index> -e to bind to a specific non-WLAN interface):\n");
		vifPrinter(stdout); // vifPrinter() from Rawsock library 0.2.1

	exit(EXIT_SUCCESS);
}

static void print_short_info_err(struct options *options) {
	options_free(options);

	fprintf(stdout,"\nUsage: %s [-c <destination address> [mode] | -l [mode] | -s | -m] [protocol] [options]\n"
		"%s [-h]: print help\n"
		"%s [-v]: print version information\n\n"
		"-c: client mode\n"
		"-s: server mode\n"
		"-l: loopback client mode (send packets to first loopback interface - does not support raw sockets)\n"
		"-m: loopback server mode (binds to the loopback interface - does not support raw sockets)\n\n",
		PROG_NAME_SHORT,PROG_NAME_SHORT,PROG_NAME_SHORT);

	exit(EXIT_FAILURE);
}

void options_initialize(struct options *options) {
	int i; // for loop index

	options->protocol=UNSET_P;
	options->mode_cs=UNSET_MCS;
	options->mode_ub=UNSET_MUB;
	options->interval=0;
	options->client_timeout=CLIENT_DEF_TIMEOUT;
	options->number=CLIENT_DEF_NUMBER;
	options->payloadlen=0;

	// Initial UP is set to 'UINT8_MAX', as it should not be a valid value
	// When this value is detected by the application, no setsockopt is called
	options->macUP=UINT8_MAX;

	options->init=INIT_CODE;

	// IP-UDP specific (should be inserted inside a union when other protocols will be implemented)
	options->dest_addr_u.destIPaddr.s_addr=0;
	options->port=DEFAULT_LATE_PORT;

	for(i=0;i<6;i++) {
		options->destmacaddr[i]=0x00;
	}

	options->mode_raw=NON_RAW; // NON_RAW mode is selected by default

	options->filename=NULL;
	options->overwrite=0;

	options->dmode=0;

	options->latencyType=USERTOUSER; // Default: user-to-user latency

	options->nonwlan_mode=0;
	options->if_index=0;

	options->confidenceIntervalMask=0x02; // Binary 010 as default value: i.e. print only .95 confidence intervals

	options->followup_mode=FOLLOWUP_OFF;
	options->refuseFollowup=0;

	options->verboseFlag=0;

	options->Wfilename=NULL;

	options->printAfter=0;

	options->dest_addr_u.destAddrStr=NULL;

	#if AMQP_1_0_ENABLED
	options->queueNameTx=NULL;
	options->queueNameRx=NULL;
	#endif
}

unsigned int parse_options(int argc, char **argv, struct options *options) {
	int char_option;
	int values[6];
	int i; // for loop index
	uint8_t v_flag=0; // = 1 if -v was selected, in order to exit immediately after reporting the requested information
	uint8_t M_flag=0; // = 1 if a destination MAC address was specified. If it is not, and we are running in raw server mode, report an error
	uint8_t L_flag=0; // = 1 if a latency type was explicitely defined (with -L), otherwise = 0
	uint8_t eI_flag=0; // = 1 if either -e or -I (or both) was specified, otheriwse = 0
	uint8_t C_flag=0; // = 1 if -C was specified, otheriwise = 0
	uint8_t F_flag=0; // = 1 if -F was specified, otherwise = 0
	uint8_t T_flag=0; // = 1 if -T was specified, otherwise = 0

	/* 
	   The p_flag has been inserted only for future use: it is set as a port is explicitely defined. This allows to check if a port was specified
	   for a protocol without the concept of 'port', as more protocols will be implemented in the future. In that case, it will be possible to
	   print a warning and ignore the specified value.
	*/
	uint8_t p_flag=0; // =1 if a port was explicitely specified, otherwise = 0
	char *sPtr; // String pointer for strtoul() and strtol() calls.
	size_t filenameLen=0; // Filename length for the '-f' mode

	#if AMQP_1_0_ENABLED
	size_t queueNameLen=0; // Queue name length for the '-q' mode
	uint8_t H_flag=0; // =1 if -H was specified, otherwise = 0
	#endif

	size_t destAddrLen=0; // Destination address string length

	if(options->init!=INIT_CODE) {
		fprintf(stderr,"parse_options: you are trying to parse the options without initialiting\n"
			"struct options, this is not allowed.\n");
		return 1;
	}

	while ((char_option=getopt(argc, argv, VALID_OPTS)) != EOF) {
		switch(char_option) {
			case 'h':
				print_long_info();
				break;

			case 'u':
				options->protocol=UDP;
				break;

			#if AMQP_1_0_ENABLED
			case 'a':
				options->protocol=AMQP_1_0;
				// Artificially change the default port to the AMQP default port (i.e. 5672)
				options->port=5672;
				break;
			#endif

			case 'r':
				fprintf(stderr,"Warning: root privilieges are required to use raw sockets.\n");
				options->mode_raw=RAW;
				break;

			case 'd':
				options->dmode=1;
				break;

			case 't':
				errno=0; // Setting errno to 0 as suggested in the strtoul() man page
				options->interval=strtoul(optarg,&sPtr,0);
				if(sPtr==optarg) {
					fprintf(stderr,"Cannot find any digit in the specified time interval.\n");
					print_short_info_err(options);
				} else if(errno) {
					fprintf(stderr,"Error in parsing the time interval.\n");
					print_short_info_err(options);
				}
				break;

			case 'n':
				errno=0; // Setting errno to 0 as suggested in the strtoul() man page
				options->number=strtoul(optarg,&sPtr,0);
				if(sPtr==optarg) {
					fprintf(stderr,"Cannot find any digit in the specified number of packets.\n");
					print_short_info_err(options);
				} else if(errno || options->number==0) {
					fprintf(stderr,"Error in parsing the number of packets.\n\t Please note that '0' is not accepted as value for -n).\n");
					print_short_info_err(options);
				}
				break;

			case 'c':
				if(options->mode_cs==LOOPBACK_CLIENT) {
					fprintf(stderr,"Error: normal client (-c) and loopback client (-l) are not compatible.\n");
					print_short_info_err(options);
				}

				if(options->mode_cs==UNSET_MCS) {
					options->mode_cs=CLIENT;
				} else {
					fprintf(stderr,"Error: only one option between -s|-m and -c|-l is allowed.\n");
					print_short_info_err(options);
				}

				// Parse string after -c (it will be kept as it is for AMQP 1.0, if enabled, or converted to an IP address for UDP/TCP/etc...)
				destAddrLen=strlen(optarg)+1;
				if(destAddrLen>1) {
					options->dest_addr_u.destAddrStr=malloc((destAddrLen)*sizeof(char));
					if(!options->dest_addr_u.destAddrStr) {
						fprintf(stderr,"Error in parsing the specified destinatio address: cannot allocate memory.\n");
						print_short_info_err(options);
					}
					strncpy(options->dest_addr_u.destAddrStr,optarg,destAddrLen);
				} else {
					fprintf(stderr,"Error in parsing the filename: null string length.\n");
					print_short_info_err(options);
				}
				break;

			case 's':
				if(options->mode_cs==LOOPBACK_SERVER) {
					fprintf(stderr,"Error: normal server (-s) and loopback server (-m) are not compatible.\n");
					print_short_info_err(options);
				}

				if(options->mode_cs==UNSET_MCS) {
					options->mode_cs=SERVER;
				} else {
					fprintf(stderr,"Error: only one option between -s|-m and -c|-l is allowed.\n");
					print_short_info_err(options);
				}
				break;

			case 'l':
				if(options->mode_cs==CLIENT) {
					fprintf(stderr,"Error: normal client (-c) and loopback client (-l) are not compatible.\n");
					print_short_info_err(options);
				}

				if(options->mode_cs==UNSET_MCS) {
					options->mode_cs=LOOPBACK_CLIENT;
				} else {
					fprintf(stderr,"Error: only one option between -s|-m and -c|-l is allowed.\n");
					print_short_info_err(options);
				}
				break;

			case 'm':
				if(options->mode_cs==SERVER) {
					fprintf(stderr,"Error: normal server (-s) and loopback server (-m) are not compatible.\n");
					print_short_info_err(options);
				}

				if(options->mode_cs==UNSET_MCS) {
					options->mode_cs=LOOPBACK_SERVER;
				} else {
					fprintf(stderr,"Error: only one option between -s|-m and -c|-l is allowed.\n");
					print_short_info_err(options);
				}
				break;

			case 'p':
				errno=0; // Setting errno to 0 as suggested in the strtol() man page
				options->port=strtoul(optarg,&sPtr,0);

				// Only if AMQP 1.0 is active, save a string with the port too, as it can be useful later on, without the need of converting
				// this value from unsigned long to char* again
				#if AMQP_1_0_ENABLED
				strncpy(options->portStr,optarg,MAX_PORT_STR_SIZE);
				#endif

				if(sPtr==optarg) {
					fprintf(stderr,"Cannot find any digit in the specified port.\n");
					print_short_info_err(options);
				} else if(errno || options->port<1 || options->port>65535) {
					fprintf(stderr,"Error in parsing the port.\n");
					print_short_info_err(options);
				}
				// If the UDP source port is equal to the fixed one we chose for the client,
				// print a warning and use CLIENT_SRCPORT+1
				if(options->port==CLIENT_SRCPORT) {
					options->port=CLIENT_SRCPORT+1;
					fprintf(stderr,"Port cannot be equal to the raw client source port (%d). %ld will be used instead.\n",CLIENT_SRCPORT,options->port);
				}

				p_flag=1;
				break;

			case 'f':
				filenameLen=strlen(optarg)+1;
				if(filenameLen>1) {
					options->filename=malloc((filenameLen+CSV_EXTENSION_LEN)*sizeof(char));
					if(!options->filename) {
						fprintf(stderr,"Error in parsing the filename: cannot allocate memory.\n");
						print_short_info_err(options);
					}
					strncpy(options->filename,optarg,filenameLen);
					strncat(options->filename,CSV_EXTENSION_STR,CSV_EXTENSION_LEN);
				} else {
					fprintf(stderr,"Error in parsing the filename: null string length.\n");
					print_short_info_err(options);
				}
				break;

			case 'o':
				options->overwrite=1;
				break;

			case 'e':
				options->nonwlan_mode=1;
				eI_flag=1;
				break;

			case 'v':
				fprintf(stdout,"%s, version %s, date %s\n",PROG_NAME_LONG,VERSION,DATE);
				v_flag=1;
				break;

			#if AMQP_1_0_ENABLED
			case 'q':
				queueNameLen=strlen(optarg)+1;
				if(queueNameLen>1) {
					options->queueNameTx=malloc((queueNameLen+TXRX_STR_LEN)*sizeof(char));
					options->queueNameRx=malloc((queueNameLen+TXRX_STR_LEN)*sizeof(char));
					if(!options->queueNameTx || !options->queueNameRx) {
						fprintf(stderr,"Error in parsing the specified queue name: cannot allocate memory.\n");
						print_short_info_err(options);
					}
					strncpy(options->queueNameTx,optarg,queueNameLen);
					strncat(options->queueNameTx,TX_STR,TXRX_STR_LEN);

					strncpy(options->queueNameRx,optarg,queueNameLen);
					strncat(options->queueNameRx,RX_STR,TXRX_STR_LEN);
				} else {
					fprintf(stderr,"Error in parsing the queue name: null string length.\n");
					print_short_info_err(options);
				}
				break;
			#endif

			case 'A':
				// This requires a patched kernel: print a warning!
				fprintf(stderr,"Warning: the use of the -A option requires a patched kernel.\n"
					"See: https://github.com/francescoraves483/OpenWrt-V2X\n"
					"Or: https://github.com/florianklingler/OpenC2X-embedded.\n");
				// Mapping between UP (0 to 7) and AC (BK to VO)
				if(strcmp(optarg,"BK") == 0) {
					options->macUP=1; // UP=1 (2) is AC_BK
				} else if(strcmp(optarg,"BE") == 0) {
					options->macUP=0; // UP=0 (3) is AC_BE
				} else if(strcmp(optarg,"VI") == 0) {
					options->macUP=4; // UP=4 (5) is AC_VI
				} else if(strcmp(optarg,"VO") == 0) {
					options->macUP=6; // UP=6 (7) is AC_VO
				} else {
				// Leave to default (UINT8_MAX), i.e. no AC is set to socket, and print error
					fprintf(stderr, "Invalid AC specified with -A.\nValid ones are: BK, BE, VI, VO.\nNo AC will be set.\n");
				}
				break;

			case 'B':
				if(options->mode_ub==UNSET_MUB) {
					options->mode_ub=PINGLIKE;
				} else {
					fprintf(stderr,"Only one option between -B and -U is allowed.\n");
					print_short_info_err(options);
				}
				break;

			case 'C':
				errno=0; // Setting errno to 0 as suggested in the strtol() man page
				options->confidenceIntervalMask=strtoul(optarg,&sPtr,0);
				if(sPtr==optarg) {
					fprintf(stderr,"Cannot find any digit in the specified mask.\n");
					print_short_info_err(options);
				} else if(errno || options->confidenceIntervalMask>0x07) {
					fprintf(stderr,"Error in parsing the mask specified after -C.\n");
					print_short_info_err(options);
				}
				C_flag=1;
				break;

			case 'F':
				F_flag=1;
				break;

			case 'M':
				if(sscanf(optarg,SCN_MAC,MAC_SCANNER(values))!=6) {
					fprintf(stderr,"Error when reading the destination MAC address after -M.\n");
					print_short_info_err(options);
				}
				for(i=0;i<6;i++) {
					options->destmacaddr[i]=(uint8_t) values[i];
				}
				M_flag=1;
				break;

			case 'P':
				if(sscanf(optarg,"%" SCNu16, &(options->payloadlen))==EOF) {
					fprintf(stderr,"Error when reading the payload length after -P.\n");
					print_short_info_err(options);
				}
				break;

			case 'U':
				if(options->mode_ub==UNSET_MUB) {
					options->mode_ub=UNIDIR;
				} else {
					fprintf(stderr,"Only one option between -B and -U is allowed.\n");
					print_short_info_err(options);
				}
				fprintf(stderr,"Warning: the use of the -U option requires the system clock to be perfectly\n"
					"synchronized with a common reference.\n"
					"In case you are not sure if the clock is synchronized, please use -B instead.\n");
				break;

			case 'V':
				options->verboseFlag=1;
				break;

			case 'L':
				if(strlen(optarg)!=1) {
					fprintf(stderr,"Error: only one character shall be specified after -L.\n");
					print_short_info_err(options);
				}

				if(optarg[0]!='r' && optarg[0]!='u' && optarg[0]!='s' && optarg[0]!='h') {
					fprintf(stderr,"Error: valid -L options: 'u', 'r', 'h' (not supported on every NIC).\n");
					print_short_info_err(options);
				}

				switch(optarg[0]) {
					case 'u':
						options->latencyType=USERTOUSER;
						break;

					case 'r':
						options->latencyType=KRT;
						break;

					case 's':
						options->latencyType=SOFTWARE;
						break;

					case 'h':
						options->latencyType=HARDWARE;
						break;

					default:
						options->latencyType=UNKNOWN;
					break;
				}

				L_flag=1;

				break;

			case 'I':
				errno=0; // Setting errno to 0 as suggested in the strtol() man page
				options->if_index=strtol(optarg,&sPtr,0);
				if(sPtr==optarg) {
					fprintf(stderr,"Cannot find any digit in the specified interface index.\n");
					print_short_info_err(options);
				} else if(errno || options->if_index<0) {
					fprintf(stderr,"Error: invalid interface index.\n");
					print_short_info_err(options);
				}
				eI_flag=1;
				break;

			case 'W':
				filenameLen=strlen(optarg)+1;
				if(filenameLen>1) {
					options->Wfilename=malloc((filenameLen+CSV_EXTENSION_LEN)*sizeof(char));
					if(!options->Wfilename) {
						fprintf(stderr,"Error in parsing the filename for the -T mode: cannot allocate memory.\n");
						print_short_info_err(options);
					}
					strncpy(options->Wfilename,optarg,filenameLen);
					strncat(options->Wfilename,CSV_EXTENSION_STR,CSV_EXTENSION_LEN);
				} else {
					fprintf(stderr,"Error in parsing the filename for the -T mode: null string length.\n");
					print_short_info_err(options);
				}
				break;

			case 'T':
				errno=0;
				options->client_timeout=strtoul(optarg,&sPtr,0);
				if(sPtr==optarg) {
					fprintf(stderr,"Cannot find any digit in the specified time interval.\n");
					print_short_info_err(options);
				} else if(errno) {
					fprintf(stderr,"Error in parsing the client timeout value.\n");
					print_short_info_err(options);
				}
				T_flag=1;
				break;

			#if AMQP_1_0_ENABLED
			case 'H':
				// Parse string after -H (AMQP 1.0 only)
				destAddrLen=strlen(optarg)+1;
				if(destAddrLen>1) {
					options->dest_addr_u.destAddrStr=malloc((destAddrLen)*sizeof(char));
					if(!options->dest_addr_u.destAddrStr) {
						fprintf(stderr,"Error in parsing the specified destination address: cannot allocate memory.\n");
						print_short_info_err(options);
					}
					strncpy(options->dest_addr_u.destAddrStr,optarg,destAddrLen);
				} else {
					fprintf(stderr,"Error in parsing the destination address: null string length.\n");
					print_short_info_err(options);
				}

				H_flag=1;
			#endif

			case '0':
				options->refuseFollowup=1;
				break;

			case '1':
				options->printAfter=1;
				break;

			default:
				print_short_info_err(options);

		}

	}

	if(v_flag) {
		exit(EXIT_SUCCESS); // Exit with SUCCESS code if -v was selected
	}

	if(options->mode_cs==UNSET_MCS) {
		fprintf(stderr,"Error: a mode must be specified, either client (-c) or server (-s).\n");
		print_short_info_err(options);
	} else if(options->mode_cs==CLIENT) {
		// Parse destination IP address if protocol is not AMQP
		if(options->protocol==UDP) {
			struct in_addr destIPaddr_tmp;
			if(inet_pton(AF_INET,options->dest_addr_u.destAddrStr,&(destIPaddr_tmp))!=1) {
				fprintf(stderr,"Error in parsing the destination IP address (required with -s).\n");
				print_short_info_err(options);
			}
			free(options->dest_addr_u.destAddrStr);

			options->dest_addr_u.destIPaddr=destIPaddr_tmp;
		}

		if(options->mode_raw==RAW && M_flag==0) {
			fprintf(stderr,"Error: in this initial version, the raw client requires the destionation MAC address too (with -M).\n");
			print_short_info_err(options);
		}
		if(options->protocol==UDP && options->dest_addr_u.destIPaddr.s_addr==0) {
			fprintf(stderr,"Error: when in UDP client mode, an IP address should be correctly specified.\n");
			print_short_info_err(options);
		}
		if(options->mode_ub==UNSET_MUB) {
			fprintf(stderr,"Error: in client mode either ping-like (-B) or unidirectional (-U) communication should be specified.\n");
			print_short_info_err(options);
		}
		if(options->printAfter==1) {
			fprintf(stderr,"Warning: -1 was specified but it will be ignored, as it is a server only option.\n");
		}
	} else if(options->mode_cs==SERVER) {
		if(options->mode_ub!=UNSET_MUB) {
			fprintf(stderr,"Warning: -B or -U was specified, but in server (-s) mode these parameters are ignored.\n");
		}
		if(C_flag==1) {
			fprintf(stderr,"Warning: -C is a client-only option. It will be ignored.\n");
		}
		if(T_flag==1) {
			fprintf(stderr,"Warning: -T is a client-only option. It will be ignored.\n");
		}
	} else if(options->mode_cs==LOOPBACK_CLIENT) {
		if(eI_flag==1) {
			fprintf(stderr,"Error: -I/-e are not supported when using loopback interfaces, as only one interface is used.\n");
			print_short_info_err(options);
		}
		if(options->mode_raw==RAW) {
			fprintf(stderr,"Error: raw sockets are not supported in loopback clients.\n");
			print_short_info_err(options);
		}
		if(options->mode_ub==UNSET_MUB) {
			fprintf(stderr,"Error: in loopback client mode either ping-like (-B) or unidirectional (-U) communication should be specified.\n");
			print_short_info_err(options);
		}
		if(options->printAfter==1) {
			fprintf(stderr,"Warning: -1 was specified but it will be ignored, as it is a server only option.\n");
		}
	} else if(options->mode_cs==LOOPBACK_SERVER) {
		if(eI_flag==1) {
			fprintf(stderr,"Error: -I/-e are not supported when using loopback interfaces, as only one interface is used.\n");
			print_short_info_err(options);
		}
		if(options->mode_raw==RAW) {
			fprintf(stderr,"Error: raw sockets are not supported in loopback servers.\n");
			print_short_info_err(options);
		}
		if(options->mode_ub!=UNSET_MUB) {
			fprintf(stderr,"Warning: -B or -U was specified, but in loopback server (-m) mode these parameters are ignored.\n");
		}
		if(C_flag==1) {
			fprintf(stderr,"Warning: -C is a client-only option. It will be ignored.\n");
		}
		if(T_flag==1) {
			fprintf(stderr,"Warning: -T is a client-only option. It will be ignored.\n");
		}
	}

	#if AMQP_1_0_ENABLED
	// No raw sockets are supported for AMQP 1.0
	if(options->protocol==AMQP_1_0) {
		if(options->mode_raw==RAW) {
			fprintf(stderr,"Error: AMQP 1.0 does not support raw sockets yet.\n");
			print_short_info_err(options);
		}

		if(options->mode_cs==CLIENT && H_flag==1) {
			fprintf(stderr,"Error: -H is a server only AMQP 1.0 option.\n");
			print_short_info_err(options);
		}

		if(options->mode_cs==SERVER && H_flag==0) {
			fprintf(stderr,"Error: running in LaMP server mode but no broker address specified.\n");
			fprintf(stderr,"Please specify one with -H.\n");
			print_short_info_err(options);
		}

		// No -e or -I can be specified (print a warning telling that they will be ignored)
		if(eI_flag==1) {
			fprintf(stderr,"Warning: -e or -I was specified but it will be ignored.\n");
		}

		// Only undirectional mode is supported as of now (with consumer/producer which can be run
		//  on the same PC, or on different devices if they have a synchronized clock)
		if(options->mode_ub==PINGLIKE) {
			fprintf(stderr,"Error: AMQP 1.0 does not support bidirectional mode yet.\n");
			print_short_info_err(options);
		}

		// No loopback client is supported for AMQP 1.0
		if(options->mode_cs==LOOPBACK_CLIENT || options->mode_cs==LOOPBACK_SERVER) {
			fprintf(stderr,"Error: AMQP 1.0 does not support loopback clients/servers.\n");
			print_short_info_err(options);
		}

		if(options->macUP!=UINT8_MAX) {
			fprintf(stderr,"Warning: no priority can be set for AMQP 1.0 packets for the time being.\n");
			fprintf(stderr,"\t The specified Access Category will be ignored.\n");
		}

		// Only user-to-user latency is supported with AMQP 1.0, for the moment
		if(options->latencyType!=USERTOUSER) {
			fprintf(stderr,"Error: you specified latency type '%c' but only user-to-user is supported with AMQP 1.0.\n",options->latencyType);
			print_short_info_err(options);
		}
	}
	#endif

	if(options->interval==0) {
		if(options->mode_cs==CLIENT || options->mode_cs==LOOPBACK_CLIENT) {
			// Set the default periodicity value if no explicit value was defined
			options->interval=CLIENT_DEF_INTERVAL;
		} else if(options->mode_cs==SERVER || options->mode_cs==LOOPBACK_SERVER) {
			// Set the default timeout value if no explicit value was defined
			options->interval=SERVER_DEF_TIMEOUT;
		}
	}

	// Important note: when adding futher protocols that cannot support, somehow, raw sockets, always check for -r not being set

	// Check for -L and -B/-U consistency (-L supported only with -B in clients, -L supported only with -U in servers, otherwise, it is ignored)
	if(L_flag==1 && (options->mode_cs==CLIENT || options->mode_cs==LOOPBACK_CLIENT) && options->mode_ub!=PINGLIKE) {
		fprintf(stderr,"Error: latency type can be specified only when the client is working in ping-like mode (-B).\n");
		print_short_info_err(options);
	}

	// -L h cannot be specified in unidirectional mode (i.e. a server can never specify 'h' as latency type)
	if((options->mode_cs==SERVER || options->mode_cs==LOOPBACK_SERVER) && options->latencyType==HARDWARE) {
		fprintf(stderr,"Error: hardware timestamps are only supported by clients and in ping-like mode (-B).\n");
		print_short_info_err(options);
	}

	// -L s cannot be specified in unidirectional mode (i.e. a server can never specify 's' as latency type)
	if((options->mode_cs==SERVER || options->mode_cs==LOOPBACK_SERVER) && options->latencyType==SOFTWARE) {
		fprintf(stderr,"Error: kernel receive/transmit timestamps are only supported by clients and in ping-like mode (-B).\n");
		print_short_info_err(options);
	}

	// Check consistency between parameters
	if((options->mode_cs==CLIENT || options->mode_cs==LOOPBACK_CLIENT) && options->refuseFollowup==1) {
		fprintf(stderr,"Error: -0 (refuse follow-up requests) is a server only option.");
		print_short_info_err(options);
	}

	if(options->protocol==UNSET_P) {
		fprintf(stderr,"Error: a protocol must be specified. Supported protocols: %s\n",SUPPORTED_PROTOCOLS);
		print_short_info_err(options);
	}

	if(options->protocol==UDP && options->payloadlen>MAX_PAYLOAD_SIZE_UDP_LAMP) {
		fprintf(stderr,"Error: the specified LaMP payload length is too big.\nPayloads up to %d B are supported with UDP.\n",MAX_PAYLOAD_SIZE_UDP_LAMP);
		print_short_info_err(options);
	}

	if(options->overwrite==1 && options->filename==NULL) {
		fprintf(stderr,"Error: '-o' (overwrite mode) can be specified only when the output to a file (with -f) is requested.\n");
		print_short_info_err(options);
	}

	if(options->filename!=NULL && options->mode_cs==SERVER) {
		fprintf(stderr,"Error: '-f' is client-only, since only the client can print reports in the current version.\n");
		print_short_info_err(options);
	}

	if((options->mode_cs==SERVER || options->mode_cs==LOOPBACK_SERVER) && F_flag==1) {
		fprintf(stderr,"Error: '-F' is client-only.\n");
		print_short_info_err(options);
	}

	if(options->mode_ub==UNIDIR && F_flag==1) {
		fprintf(stderr,"Error: '-F' is supported in ping-like mode only, at the moment.\n");
		print_short_info_err(options);
	}

	// Print a warning in case a MAC address was specified with -M in UDP non raw mode, as the MAC is obtained through ARP and this argument will be ignored
	if(options->protocol==UDP && options->mode_raw==NON_RAW && M_flag==1) {
		fprintf(stderr,"Warning: a destination MAC address has been specified, but it will be ignored and obtained through ARP.\n");
	}

	// Get the correct follow-up mode, depending on the current latency type, if F_flag=1 (i.e. if -F was specified)
	if(F_flag==1) {
		switch(options->latencyType) {
			case USERTOUSER:
				options->followup_mode=FOLLOWUP_ON_APP;
			break;

			case KRT:
				options->followup_mode=FOLLOWUP_ON_KRN_RX;
			break;

			case SOFTWARE:
				options->followup_mode=FOLLOWUP_ON_KRN;
			break;

			case HARDWARE:
				options->followup_mode=FOLLOWUP_ON_HW;
			break;

			default:
				fprintf(stderr,"Warning: unknown error. No follow-up mechanism will be activated.\n");
				options->followup_mode=FOLLOWUP_OFF;
			break;
		}
	}

	return 0;
}

void options_free(struct options *options) {
	if(options->filename) {
		free(options->filename);
	}

	if(options->Wfilename) {
		free(options->Wfilename);
	}

	#if AMQP_1_0_ENABLED
	if(options->queueNameTx) {
		free(options->queueNameTx);
	}

	if(options->queueNameRx) {
		free(options->queueNameRx);
	}

	if(options->protocol==AMQP_1_0 && options->dest_addr_u.destAddrStr) {
		free(options->dest_addr_u.destAddrStr);
	}
	#endif
}

void options_set_destIPaddr(struct options *options, struct in_addr destIPaddr) {
	options->dest_addr_u.destIPaddr=destIPaddr;
}

const char * latencyTypePrinter(latencytypes_t latencyType) {
	// enum can be used as index array, provided that the order inside the definition of latencytypes_t is the same as the one inside latencyTypes[]
	return latencyTypes[latencyType];
}