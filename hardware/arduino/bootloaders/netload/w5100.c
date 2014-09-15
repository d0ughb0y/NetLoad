/* Name: net_w5100.c
 * Author: .
 * Copyright: Arduino
 * License: GPL http://www.gnu.org/licenses/gpl-2.0.html
 * Project: eboot
 * Function: Network initialization
 * Version: 0.1 tftp / flashing functional
 */
//modifications by Jerry Sy
//aka doughboy @ forum.arduino.cc, d0ughb0y @ github.com
//Project: https://github.com/d0ughb0y/NetLoad

#include <avr/io.h>
#include <avr/eeprom.h>
#include "util.h"
#include "tftp.h"
#include "spi.h"
#include "w5100.h"
#include "neteeprom.h"
#include "serial.h"
#include "debug.h"
#include "debug_net.h"

void netInit(void)
{
	conf_t conf;
	eeprom_read_block((void*)&conf,(void*)BASE_ADDRESS,CONFSIZE);
	if (conf.checksum==0 || chksum((uint16_t*)&conf,CONFSIZE/2)!=0) {
		conf_t tconf = DEFAULTCONF;
		tconf.checksum=chksum((uint16_t*)&tconf,CONFSIZE/2-1);
		eeprom_write_block((const void*)&tconf,(void*)BASE_ADDRESS,CONFSIZE);
		eeprom_read_block((void*)&conf,(void*)BASE_ADDRESS,CONFSIZE);
	}
#if defined(RANDOM_TFTP_DATA_PORT)
	while ((tftpTransferPort = TCNT1)<46969);
#else
	tftpTransferPort = conf.tftpdataport;
#endif			
	
	uint8_t i;
  DBG_NET(tracePGMlnNet(mDebugNet_EEPROM);)
	DBG_NET(
		tracePGMlnNet(mDebugNet_ADDR);
		for(i = 0; i < 4; i++) {
			tracenet(conf.ipaddr[i]);
			if(i != 3) putch(0x2E);
		}
		tracePGMlnNet(mDebugNet_SUBN);
		for(i = 0; i < 4; i++) {
			tracenet(conf.subnet[i]);
			if(i != 4) putch(0x2E);
		}
		tracePGMlnNet(mDebugNet_GW);
		for(i = 0; i < 4; i++) {
			tracenet(conf.gateway[i]);
			if(i != 3) putch(0x2E);
		}
		tracePGMlnNet(mDebugNet_MAC);
		for(i = 0; i < 6; i++) {
			tracenet(conf.mac[i]);
			if(i != 5) putch(0x2E);
		}
	)
	/** Configure Wiznet chip. Network settings */
  spiWriteReg(0, 0x80); //reset
  uint8_t* data = (uint8_t*)&conf;
	for(i = 0; i < CONFSIZE-4; i++)
		spiWriteReg(i+1, data[i]);
	DBG_NET(tracePGMlnNet(mDebugNet_DONE);)
}

uint8_t isW5100(void){
	spiWriteReg(0,0x10);
	if ( spiReadReg(0)!=0x10) 
		return 0;
	spiWriteReg(0,0x12);
	if ( spiReadReg(0)!=0x12)
		return 0;
	spiWriteReg(0,0x00);
	if ( spiReadReg(0)!=0x00)
		return 0;		
	else
		return 1;
}

