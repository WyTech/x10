// VS4T1 Video/Audio 4-to-1 switching cable controller
// XVideo26 VS4T1-to-MR26A video switch firmware
// (C) 2013 Alan Capesius 
// The code released under Open Source Expat MIT License
// See license-mit-expat.txt for details

// this code implements a dual mode serial port
// programming (or menu) mode talks to the user via a serial terminal for device configuration
// run mode acts as slave to a X-10 MR26A controller

// This code is memory tight on the ATTiny2313, if you are building your own hardware, 
// select a chip with more memory and make your life easier.

// If you are reviewing this code for other X-10 development, be aware that since only 
// part of the X-10 command set is needed, the code is written to support only those 
// commands and unit codes needed and some shortcuts are used to save memory that may 
// not be compatible with the full X-10 command set.

// for more X-10 reference, see T_H_X10 project currently at
//   http://webpages.charter.net/mkeryan/t_h_x10/t_h_x10.htm
// and the MR26A protocol specification at
//   ftp.x10.com   cm17a_protocol.txt
// there is also significant material available on Google for CM17A interfacing

// Fuses:
// Brown-out detection disabled BODLEVEL=1111
// Int RC Osc 8Mhz 65ms  CKSEL=0100
// Serial program downloading Enabled SPIEN=0

// Clock: 8Mhz
// Based on ATTiny2313 2048 bytes flash, 128 bytes RAM, 128 bytes EEPROM

#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>

// this line instructs avrdude to make an EEP file containing this data
// and to program the chip with the data at burn time
// this code is not executed at runtime
// this data is mapped to the program specific ee data structure below
uint8_t EEMEM eeprom[5]={"140PS"};

// eeprom storage variables
struct {
	unsigned char cam;
	unsigned char maxcam;
	unsigned char multiplier;
	unsigned char housecode;
	unsigned char idlemode;
} ee;

#define SCAN_OFF 1		// scan_off is any positive value
#define SCAN_ON 0


unsigned char *eeptr=0x0000;  // dummy ptr for offset in eeprom
unsigned char timer5 = 0;  // run scan process at 1/5th timer speed
unsigned char cycnt = '0';   // controls camera switching
unsigned char scan;			// 1=not scanning 0=scanning
unsigned char inmenu = 0;    // default to operational mode
unsigned char menucnt = 0;  
unsigned char suppresscodes = 99;

// 66 bytes
void TransmitByte( unsigned char data )
{
	while ( !(UCSRA & (1<<UDRE)) );		// Wait for empty transmit buffer 
	
	unsigned char time;
	
	unsigned char ms = 20;
	
  while (--ms != 0) {
	  // this number (100) is dependent on the clock frequency
	  for (time=0; time <= 100 ; time++);
	}
  
  UDR = data; 			    	// Start transmission 
}


// sets the hardware to the current camera and RTS setting
// 78 bytes
void sethdw(){
  if(ee.cam>'4') ee.cam = '1';
    
  if(ee.cam=='0'){
    PORTB = 0x00;
  } else {
    PORTB = (1<<(ee.cam-'1'));   // shift 1 cam-'1' positions left
  }

  __asm__ volatile("nop");
  __asm__ volatile("nop");

  DDRB = 0x0F;      
  
}

// 8 bytes
void setcam(){
  sethdw();
  scan = SCAN_OFF;
}

// 268 bytes
void showbanner(){
	unsigned char x = 0;
	unsigned char c;

  PGM_P msg = 	PSTR( "\nOpen Source Public Domain Example\n?:Menu\n" \
                      "\x05" \  
                      "C: Cameras:\x01\nS: Scan Time:\x02sec\nH: House Code:\x03\nI: Idle Mode:\x04\n0: Video Off\n1-4: Select Camera\n");

	while( (c = pgm_read_byte(msg+x)) ){
    if(c=='\n') TransmitByte('\r');
    if(c<= '\x05'){

      if(c=='\x01') TransmitByte(ee.maxcam);

      if(c=='\x02'){
        TransmitByte( (ee.multiplier + 49)>>1);
      	if(ee.multiplier & 0x01){ 
          TransmitByte('0');
        } else {
          TransmitByte('5');
        }
      }

      if(c=='\x03') TransmitByte(ee.housecode);
      if(c=='\x04') TransmitByte(ee.idlemode);
      if(c=='\x05' && inmenu==0) return;

    } else {
      TransmitByte(c);
    }

  	x++;
  }

}




