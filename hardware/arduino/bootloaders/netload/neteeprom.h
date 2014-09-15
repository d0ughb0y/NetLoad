//modifications by Jerry Sy
//aka doughboy @ forum.arduino.cc, d0ughb0y @ github.com
//Project: https://github.com/d0ughb0y/NetLoad

#include <avr/eeprom.h>
#define ARIADNE_MAJVER 0
#define ARIADNE_MINVER 4

#define CONFSIZE sizeof(conf_t)
#define BASE_ADDRESS E2END-CONFSIZE
#define DEFAULTCONF {{192,168,0,1},{255,255,255,0},{0x12,0x34,0x56,0x78,0x9a,0xbc},{192,168,0,120},46969,0}

typedef struct {
	uint8_t gateway[4];
	uint8_t subnet[4];
	uint8_t mac[6];
	uint8_t ipaddr[4];
	uint16_t tftpdataport;
	uint16_t checksum;
} conf_t;
