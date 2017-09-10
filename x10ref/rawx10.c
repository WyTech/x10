/*  This program is a part of the 'T_H_X10' project, found at
       http://members.home.net/mkeryan/t_h_x10/

    It is Copyrighted by under the GPL:  (c) 1999 Michael J. Keryan
    See the file "COPYING" included with this program.
    You are granted permission to use and modify the code with the
    stipulation that copyright notices remain and the author is to be
    notified of any improvements, bug fixes, etc.

    rawx10.c  inputs data from serial port connected to CM11A x10 interface
              default port is /dev/ttyS0, overide is option:  -p myport
	      output is a series of: achar, Oxhex, where
	         achar is 'A' for address or other data,
		          'F' for function
                 hex is data from 00 thru FF
*/

#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#define DEFAULTPORT "/dev/ttyS0"
#define BUFSIZE 256

void readx10(int, int);

int main(int argc, char* argv[ ])
{
  char optstring[] = "dp:";
  char *port;
  int c, fd;
  int debugmode = 0;
  struct termios tios;

  opterr = 0;
  port = DEFAULTPORT;

  while ((c = getopt(argc, argv, optstring)) != -1)
    switch (c) {
    case 'd':
      debugmode = 1;
      fprintf(stderr, "debugmode is on \n");
      break;
    case 'p':
      port = optarg;
      if (debugmode) fprintf(stderr, "port defined as %s\n", port);
      break;
    case '?':
      printf("Unknown argument: %c\n", optopt);
      exit(-1);
    }

  fd = open(port, O_RDWR);
  if(fd < 0) {
    fprintf(stderr, "Error opening port %s\n", port);
    return 1;
  }

  /* set up the serial line in raw mode, 4800 baud */
  tcgetattr(fd, &tios);
  cfmakeraw(&tios);
  cfsetospeed(&tios, B4800);
  cfsetispeed(&tios, B4800);
  tcsetattr(fd, TCSANOW, &tios);

  readx10(fd, debugmode);
  return 0;
}

/* these codes come from x10 interface */
#define ANULL 0x00
#define POLL 0x5A
#define TIME_REQ 0xA5
#define ISREADY 0x55

void readx10(int fd, int debugmode)
{
  unsigned char buf[BUFSIZE], buf2[BUFSIZE];

  /* these codes are sent back to x10 interface */
  const char anull = 0x00;
  const char pollback = 0xC3;
  const char time_resp[] = {0x9B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

  int numbytes, numread, numread2, i, bitmask;
  char typebyte;

  for(;;) {
    numread = read(fd, buf, 1);
    switch (buf[0]) {

    case POLL:
      /* send pollback to POLL */
      write(fd, &pollback, 1);
      if (debugmode) fprintf(stderr, "rx 5A;  tx C3\n");
      break;

    case ANULL:
      /* send anull to ANULL */
      write(fd, &anull, 1);
      if (debugmode) fprintf(stderr, "rx 00;  tx 00\n");
      break;

    case TIME_REQ:
      /* send time_resp to TIME_REQ */
      write(fd, &time_resp, sizeof(time_resp));
      if (debugmode) fprintf(stderr, "rx A5;  tx 9B 00 00 00 00 00 00\n");
      break;

    case ISREADY:
      /* send pollback to POLL */
      if (debugmode) fprintf(stderr, "rx 55;  tx nothing\n");
      break;

    default:
      /* first byte is always number of bytes to follow */
      /* second byte is bitmask, one bit for each following byte */
      /* bit = 1 means function, bit = 0 means address or data */
      numbytes = buf[0];

      if (numbytes > 9) {
	/* unsynchronized, numbytes can't be > 9, discard buffer */
	numread = read(fd, buf, 10);
	fprintf(stderr, "Unsynchronized error, dumping buffer\n");
      }
      else {
	numread = read(fd, buf, numbytes);
	/* may not be fast enough to grab whole buffer in one shot */
	if (numread != numbytes) {
	  numread2 = read(fd, buf2, (numbytes-numread));
	  for (i = 0; i < numread2; ++i) {
	    buf[i+numread] = buf2[i];
	  }
	  numread = numread + numread2;
	}

	if (numread != numbytes) fprintf(stderr, "Error: %02X read but %02X expected\n", numread, numbytes);
	if (debugmode) {
	  fprintf(stderr, "rx %02X numbytes\n", numbytes);
	  fprintf(stderr, "rx %02X bitmask\n", buf[0]); }
      
	for (i = 1, bitmask = 1; i < numread; ++i, bitmask <<= 1) {
	  if (buf[0] & bitmask) typebyte = 'F';
	  else typebyte = 'A';

	  printf("%c 0x%02X\n", typebyte, (int) buf[i]);

	  if (debugmode) fprintf(stderr, "rx %02X, type: %c\n", buf[i], typebyte);
	}
      }
      fflush(stdout);
      break;
    }
  }
}
  
