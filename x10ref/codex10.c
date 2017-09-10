/*  This program is a part of the 'T_H_X10' project, found at
       http://members.home.net/mkeryan/t_h_x10/

    It is Copyrighted by under the GPL:  (c) 1999 Michael J. Keryan
    See the file "COPYING" included with this program.
    You are granted permission to use and modify the code with the
    stipulation that copyright notices remain and the author is to be
    notified of any improvements, bug fixes, etc.

    codex10.c  inputs data from stdin
               as a series of: achar, Oxhex, where
	           achar is 'A' for address or other data,
		            'F' for function
		   hex is data from 00 thru FF
		   examples:  "F 0x33" or "A 0x99"
	       outputs data to stdout as Address, Function, or Data
	           examples:  "ADDR F04" or "FUNC F On" or "DATA 0x03"
*/

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define BUFSIZE 256

int main(int argc, char* argv[ ])
{
  char typebyte, house;
  char aline[BUFSIZE];
  unsigned int abyte, device, datacount = 0;
  const int devcode[] = {13, 5, 3, 11, 15, 7, 1, 9,
			 14, 6, 4, 12, 16, 8, 2, 10};
  const char hcode[] = {'M', 'E', 'C', 'K', 'O', 'G', 'A', 'I',
			'N', 'F', 'D', 'L', 'P', 'H', 'B', 'J'};
  const char *funcode[] = {"All_Units_Off", "All_Lights_On", "On", "Off",
			   "Dim", "Bright", "All_Lights_Off", "Extended_Code",
			   "Hail_Request", "Hail_Acknowledge",
			   "Pre-set_Dim(1)", "Pre-set_Dim(2)",
			   "Extended_Data_Transfer", "Status_On",
			   "Status_Off", "Status_Request"};

  for(;;) {
    if (fgets(aline, sizeof(aline), stdin) == NULL) {
      if (feof(stdin)) {
	fprintf(stderr, "Error; unexpected EOF\n");
	break;
      } else {
	fprintf(stderr, "Error: %s\n", strerror(ferror(stdin)));
	break;
      }
    } else {

      if (sscanf(aline, "%c %X", &typebyte, &abyte) != 2) {
	fprintf(stderr, "Error; unexpected short line\n");
	continue;
      }
    
      if (datacount > 0) {
	typebyte = 'D';
	datacount --;
      }

      if (typebyte == 'A') {
	house = hcode[(abyte >> 4) & 0x0F];
	device = devcode[abyte & 0x0F];
	printf("ADDR %c%02d\n", house, device);
      }
      
      if (typebyte == 'F') {
	house = hcode[(abyte >> 4) & 0x0F];
	printf("FUNC %c %s\n", house, funcode[abyte & 0x0F]);
	/* handle dim and bright data */
	if ((abyte & 0x0F) == 4 || (abyte & 0x0F) == 5)
	  datacount = 1;
	/* handle extended code data */
	else if ((abyte & 0x0F) == 7 )
	  datacount = 2;
      }
      
      if (typebyte == 'D') {
	printf("DATA 0x%02X\n", abyte);
      }
    
      fflush(stdout);
    }

  }
  return 0;
}

