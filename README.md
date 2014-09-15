# NetLoad Bootloader for Arduino
######][based on ariadne bootloader](http://codebendercc.github.com/Ariadne-Bootloader/)

Please read the README of the Ariadne project. Most of the info still applies here. I will only describe the changes.

## Bootloader works with Serial and Ethernet upload
You can upload using usb cable via Arduino IDE without the Ethernet shield attached. Bootloader waits 1 second for usb
program upload. If Ethernet shield is present, bootloader waits 5 seconds for program upload via usb or ethernet.

## TFTP upload optimized
There is no time delay added to tftp code. Upload is very fast. You can use curl to upload as follows:

    curl -T sketch.bin tftp://192.168.0.120


## Bootloader data stored at the end of EEPROM space
Most user programs stores data starting at EEPROM address 0. This bootloader stores the Network settings at the end of
EEPROM space. It takes up 22 bytes. Use sketch netloadinit.ino to read and/or update the network settings used by the
bootloader.

## Default Network Settings
The default built-in network settings of the bootloader are listed below.

`
* IP Address:  192.168.0.120
* Subnet Mask: 255.255.255.0
* Gateway:     192.168.0.1
* MAC Address: 0x12.0x34.0x56.0x78.0xAB.0xCD

* TFTP Listen Port: 69
* TFTP Data Port: 46969
`
##Upload to Arduino Mega 2560 works using USBASP programmer
If you use Arduino IDE with USBASP programmer to burn the bootloader to Mega, you will get a verification error at the
end. As far as I can tell, the bootloader uploads properly and all bootloader functions work correctly. I have modified
the Makefile so atmega2560_isp target skips verification.

##Remote reset using watchdog timer
You can reset the Arduino remotely if the current running sketch has code to receive command to reset. Note that
password data is part of application hence it is not stored in EEPROM as part of bootloader data. You can store that in
your sketch.
