/***********************************************************************
* b5500emulator
************************************************************************
* Copyright (c) 2018, Reinhard Meyer, DL5UY
* Licensed under the MIT License,
*       see LICENSE
************************************************************************
* b9353 Input and Display System
*
* Note 1:
*	According to Paul Kimpel, the MCP Release XIII does not really
*	support the B9353(Type=3) setting in the SYSTEM/DISK file.
*	Observable, the "SEQ" command in CANDE does not output 10 empty
*	lines to be filled by the user.
*
*	Using the B9352(Type=1) setting, those lines are output as
*	described in the "B5700 Time Sharing Reference Manual" Page 1-54.
*
*	Unfortunately, no description of the workings of the B9352 could
*	be found.
*
*	For the B9353 an elaborate manual exits.
*	We will therefore implemnent the B9353 display. First tries show
*	that the functionality seems suited for the B9352(type=1) setting.
*
* Note 2:
*	Data to/from this emulation is in ASCII with ASCII control codes.
*	Un-escaping and escaping to the 6 Bit/char BIC codes is not part
*	of this emulation. 62 of the 64 possible characters can be used:
*	0-9 A-Z [ ] < = > # " $ % & ( ) * + , - . / : ; ?
*	SPACE
*	MULTIPLY (shown as x)
*	LEFTARROW (shown as ~)
*	LESSEQUAL (shown as {)
*	GREATEREQUAL (shown as })
*
* Note 3:
*	On keyboard input
*	- keyboard a-z is converted to A-Z
*	- all other keyboard characters are ignored and sound the bell
*
* Note 4:
*	An ANSI or VT220 compatible terminal (emulation) is required in
*	full-duplex mode without local echo.
*	The B9353 emulation will convert the B9363 controls to ANSI.
*
* Note 5:
*	The B9353 is a vector display with non-spatial memory of max.
*	1018 characters. A special code in memory terminates a line
*	and the display process will continue in the next line below.
*	The B9353 will not scroll, any output after 25 lines will be
*	discarded.
*
************************************************************************
* 2018-03-12  R.Meyer
*   Initial Version
* 2018-03-26  R.Meyer
*   Write output into non-spatial memory and copy to ANSI terminal
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

/***********************************************************************
* defines for screen resolution and storage
***********************************************************************/
#define	COLS	80	// number of characters per row
#define	ROWS	25	// number of rows
#define	MEMSIZ	1018	// size of the non-spatial memory
#define	BUFLEN	200	// length of TCP input/output buffers

/***********************************************************************
* ASCII control codes
* the codes for LF, RS, US are also used in the non-spatial memory
***********************************************************************/
#define	NUL	0x00	// time fill - ignored
#define	STX	0x02	// start of text during transmission
#define	ETX	0x03	// end of text during transmission
#define	EOT	0x04	// end of transmission, turn over
#define	ENQ	0x05	// status request
#define	ACK	0x06	// positive acknowledge
#define	BEL	0x07	// ring the bell
#define	BS	0x08	// one position to the left
#define	TAB	0x09	// forward to tab stop
#define	LF	0x0a	// line feed
#define	FF	0x0c	// clear screen
#define	CR	0x0d	// return
#define	DC1	0x11	// line erase
#define	DC2	0x12
#define	DC3	0x13
#define	DC4	0x14	// cursor home
#define	NAK	0x15	// negative acknowledge
#define	RS	0x1e	// "shift out" - protected area start
#define	US	0x1f	// "shift in" - protected area end

/***********************************************************************
* ANSI control codes
***********************************************************************/
#define _SO_	"\033[7m"	// inverse display on
#define _SI_	"\033[27m"	// inverse display off
#define	_EREOL_	"\033[K"	// erase to end of line
#define	_EREOS_	"\033[J"	// erase to end of screen
#define	_BS_	"\010"		// backup one char
#define	_CR_	"\015"		// return to begin of SAME line
#define	_LF_	"\012"		// one line down
#define	_HOME_	"\033[H"	// cursor to top left corner
#define	_CLS_	"\033[H\033[2J"	// clear screen and curso to top left corner
#define	_UP_	"\033[A"	// one line up
#define	_GOTO_	"\033[%u;%uH"	// set cursor to row and column

/***********************************************************************
* Special UTF-8 characters
***********************************************************************/
#define	_STAR_	"\302\244"
#define	_LE_	"\302\253"
#define	_NOT_	"\302\254"
#define	_PARA_	"\302\266"
#define	_GE_	"\302\272"
#define	_MULT_	"\303\230"

/***********************************************************************
* the data
***********************************************************************/

// telnet
static int sock = -1;

// non-spatial memory
static char screen[MEMSIZ];	// non-spatial memory
static char *curp = screen;	// current cursor position
static char *endp = screen;	// current end position

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
* Get cursor pointer into coordinates
***********************************************************************/
static void getcursor(unsigned *row, unsigned *col) {
	char *p = screen;
	*row = 1;
	*col = 1;
	while (p < curp) {
		// CR means end of line
		if (*p == CR) {
			(*col) = 1;
			if (*row == ROWS)
				return;
			(*row)++;
		} else {
			(*col)++;
			if ((*col) > COLS) {
				(*col) = 1;
				if ((*row) >= ROWS)
					return;
				(*row)++;
			}
		}
		p++;
	}
}

