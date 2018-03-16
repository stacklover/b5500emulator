/***********************************************************************
* b5500emulator
************************************************************************
* Copyright (c) 2018, Reinhard Meyer, DL5UY
* Licensed under the MIT License,
*       see LICENSE
************************************************************************
* b9353 Input and Display System
*
************************************************************************
* 2018-03-12  R.Meyer
*   Initial Version
***********************************************************************/

#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <signal.h>
#include <arpa/inet.h>

#define	BUFLEN 112
#define	BUFLEN2 200

#define	NUL	0x00
#define	STX	0x02
#define	ETX	0x03
#define	EOT	0x04
#define	ENQ	0x05
#define	ACK	0x06
#define	BEL	0x07
#define	BS	0x08
#define	TAB	0x09
#define	LF	0x0a
#define	FF	0x0c
#define	CR	0x0d
#define	DC1	0x11
#define	DC2	0x12
#define	DC3	0x13
#define	DC4	0x14
#define	NAK	0x15

/***********************************************************************
* the data
***********************************************************************/

// telnet
static int sock = -1;

// user input
static pthread_t user_input_handler;
static char inbuf[BUFLEN];
static volatile char *inp = NULL;

// traces
int ctrace = false;
int ptrace = false;

/***********************************************************************
* Socket Close
***********************************************************************/
static void socket_close(void) {
	int so = sock;
	sock = -1;		// prevent recursion
	if (so > 2) {
		close(so);
		fprintf(stderr, "socket %d closed\n", so);
	}
}

/***********************************************************************
* Socket Read
***********************************************************************/
static int socket_read(char *buf, int len) {
	if (sock > 2) {
		int cnt = read(sock, buf, len);
		if (cnt < 0) {
			if (errno == EAGAIN)
				return -1;
			perror("read failed - closing");
			socket_close();
		}
		return cnt;
	}
	errno = ECONNRESET;
	return -1;
}

/***********************************************************************
* Socket Write
***********************************************************************/
static int socket_write(const char *buf, int len) {
	if (sock > 2) {
		int cnt = write(sock, buf, len);
		if (cnt < 0) {
			if (errno == EAGAIN)
				return -1;
			perror("write failed - closing");
			socket_close();
		}
		return cnt;
	}
	errno = ECONNRESET;
	return -1;
}

/***********************************************************************
* Socket Open
***********************************************************************/
static int socket_open(const char *host, unsigned port) {
	struct sockaddr_in addr;

	// create socket for connect
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		perror("telnet socket");
		return errno;
	}
	// evaluate (numeric!) host/port
	bzero((char*)&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	if (inet_pton(AF_INET, host, &addr.sin_addr) < 0) {
		perror("telnet hostname");
		close(sock);
		sock = -1;
		return errno;
	}
	// start the connect
	if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
		perror("telnet connect");
		close(sock);
		sock = -1;
		return errno;
	}
	// now set to non-blocking
	// all good
	return 0;
}

/***********************************************************************
* User Input Thread Function
***********************************************************************/
static void *user_input_function(void *v) {
	char buf[5];
	char *p;
loop:
	inp = fgets(inbuf, sizeof inbuf, stdin);
	if (inp != NULL) {
		// send ENQ
		p = buf;
		*p++ = ENQ;
		if (ptrace)
			printf("[ENQ]\n");
		if (socket_write(buf, p-buf) < p-buf)
			inp = NULL;
	}
	goto loop;
	return NULL;	// compiler requires this!	
}

/***********************************************************************
* Main Program
***********************************************************************/
int main(int argc, char*argv[]) {
	char buf[BUFLEN2];
	char buf2[BUFLEN2];
	char *p;
	int len, i;
	int intext = false;
        int opt;
	const char *server = "127.0.0.1";
	unsigned port = 8023;

        while ((opt = getopt(argc, argv, "CPs:p:")) != -1) {
                switch (opt) {
                case 'C':
                        ctrace = true;
                        break;
                case 'P':
                        ptrace = true;
                        break;
                case 's':
                        server = optarg;
                        break;
                case 'p':
                        port = atoi(optarg);
                        break;
                default: /* '?' */
                        fprintf(stderr,
                                "Usage: %s\n"
                                "\t-C\t\tTerminal Control Character Trace\n"
                                "\t-P\t\tProtocol Trace\n"
                                "\t-s\t<server>\n"
                                "\t-p\t<port>\n"
                                , argv[0]);
                        exit(2);
                }
        }

	if (socket_open(server, port))
		return 1;
	pthread_create(&user_input_handler, 0, user_input_function, 0);
loop:
	len = socket_read(buf, sizeof buf);
	if (len == 0)
		return 1;	// socket was closed by peer
	for (i=0; i<len; i++) {
		if (buf[i] == ACK) {
			if (ptrace)
				printf("<ACK>");
			// we may send now
			if (inp != NULL) {
				p = buf2;
				*p++ = STX;
				strcpy(p, (char*)inp);
				p += strlen((char*)inp);
				*p++ = ETX;
				if (ptrace)
					printf("[STX]%s[ETX]\n", inp);
				inp = NULL;
				printf("\033[A"); // undo the CRLF currently done by cooked reading of stdin
				if (socket_write(buf2, p-buf2) < p-buf2)
					return 1;
			} else {
				// oops, nothing to send
				p = buf2;
				*p++ = EOT;
				if (ptrace)
					printf("[EOT]\n");
				if (socket_write(buf2, p-buf2) < p-buf2)
					return 1;
			}
		} else if (buf[i] == EOT) {
			if (ptrace)
				printf("<EOT>");
			intext = false;
		} else if (buf[i] == ENQ) {
			if (ptrace)
				printf("<ENQ>");
			p = buf2;
			*p++ = ACK;
			if (ptrace)
				printf("[ACK]\n");
			if (socket_write(buf2, p-buf2) < p-buf2)
				return 1;
		} else if (buf[i] == STX) {
			if (ptrace)
				printf("<STX>");
			intext = true;
		} else if (buf[i] == ETX) {
			if (ptrace)
				printf("<ETX>");
			intext = false;
			p = buf2;
			*p++ = ACK;
			if (ptrace)
				printf("[ACK]\n");
			if (socket_write(buf2, p-buf2) < p-buf2)
				return 1;
		} else {
			// what remains...
			if (intext) {
				// text to display
				if (isprint(buf[i])) {
					printf("%c", buf[i]);
				} else {
					if (ctrace)
						printf("<%02x>", buf[i]);
					if (buf[i] == CR)
						printf("\033[K\n");
					else if (buf[i] == DC1)
						printf("\033[K");
					else if (buf[i] == BS)
						printf("\033[D");
					else if (buf[i] == LF)
						printf("\033[B\033[K");
					else if (buf[i] == DC4)
						printf("\033[H");
					else if (buf[i] == DC3)
						printf("\033[A");
					else if (buf[i] == FF)
						printf("\033[2J");
					else if (buf[i] != NUL)
						printf("<%02x>", buf[i]);
				}
			} else {
				// chars outside STX/ETX
				printf("{%02x}", buf[i]);
			}
		}
	}
	fflush(stdout);
	goto loop;
}

