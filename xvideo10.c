// VS4T1 Video/Audio 4-to-1 switching cable controller
// XVideo10 VS4T1-to-CM11A video switch firmware
// (C) 2013 Tech World Inc 
// The code released under Open Source Expat MIT License
// See license-mit-expat.txt for details

// Fuses:
// Brown-out detection disabled BODLEVEL=1111
// Int RC Osc 8Mhz 65ms  CKSEL=0100
// Serial program downloading Enabled SPIEN=0

// Clock: 8Mhz
// Based on ATTiny2313 2048 bytes flash, 128 bytes RAM, 128 bytes EEPROM

#include <ctype.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>

// this line makes EEP file used by avrdude to program eeprom
uint8_t EEMEM eepromdefaults[5]={"140PS"};

// eeprom storage variables
struct {
	unsigned char cam;
	unsigned char maxcam;
	unsigned char cyclemax;
	unsigned char housecode;
	unsigned char idlemode;
} ee;

#define SCAN_OFF 1		// scan_off is any positive value
#define SCAN_ON 0

unsigned char *eeptr=0x0000; 			
unsigned char cycnt = '0';
unsigned char cmtimeout = 0;
unsigned char scan;			// 1=not scanning 0=scanning
unsigned char disablemenu = 0;
unsigned char inmenu = 0;
unsigned char menucnt = 0;
unsigned char wanttime = 0;

void TransmitByte( unsigned char data )
{
	while ( !(UCSRA & (1<<UDRE)) );		// Wait for empty transmit buffer 
	
	unsigned char time;
	
	unsigned char ms = 20;
  	while (--ms != 0) {
    	// this number (100) is dependent on the clock frequency
    	for (time=0; time <= 100 ; time++);
  	}
	UDR = data; 			        	// Start transmission 
}

void TransmitString( PGM_P msg)
{
	unsigned char x = 0;
	unsigned char c;

	while( (c = pgm_read_byte(msg+x)) ){
		if(c=='\n'){
			TransmitByte('\r');
		}
		TransmitByte(c);
		x++;
	}

}

// switch to camera 1-4 or zero=all off
//void setcam(unsigned char cam, unsigned char scanmode){
void setcam(){
	if(ee.cam=='0'){
		PORTB = 0xF0;	// pullups on, video off
	} else {
		PORTB = 0xF0 + (1<<(ee.cam-'1'));   // pullups on + video setting  36 bytes
	}

	scan = SCAN_OFF;		// turn scan off on camera events
}

// takes a house code A-P and returns an X10 house code suitable for transmission to the CM11a
unsigned char x10housecode()
{
	PGM_P hex = PSTR("\x06\x0e\x02\x0A\x01\x09\x05\x0D\x07\x0F\x03\x0B\x00\x08\x04\x0C");

	return pgm_read_byte(hex+ee.housecode-'A');
}

// sends time response and X10 house code to monitor to CM11a
//void sendtime()
//{
//}

void saveandshowconfig()
{
	PGM_P help1 = 	PSTR("\nXVideo10 Public Domain\nC:Cameras:");
	PGM_P help2 = 	PSTR("\nS:ScanTime:");
	PGM_P help3 = 	PSTR("sec\nH:HouseCode:");
	PGM_P help4 = 	PSTR("\nI:IdleMode:");
 	PGM_P help5 =   PSTR("\n?:Menu\n");

	eeprom_write_block(&ee, eeptr, sizeof(ee));

	// show banner
	TransmitString(help1);
	TransmitByte(ee.maxcam);
	TransmitString(help2);

	// print the number of seconds currently configured
	TransmitByte( (ee.cyclemax + 49)>>1);

	if(ee.cyclemax & 0x01){ 
		TransmitByte('0');
	} else {
		TransmitByte('5');
	}

	TransmitString(help3);
	TransmitByte(ee.housecode);
	TransmitString(help4);
	TransmitByte(ee.idlemode);
	TransmitString(help5);
}

void idle()
{
	if(ee.idlemode=='S'){
		scan = SCAN_ON;	// scan on
	}
	
	if(ee.idlemode=='P'){
		ee.cam = '1';	// priority camera 1
		setcam();
	}
}

// handle timer events
// this routine handles scan mode timer switching and setting the time periodically on the CM11a
// timer signalling is always on on this device.
SIGNAL(SIG_OVERFLOW1)
{
	if(scan==SCAN_ON){		// scan is zero
		// this code performs the scan change for cameras
		// scanning, count up to cyclemax and then switch to next camera
		if(cycnt++ >= ee.cyclemax){
			disablemenu = 1;
			if(++ee.cam>ee.maxcam){
				ee.cam = '1';
			}
			setcam();			// turns off scanning
			scan = SCAN_ON;		// turns scan back on
			cycnt = '0';
		}
	} else {				// scan > zero
		// not scanning
		// runs idle mode setting after 1 minute of inactivity
		if(++scan>13){			// after one minute inactivity
			idle();				
			scan = 1;		// resets scan timeout count
		}
	}
	

	// sends time initialization to the CM11a every five minutes
	if(++cmtimeout>60){			// every 5 minutes
		wanttime = 1;
		cmtimeout = 0;
	}


	TCNT1 = 26471;				// 5 sec Timer runs at 7813hz at 8Mhz 65536-7813*5 = 26471
}