// 150 bytes  
void saveandshowconfig()
{
  unsigned char x;  //148
	//eeprom_write_block(&ee, eeptr, sizeof(ee));  //220
  
  for(x=0;x<sizeof(ee);x++){
    eeprom_write_byte(eeptr+x,((char*)&ee)[x]);
  }
	showbanner();  // 2bytes
}

// 28 bytes
void idle()
{
	if(ee.idlemode=='S'){
		scan = SCAN_ON;	// scan on
	}
	
	if(ee.idlemode=='P'){
		ee.cam = '1';	// priority camera 1
    setcam();   // scan set off
	}
}

// handle timer events
// timer signalling is always on on this device at 1 second interval
// every five cycles, scan mode processing is performed.
// X10 codes are tracked and dropped if duplicates. The MR26A sends each code 5 times
// Each code is stored in suppresscodes and cleared once per second by the timer
// 218 bytes
SIGNAL(SIG_OVERFLOW1)
{
  
  timer5++;

  if(timer5>=5){
  // change cameras every 5 seconds
  if(scan==SCAN_ON){		// scan is zero
  	// this code performs the scan change for cameras
  	// ee.multiplier delays in multiples of 5 seconds as set by user
  	if(cycnt++ >= ee.multiplier){
  		if(++ee.cam>ee.maxcam){
  			ee.cam = '1';
  		}
  		sethdw();			// turns off scanning
  		cycnt = '0';
  	}
  } else {				// scan > zero
  	// not scanning
  	// runs idle mode setting after 1 minute of inactivity
  	if(++scan>13){			// after one minute inactivity
  		idle();				
  	}
  }
  
  timer5 = 0;
  }

  // 5 sec Timer runs at 7813hz at 8Mhz and counts up from this value 
  // and fires at overflow  
  // 65536-7813*5 = 26471  5 seconds
  // 65536-7813 = 57723  1 second
  TCNT1 = 57723;

  if(suppresscodes!=99){
    suppresscodes = 99;
  }
}


