# X10 Stuff

These are code examples for AVR processors to interface directly with X10 serial computer modules.
This code is designed to work with the VS4T1 video switch which is just a ATTiny2313 and MAX232 circuit.

This code has two modes: 
- programming mode to configure the device using a PC serial port to talk to the devices MAX232. The device provides a config menu.
- run mode which talks directly to a X-10 CM11A or MR26A via the MAX232 and receives commands to control video switching.

See the VS4T1 repo for details on the hardware.
