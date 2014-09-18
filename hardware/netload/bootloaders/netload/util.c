/* Name: util.c
 * Author: Stelios Tsampas
 * Copyright: Arduino and optiboot
 * License: GPL http://www.gnu.org/licenses/gpl-2.0.html
 * Project: tftpboot
 * Function: Utility routines for various stuff
 * Version: 0.2 support for USB flashing
 */
//modifications by Jerry Sy
//aka doughboy @ forum.arduino.cc, d0ughb0y @ github.com
//Project: https://github.com/d0ughb0y/NetLoad

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>

#include "util.h"
#include "spi.h"
#include "debug.h"
#include "debug_util.h"

static uint16_t last_timer_1=0;
static uint16_t tick = 0;
uint8_t timeout;

void updateLed(void)
{
	uint16_t next_timer_1 = TCNT1;

	if(next_timer_1 & 0x800) LED_PORT ^= _BV(LED); // Led pin high
	else LED_PORT &= ~_BV(LED); // Led pin low

	if(next_timer_1 < last_timer_1) {
		tick++;
		DBG_UTIL(
		    tracePGMlnUtil(mDebugUtil_TICK);
		    tracenum(tick);
		    tracePGMlnUtil(mDebugUtil_NEXT);
		    tracenum(next_timer_1);
		    tracePGMlnUtil(mDebugUtil_LAST);
		    tracenum(last_timer_1);
		)
	}

	last_timer_1 = next_timer_1;
}

void resetTick(void)
{
	TCNT1 = 0;
	tick = 0;
}

uint8_t timedOut(void)
{
	// Never timeout if there is no code in Flash
#if (FLASHEND > 0x10000)
	if(pgm_read_word_far(0x0000) == 0xFFFF) return(0);
#else
	if(pgm_read_word_near(0x0000) == 0xFFFF) return(0);
#endif

	if(tick >= timeout) return(1);
	else return(0);
}

uint16_t chksum(uint16_t data[],uint8_t size) {
  uint16_t sum = 0;
   for (uint16_t i=0;i<size;i++) {
     sum += data[i];
   } 
   return ~sum+1;
}
