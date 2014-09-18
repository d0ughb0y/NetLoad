/* Name: main.c
 * Author: .
 * Copyright: Arduino
 * License: GPL http://www.gnu.org/licenses/gpl-2.0.html
 * Project: tftpboot
 * Function: Bootloader core
 * Version: 0.2 support for USB flashing
 */
//modifications by Jerry Sy
//aka doughboy @ forum.arduino.cc, d0ughb0y @ github.com
//Project: https://github.com/d0ughb0y/NetLoad

#include <avr/eeprom.h>
#include <avr/boot.h>
#include <avr/wdt.h>
#include <util/delay.h>

#include "util.h"
#include "spi.h"
#include "w5100.h"
#include "tftp.h"
#include "serial.h"
#include "neteeprom.h"
#include "watchdog.h"
#include "debug.h"
#include "debug_main.h"
#if defined(ANNOUNCE)
	#include "announce.h"
#endif

int main(void) __attribute__((naked)) __attribute__((section(".init9")));
//void (*appStart)(void) __attribute__((naked)) = 0x0000;
//void (*appStart)(void) = 0x0000;

int main(void)
{
	/* Disable the watchdog timer to prevent
	 * eternal reset loop of doom and despair */
	MCUSR = 0;
	wdt_disable();

	// Wait to ensure startup of W5100
	_delay_ms(200);

	/* This code makes the following assumptions:
	 * No interrupts will execute
	 * SP points to RAMEND
	 * r1 contains zero */
	//cli();
	asm volatile("clr __zero_reg__");
#if defined(__AVR_ATmega8__)
	SP = RAMEND;  // This is done by hardware reset
#endif

	// Prescaler=0, ClkIO Period = 62,5ns
	// TCCR1B values:
	// 0x01 -> ClkIO/1 -> 62,5ns period, 4ms max
	// 0x02 -> ClkIO/8 -> 500ns period, 32ms max
	// 0X03 -> ClkIO/64 -> 4us period, 256ms max
	// 0x04 -> ClkIO/256 -> 16us period, 1024ms max
	// 0x05 -> ClkIO/1024 -> 64us period, 4096ms max
	// Set up Timer 1 as timekeeper for LED flashing
	TCCR1B = _BV(CS12); // Same thing as TCCR1B = 0x04; 

	/* Initialize UART communication */
	serialInit();
	DBG_MAIN(tracePGMlnMain(mDebugMain_TITLE);)

	DBG_BTN(
		DBG_MAIN_EX(tracePGMlnMain(mDebugMain_BTN);)
		buttonInit();
	)

	/* Initalize SPI communication */
	DBG_MAIN_EX(tracePGMlnMain(mDebugMain_SPI);)
	spiInit();
	/* Initialize networking */
	DBG_MAIN_EX(tracePGMlnMain(mDebugMain_NET);)
	netInit();

	/* Initialize the UDP socket for tftp */
	DBG_MAIN_EX(tracePGMlnMain(mDebugMain_TFTP);)
	uint8_t hasW5100 = isW5100();
	if (hasW5100) {
		tftpInit();
		timeout=5;
	} else 
		timeout=1;

	/* This code is to be used with the java-client inherited from the
	 * Arduino project. We don't support it and it adds about
	 * 600 bytes to the binary. So off it goes */
#if defined(ANNOUNCE)
	DBG_MAIN_EX(tracePGMlnMain(mDebugMain_ANN);)
	announceInit();
#endif

	serialFlashing = FALSE;
	tftpFlashing = FALSE;

	for(;;) {
		// If there is no serial flashing under way, poll tftp
		if(!serialFlashing)
			// If tftp recieved a FINAL_ACK, break

			if(hasW5100 && tftpPoll() == 0)
				break;

		// If there is no tftp flashing, poll serial
		if(!tftpFlashing)
			// If flashing is done exit
			if(serialPoll() == 0)
				break;

		/* As explained above this goes out */
#if defined(ANNOUNCE)
		announcePoll();
#endif

		if(timedOut()) {
			if (!tftpFlashing && !serialFlashing) break;
			//TODO: determine the conditions for reseting server OR reseting socket
			if(tftpFlashing == TRUE) {
				// Delete first page of flash memory
				boot_page_erase(0);
				// Reinitialize TFTP
				tftpInit();
				// Reset the timeout counter
				resetTick();
				// Unset tftp flag
				tftpFlashing = FALSE;
			}
		}
		/* Blink the notification led */
		updateLed();
	}

	/* Exit to user application */
	DBG_MAIN(tracePGMlnMain(mDebugMain_EXIT);)
	asm volatile(
		"clr	r30		\n\t"
		"clr	r31		\n\t"
		"ijmp	\n\t"
	);
	//appStart();
	//return(0); /* never reached */
}