/***********************************************************************
* Set cursor pointer from coordinates
***********************************************************************/
static void setcursor(unsigned drow, unsigned dcol) {
	unsigned row = 1;
	unsigned col = 1;
	curp = screen;
	while (curp < endp) {
		// CR means end of line
		if (*curp == CR) {
			col = 1;
			if (row == ROWS)
				return;
			row++;
		} else {
			col++;
			if (col > COLS) {
				col = 1;
				if (row == ROWS)
					return;
				row++;
			}
		}
		if (drow == row && dcol == col)
			return;
		curp++;
	}
}

/***********************************************************************
* Redisplay non-spatial memory on ANSI terminal
***********************************************************************/
static void redisplay(void) {
	unsigned row=1, col=1, cursorrow = 1, cursorcol = 1;
	char *p = screen;
	char ch = 0;
	// home cursor to start at top left
	printf(_HOME_);
	for (row = 1; row <= ROWS; row++) {
		// move cursor to begin of line
		printf(_GOTO_, row, 1);
		for (col = 1; col <= COLS; col++) {
			if (p == curp) {
				// remember cursor position
				cursorrow = row;
				cursorcol = col;
			}
			if (p < endp) {
				ch = *p++;
			} else {
				// end of non-spatial memory
				// just clear the rest of the screen
				printf(_NOT_ _EREOS_);
				goto done;
			}
			if (row == ROWS && col == COLS) {
				// cannot use last char of last line
				break;
			}
			if (ch == CR || ch == LF) {
				// end of line
				// just clear the rest of the line (or the whole line)
				printf(_PARA_ _EREOL_);
				break;
			} else if (ch == ETX) {
				printf(_STAR_);
			} else if (ch == RS) {
				printf(_SO_ _LE_);
			} else if (ch == US) {
				printf(_GE_ _SI_);
			} else {
				printf("%c", ch);
			}
		}
	}
done:
	// move cursor to its position
	printf(_GOTO_, cursorrow, cursorcol);
	fflush(stdout);
}

/***********************************************************************
* Main Program
***********************************************************************/
int main(int argc, char*argv[]) {
	char buf[BUFLEN];
	char buf2[BUFLEN];
	char *p;
	int len, i;
	int intext = false;
        int opt;
	const char *server = "127.0.0.1";
	unsigned port = 8023;

        while ((opt = getopt(argc, argv, "CPs:p:T")) != -1) {
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
                case 'T':
			for (opt=32; opt<0x7f; opt++) {
				if (opt % 32 == 0)
					printf("\n%03x ", opt);
				printf("%c", opt);
			}
			for (opt=0x80; opt<=0x7ff; opt++) {
				if (opt % 32 == 0)
					printf("\n%03x ", opt);
				printf("%c%c", 0xc0+(opt>>6), 0x80+(opt&0x3f));
			}
			exit(0);
                        break;
                default: /* '?' */
                        fprintf(stderr,
                                "Usage: %s\n"
                                "\t-C\t\tTerminal Control Character Trace\n"
                                "\t-P\t\tProtocol Trace\n"
                                "\t-s\t<server>\n"
                                "\t-p\t<port>\n"
                                "\t-T\t\tTest Charset\n"
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
				if (socket_write(buf2, p-buf2) < p-buf2)
					return 1;
				curp = screen;
				redisplay();
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
			// ETX == end of transmission
			// needs one character space
			if (ptrace)
				printf("<ETX>");
			intext = false;
			p = buf2;
			*p++ = ACK;
			if (ptrace)
				printf("[ACK]\n");
			if (socket_write(buf2, p-buf2) < p-buf2)
				return 1;
			if (curp < screen+MEMSIZ) {
				*curp++ = buf[i];
				if (endp < curp)
					endp = curp;
			}
			redisplay();
		} else {
			// what remains...
			if (intext) {
				// text to display or allowed control chars
				if (isprint(buf[i]) || buf[i] == CR || buf[i] == LF || buf[i] == RS || buf[i] == US) {
					if (curp < screen+MEMSIZ) {
						if (buf[i] == LF)
							*curp++ = CR;
						else
							*curp++ = buf[i];
						if (endp < curp)
							endp = curp;
					}
				} else {
					if (ctrace)
						printf("<%02x>", buf[i]);
					if (buf[i] == DC1) {
						// DC1 = clear from current to end of line
						// search end of line or end of screen
						for (p = curp; p < endp && *p != CR; p++)
							;
						opt = p - curp;
						if (opt > 0) {
							memcpy(curp, p, endp - p);
							endp -= opt;
						}
					} else if (buf[i] == BS) {
						// BS = one position left
						if (curp > screen)
							curp--;
					} else if (buf[i] == DC4) {
						// DC4 = home position, no clear
						curp = screen;
					} else if (buf[i] == DC3) {
						// DC3 = one position up
						unsigned row, col;
						getcursor(&row, &col);
						if (row > 1) {
							row--;
							setcursor(row, col);
						}
					} else if (buf[i] == FF) {
						// FF = clear screen and cursor home
						curp = endp = screen;
					} else if (buf[i] != NUL) {
						// NUL = time fill, no effect on screen
						printf("<%02x>", buf[i]);
					}
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