// Main - a simple test program
int main( void )
{
	unsigned char x;
	unsigned char house = 0xFF;		// the received house code
	unsigned char dev = 0xFF;		// the last received device number
	unsigned char numbytes;			// the number of bytes in the CM11 buffer during a receive
	unsigned char buffer[11];		// the CM11 bytes
	unsigned char bufidx;			// idx to the buffer
	unsigned char rcvbufmode = 0;	// flag to indicate that we are processing a receive string
	unsigned char inchar;			// input byte from serial port

	// load defaults
	eeprom_read_block(&ee, eeptr, sizeof(ee));
	
	DDRB = 0xFF;    // set portb PB0-7 to outputs
	PORTB = 0x01;	// All lines low (PB0-3 turns off video) default to camera 1
	DDRD = 0x00;	// serial I/O on port D

	// set baud rate and switch to scan mode on startup
	// Set baud rate 
	UBRRH = 0;
	UBRRL = 103;		// 4800 bps
	
	// Enable receiver and transmitter
	UCSRB = (1<<RXEN)|(1<<TXEN);
	
	// Set frame format: 8N1
	//UCSRC = 0x06;
	UCSRC = (3<<UCSZ0);

	TCCR1B = (1<<CS10) | (1<<CS12);		// prescale /1024, normal mode 8 sec

	scan = SCAN_OFF;
	idle();

	TIMSK |= _BV(TOIE1);		// enable timer1 overflow

	// activate timer
	sei();

  	// main loop
	while (1) {
				
		// Check for incoming data 
		if( (UCSRA & (1<<RXC)) ){ ; 	

			// data found, disable interrupts
			cli();

			inchar = UDR;		

			if(inmenu){

				if(inchar>='a'){
					inchar -= 0x20;
				}
        
				if(inchar=='C'){
					// set max cam to 2,3 or 4
					if(++ee.maxcam>'4') {
						ee.maxcam = '2';
					}
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
					if(++ee.cyclemax>'5') ee.cyclemax='0';
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
				if(disablemenu==0){
					if(inchar=='!'){
						if(++menucnt>=3){
							inmenu = 1;
							TIMSK = 0;	// disable timer in menu mode
							saveandshowconfig();
						}
					} else {
						disablemenu = 1;
						inmenu = 0;
						wanttime = 1;					
					}
				}

				if(rcvbufmode==0){		
		    		// not in the middle of a code, so check this byte
					if(inchar==0x5A){			// this is a POLL request from the CM11A  'Z'
			    		TransmitByte(0xC3);		// send ACK to CM11A
						rcvbufmode = 1;		// enter receive mode
						numbytes = 0;			// clear byte count
						bufidx = 0;
					} 

				    if(inchar==0xA5 || wanttime){			// this is a TIME request from the CM11A
						//sendtime();				// send the time and house code
						PGM_P timeseq = PSTR("\x9b\x00\x00\x00\x00\x00");
						TransmitString(timeseq);			// send time cmd
						TransmitByte(x10housecode()<<4);	// bits 4-7 set the house code 
						wanttime = 0;
					}

			    	if(inchar==0x55){			// This is a CM11A ready indicator, do nothing
						// send on/off status commands here
					}
				} else {
					// in the middle of receiving x10 code, continue
					if(numbytes==0){
						// the numbytes byte has not been received, so this byte is the byte count 
						if(inchar>9){	
							// >9 is out of bounds, so abort - this is a sanity check
							rcvbufmode = 0;
						} else {
							// save the number of bytes (always less than 10)
							numbytes = inchar;
						}
					} else {
						// the number of bytes has already been received, so this is part of the command string
						buffer[bufidx++] = inchar;
						// process until the buffer has the expected number of bytes
						if(bufidx>=numbytes){
							// done receiving, process the codes
							rcvbufmode = 0;	// stop receive processing
		
							// the remaining bytes are function/address codes
							for(x=1;x<numbytes;x++){
								// split the byte
								// unsigned char bytehi = (buffer[x] & 0xF0) >> 4;
								// unsigned char bytehi = (buffer[x]>>4) & 0x0F;
								unsigned char bytehi = (buffer[x]>>4);
								unsigned char bytelo = buffer[x] & 0x0F;

								// the function/address mask is in byte0
								// is the low mask bit set? 
								if(buffer[0] & 0x01){

									// bit is set, so this is a function
									// the events are activated here
									if(house==x10housecode()){
										
										if(bytelo == 0x02){		// 2	X10 ON Command

											// switch to selected camera
											// house code must be P and device must be 1-5
											if(dev==0x06){		// unit 1 ON
												ee.cam = '1';				
												setcam();	// goto video 1     
											}
											if(dev==0x0E){		// unit 2 ON
												ee.cam = '2';
												setcam();	// goto video 2
											}
											if(dev==0x02){		// unit 3 ON
												ee.cam = '3';
												setcam();	// goto video 3
											}
											if(dev==0x0A){		// unit 4 ON
												ee.cam = '4';
												setcam();	// goto video 4
											}

											if(dev==0x01){		// unit 5 ON
												scan = SCAN_ON;	// scan mode on
											}
											if(dev==0x09){		// unit 6 ON
												if(--ee.cam<'1') ee.cam='4';
												setcam();	// switch to prev camera
											}


										}

										// unit off commands
										if(bytelo==0x03){	// 3	OFF
											// for 1-4 OFF, compare unit code to current camera and turn off only if they match
											if( (dev==0x06 && ee.cam=='1') || 
												(dev==0x0E && ee.cam=='2') || 
												(dev==0x02 && ee.cam=='3') || 
												(dev==0x0A && ee.cam=='4') ){
												idle();
											}
											
											if(dev==0x01){		// unit 5 OFF
												ee.cam = '0';
												setcam();	// turns off video
											}
											
											if(dev==0x09){		// unit 6 OFF
												if(++ee.cam>'4') ee.cam='1';
												setcam();		// switch to next camera
											}
										}
									}
								} else {
									// bit is clear, so this is an address
									house = bytehi;
									dev = bytelo;
								}
								buffer[0] = buffer[0] >> 1;
								
							}
						}
					}
				}
			} // else
			sei();
		}
	}
}


