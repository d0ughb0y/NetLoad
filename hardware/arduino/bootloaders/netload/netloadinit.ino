//Sketch by Jerry Sy
//aka doughboy @ forum.arduino.cc, d0ughb0y @ github.com
//Project: https://github.com/d0ughb0y/NetLoad

#include <avr/eeprom.h>

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

void setup(){
  conf_t conf;
  Serial.begin(115200);
  eeprom_read_block((void*)&conf,(void*)BASE_ADDRESS,CONFSIZE);
  if (conf.checksum==0 || chksum((uint16_t*)&conf,CONFSIZE/2)!=0) {
    Serial.println("initializing eeprom...");
    Serial.println("click send to continue...");
    while (!Serial.available());
    conf_t tconf = DEFAULTCONF;
    tconf.checksum=chksum((uint16_t*)&tconf,CONFSIZE/2-1);
    eeprom_write_block((const void*)&tconf,(void*)BASE_ADDRESS,CONFSIZE);
    eeprom_read_block((void*)&conf,(void*)BASE_ADDRESS,CONFSIZE);
  }
  uint8_t i;
  Serial.print("Gateway = ");
  for (i=0;i<4;i++) {
    Serial.print(conf.gateway[i]);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Subnet = ");
  for (i=0;i<4;i++) {
    Serial.print(conf.subnet[i]);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("MAC = ");
  for (i=0;i<6;i++) {
    Serial.print(conf.mac[i],HEX);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("ip = ");
  for (i=0;i<4;i++) {
    Serial.print(conf.ipaddr[i]);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("tftp data port = ");
  Serial.println(conf.tftpdataport);
}

void loop(){
  
}

uint16_t chksum(uint16_t data[],uint8_t size) {
  uint16_t sum = 0;
   for (uint16_t i=0;i<size;i++) {
     sum += data[i];
   } 
   return ~sum+1;
}