int main( void )
{
	unsigned char numbytes = 0;			// the number of bytes in the CM11 buffer during a receive
	unsigned char buffer[6];		// the CM11 bytes
	unsigned char inchar;			// input byte from serial port
  
	// load defaults from eeprom into ee ram structure
  unsigned char x;  
  
  for(x=0;x<sizeof(ee);x++){
    ((char*)&ee)[x] = eeprom_read_byte(eeptr+x);
  }
	
	DDRB = 0x9F;  // set portb PB5-6 inputs, others to outputs
	DDRD = 0x00;	  // serial I/O on port D
  sethdw();     // set PORTB

	// config serial port
	UBRRH = 0;  // Set baud rate 
	UBRRL = 51;   // 9600bps;
	UCSRB = (1<<RXEN)|(1<<TXEN);  // Enable receiver and transmitter
	UCSRC = (3<<UCSZ0);       // Set frame format: 8N1

	// prescaler takes 8mhz down to 7812.5Hz
  TCCR1B = (1<<CS10) | (1<<CS12);	

	scan = SCAN_OFF;
	idle();  // set scan mode to eeprom setting. default is on, user may change it

	// activate timer
  TCNT1 = 26471;				// 5 sec
	TIMSK |= _BV(TOIE1);		// enable timer1 overflow

	sei();          // timer interrupt on

  showbanner();
	
	while (1) {
				
		// Check for incoming data on serial port
		if( (UCSRA & (1<<RXC)) ){ ; 	

			// data found, disable interrupts
			cli();

			inchar = UDR;		

			if(inmenu){
        // this code handles user input on the serial port
				if(inchar>='a'){   // upper case the input
					inchar -= 0x20;
				}
    
  			if(inchar>='0' && inchar <='4'){
	  			// switch camera input
          ee.cam = inchar;
		  		setcam();
			  }

				if(inchar=='C'){
          ee.maxcam++;
          if(ee.maxcam>'4') ee.maxcam='2';
					saveandshowconfig();
        }

				if(inchar=='H'){
					// set house code to A-P
					if(++ee.housecode>'P') {
						ee.housecode = 'A';
					}
					saveandshowconfig();
				}

				if(inchar=='S'){
					// set scan interval to 5-30 seconds
					if(++ee.multiplier>'5') ee.multiplier='0';
					saveandshowconfig();
				}

				if(inchar=='I'){
					if(ee.idlemode=='S'){
						ee.idlemode = 'P';
					} else {
						if(ee.idlemode=='P'){
							ee.idlemode = 'N';
						} else {
							ee.idlemode = 'S';
						}
					}
					saveandshowconfig();
				}

				if(inchar=='?'){
					saveandshowconfig();
				}

			} else {
        if(numbytes==0 && inchar=='?'){
          inmenu = 1;
          showbanner();
        } else {
          // 524 bytes
            
          // MR26A data packets
          // D5AA6000AD  A1 on
          // D5AA6010AD
          // D5AA6008AD
          // D5AA6018AD
          // D5AA6040AD
          // D5AA6050AD  A6 on
          // D5AA6020AD A1 off
          // D5AA6030AD
          // D5AA6028AD
          // D5AA6038AD
          // D5AA6060AD
          // D5AA6070AD A6 off
          // D5AA7000AD B1 on
          // D5AA7010AD
          // D5AA7008AD
          // D5AA7018AD
          // D5AA7040AD
          // D5AA7050AD B6 on

        	if( (numbytes==0 && inchar==0xD5) ||
              (numbytes==1 && inchar==0xAA) ||
              (numbytes==4 && inchar==0xAD) ||
              numbytes==2 || 
              numbytes==3
            ){
        		  buffer[numbytes++] = inchar;
          } else {
            // unexpected value, discard data and wait for message
            numbytes = 0;
          }

          if(numbytes==5){
      		  // a full message has been received, process it
      		  unsigned char houseraw = buffer[2] >> 4;

           	// house code list A-P
            // optimized
            PGM_P hexhouse = PSTR("\x06\x07\x04\x05\x08\x09\x0A\x0B\x0E\x0F\x0C\x0D\x00\x01\x02\x03");
            
            unsigned char x;
            unsigned char hc = 'A';
      
            for(x=0;x<16;x++){
              if(pgm_read_byte(hexhouse+x)==houseraw){
                hc = x + 'A';
              }
            }



            if(hc==ee.housecode){
              // this is the housecode the switch is set to
              // unit codes 9-16 will set buffer2 bits, but the units we need wont
              // so the unit code is fully contained in buffer[3]
              // 16 bit codes:
              // unsigned int unithi = ((unsigned int)(buffer[2]&0x0F))<<8;
              // unsigned int unitraw = unithi + buffer[3];
              // 8 bit codes:
              unsigned char unitraw = buffer[3];
              unsigned char savecam = ee.cam;

              // skip duplicate codes rapid fired from MR26A
              // suppresscodes gets reset after 1 second by timer1
              if(suppresscodes!=unitraw){            
        				// switch to selected camera
        				// device must be 1-5
                
                // 14 bytes per line
                if(unitraw==0x00)  ee.cam = '1';  // 1 on				
                if(unitraw==0x10)  ee.cam = '2';  // 2 on				
                if(unitraw==0x08)  ee.cam = '3';  // 3 on				
                if(unitraw==0x18)  ee.cam = '4';  // 4 on				
                if(unitraw==0x60)  ee.cam = '0';  // 5 off, video off
                if(unitraw==0x50) {
    							if(--ee.cam<'1') ee.cam='4';  // 6 on, switch to prev camera
                }

                if(unitraw==0x70) {
          				if(++ee.cam>'4') ee.cam='1';  // 6 off, switch to next camera
                }
 
                if(savecam!=ee.cam){
                  setcam();       // do the camera switch
                  TCNT1 = 57723;
                }

                // 66 bytes
                if(unitraw==0x40){
                  scan = SCAN_ON;	// 5 on scan mode on
                  ee.cam = '1';
                  sethdw();
                }

                // optimized
                if( (unitraw==0x20 && ee.cam=='1') ||
                  (unitraw==0x30 && ee.cam=='2') ||
                  (unitraw==0x28 && ee.cam=='3') ||
                  (unitraw==0x38 && ee.cam=='4') ){
                  idle();
                }
                

                suppresscodes = unitraw;
 
              }
           }

            numbytes = 0;    // flush buffer if processed or not our house code
    			}
        }
    	} // else

			sei();
		}
	}
}


