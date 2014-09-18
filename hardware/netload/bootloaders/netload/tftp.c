/* Name: tftp.c
 * Author: .
 * Copyright: Arduino
 * License: GPL http://www.gnu.org/licenses/gpl-2.0.html
 * Project: ethboot
 * Function: tftp implementation and flasher
 * Version: 0.2 tftp / flashing functional
 */
//modifications by Jerry Sy
//aka doughboy @ forum.arduino.cc, d0ughb0y @ github.com
//Project: https://github.com/d0ughb0y/NetLoad

#include <avr/pgmspace.h>
#include <avr/boot.h>

#include "util.h"
#include "spi.h"
#include "w5100.h"
#include "neteeprom.h"
#include "tftp.h"
#include "validate.h"
#include "serial.h"
#include "debug.h"
#include "debug_tftp.h"

/** Opcode?: tftp operation is unsupported. The bootloader only supports 'put' */
#define TFTP_OPCODE_ERROR_LEN 12
const unsigned char tftp_opcode_error_packet[] PROGMEM = "\0\5" "\0\4" "Opcode?";

/** Full: Binary image file is larger than the available space. */
#define TFTP_FULL_ERROR_LEN 9
const unsigned char tftp_full_error_packet[] PROGMEM = "\0\5" "\0\3" "Full";

/** General catch-all error for unknown errors */
#define TFTP_UNKNOWN_ERROR_LEN 10
const unsigned char tftp_unknown_error_packet[] PROGMEM = "\0\5" "\0\0" "Error";

/** Invalid image file: Doesn't look like a binary image file */
#define TFTP_INVALID_IMAGE_LEN 23
const unsigned char tftp_invalid_image_packet[] PROGMEM = "\0\5" "\0\0" "Invalid image file";

uint16_t lastPacket = 0, highPacket = 0;

static void sockInit(uint16_t port)
{
	DBG_TFTP(
		tracePGMlnTftp(mDebugTftp_SOCK);
		tracenum(port);
	)

	spiWriteReg(REG_S3_CR, CR_CLOSE);

	do {
		// Write TFTP Port
		spiWriteWord(REG_S3_PORT0, port);
		// Write mode
		spiWriteReg(REG_S3_MR, MR_UDP);
		// Open Socket
		spiWriteReg(REG_S3_CR, CR_OPEN);

		// Read Status
		if(spiReadReg(REG_S3_SR) != SOCK_UDP)
			// Close Socket if it wasn't initialized correctly
			spiWriteReg(REG_S3_CR, CR_CLOSE);

		// If socket correctly opened continue
	} while(spiReadReg(REG_S3_SR) != SOCK_UDP);
}


#if (DEBUG_TFTP > 0)
static uint8_t processPacket(uint16_t packetSize)
#else
static uint8_t processPacket(void)
#endif
{

	uint8_t buffer[TFTP_PACKET_MAX_SIZE];
	uint16_t readPointer;
	address_t writeAddr;
	// Transfer entire packet to RAM
	uint8_t* bufPtr = buffer;
	uint16_t count;

	DBG_TFTP(
		tracePGMlnTftp(mDebugTftp_START);
		tracenum(packetSize);

		if(packetSize >= 0x800) tracePGMlnTftp(mDebugTftp_OVFL);

		DBG_BTN(button();)
	)

	// Read data from chip to buffer
	readPointer = spiReadWord(REG_S3_RX_RD0);

	DBG_TFTP_EX(
		tracePGMlnTftp(mDebugTftp_RPTR);
		tracenum(readPointer);
	)

	if(readPointer == 0) readPointer += S3_RX_START;
	for(count = TFTP_PACKET_MAX_SIZE; count--;) {

		DBG_TFTP_EX(
			if((count == TFTP_PACKET_MAX_SIZE - 1) || (count == 0)) {
				tracePGMlnTftp(mDebugTftp_RPOS);
				tracenum(readPointer);
			}
		)

		*bufPtr++ = spiReadReg(readPointer++);

		if(readPointer == S3_RX_END) readPointer = S3_RX_START;
	}

	spiWriteWord(REG_S3_RX_RD0, readPointer);     // Write back new pointer
	spiWriteReg(REG_S3_CR, CR_RECV);

	while(spiReadReg(REG_S3_CR));

	DBG_TFTP_EX(
		tracePGMlnTftp(mDebugTftp_BLEFT);
		tracenum(spiReadWord(REG_S3_RX_RSR0));
	)

	// Dump packet
	DBG_TFTP_EX(
		bufPtr = buffer;
		tracePGM(mDebugTftp_NEWLINE);

		for(count = TFTP_PACKET_MAX_SIZE / 2; count--;) {
			uint16_t val = *bufPtr++;
			val |= (*bufPtr++) << 8;
			tracenum(val);

			if((count % 8) == 0 && count != 0) tracePGM(mDebugTftp_NEWLINE);
			else putch(0x20); //Print space
		}
	)

	// Set up return IP address and port
	uint8_t i;


	for(i = 0; i < 6; i++) spiWriteReg(REG_S3_DIPR0 + i, buffer[i]);

	DBG_TFTP(tracePGMlnTftp(mDebugTftp_RADDR);)

	// Parse packet
	uint16_t tftpDataLen = (buffer[6] << 8) + buffer[7];
	uint16_t tftpOpcode  = (buffer[8] << 8) + buffer[9];
	uint16_t tftpBlock   = (buffer[10] << 8) + buffer[11];

	DBG_TFTP(
		tracePGMlnTftp(mDebugTftp_BLOCK);
		tracenum(tftpBlock);
		tracePGM(mDebugTftp_OPCODE);
		tracenum(tftpOpcode);
		tracePGM(mDebugTftp_DLEN);
		tracenum(tftpDataLen - (TFTP_OPCODE_SIZE + TFTP_BLOCKNO_SIZE));
	)

	if((tftpOpcode == TFTP_OPCODE_DATA)
		&& ((tftpBlock > MAX_ADDR / 0x200) || (tftpBlock < highPacket) || (tftpBlock > highPacket + 1)))
		tftpOpcode = TFTP_OPCODE_UKN;

	if(tftpDataLen > (0x200 + TFTP_OPCODE_SIZE + TFTP_BLOCKNO_SIZE))
		tftpOpcode = TFTP_OPCODE_UKN;

	uint8_t returnCode = ERROR_UNKNOWN;
	uint16_t packetLength;


	switch(tftpOpcode) {

		case TFTP_OPCODE_WRQ: // Write request
			DBG_TFTP(tracePGMlnTftp(mDebugTftp_OPWRQ);)
			sockInit(tftpTransferPort);

			DBG_TFTP(
				tracePGMlnTftp(mDebugTftp_NPORT);
				tracenum(tftpTransferPort);
			)

			lastPacket = highPacket = 0;
			returnCode = ACK; // Send back acknowledge for packet 0
			break;

		case TFTP_OPCODE_DATA:
			DBG_TFTP(tracePGMlnTftp(mDebugTftp_OPDATA);)

			packetLength = tftpDataLen - (TFTP_OPCODE_SIZE + TFTP_BLOCKNO_SIZE);
			lastPacket = tftpBlock;
#if defined(RAMPZ)
			writeAddr = (((address_t)((tftpBlock - 1)/0x80) << 16) | ((address_t)((tftpBlock - 1)%0x80) << 9));
#else
			writeAddr = (address_t)((address_t)(tftpBlock - 1) << 9); // Flash write address for this block
#endif

			if((writeAddr + packetLength) > MAX_ADDR) {
				// Flash is full - abort with an error before a bootloader overwrite occurs
				// Application is now corrupt, so do not hand over.

				DBG_TFTP(tracePGMlnTftp(mDebugTftp_FULL);)

				returnCode = ERROR_FULL;
			} else {

				DBG_TFTP(
					tracePGMlnTftp(mDebugTftp_WRADDR);
					traceadd(writeAddr);
				)

				uint8_t* pageBase = buffer + (UDP_HEADER_SIZE + TFTP_OPCODE_SIZE + TFTP_BLOCKNO_SIZE); // Start of block data
				uint16_t offset = 0; // Block offset


				// Set the return code before packetLength gets rounded up
				if(packetLength < TFTP_DATA_SIZE) returnCode = FINAL_ACK;
				else returnCode = ACK;

				// Round up packet length to a full flash sector size
				while(packetLength % SPM_PAGESIZE) packetLength++;

				DBG_TFTP(
					tracePGMlnTftp(mDebugTftp_PLEN);
					tracenum(packetLength);
				)

				if(writeAddr == 0) {
					// First sector - validate
					if(!validImage(pageBase)) {

#if defined(__AVR_ATmega328__) || defined(__AVR_ATmega328P__)
						/* FIXME: Validity checks. Small programms (under 512 bytes?) don't
						 * have the the JMP sections and that is why app.bin was failing.
						 * When flashing big binaries is fixed, uncomment the break below.*/
						returnCode = INVALID_IMAGE;
						break;
#endif
					}
				}

				// Flash packets
				uint16_t writeValue;
				for(offset = 0; offset < packetLength;) {
					writeValue = (pageBase[offset]) | (pageBase[offset + 1] << 8);
					boot_page_fill(writeAddr + offset, writeValue);

					DBG_TFTP_EX(
						if((offset == 0) || ((offset == (packetLength - 2)))) {
							tracePGMlnTftp(mDebugTftp_WRITE);
							tracenum(writeValue);
							tracePGM(mDebugTftp_OFFSET);
							tracenum(writeAddr + offset);
						}
					)

					offset += 2;

					if(offset % SPM_PAGESIZE == 0) {
						boot_page_erase(writeAddr + offset - SPM_PAGESIZE);
						boot_spm_busy_wait();
						boot_page_write(writeAddr + offset - SPM_PAGESIZE);
						boot_spm_busy_wait();
#if defined(RWWSRE)
						// Reenable read access to flash
						boot_rww_enable();
#endif
					}
				}

				if(returnCode == FINAL_ACK) {
					// Flash is complete
					// Hand over to application

					DBG_TFTP(tracePGMlnTftp(mDebugTftp_DONE);)
				}
			}

			break;

		// Acknowledgment
		case TFTP_OPCODE_ACK:

			DBG_TFTP(tracePGMlnTftp(mDebugTftp_OPACK);)

			break;

		// Error signal
		case TFTP_OPCODE_ERROR:

			DBG_TFTP(tracePGMlnTftp(mDebugTftp_OPERR);)

			/* FIXME: Resetting might be needed here too */
			break;

		case TFTP_OPCODE_RRQ: // Read request
			DBG_TFTP(tracePGMlnTftp(mDebugTftp_OPRRQ);)
		default:
			DBG_TFTP(
				tracePGMlnTftp(mDebugTftp_INVOP);
				tracenum(tftpOpcode);
			)
			// Invalid - return error
			returnCode = ERROR_INVALID;
			break;

	}

	return(returnCode);
}


static void sendResponse(uint16_t response)
{
	uint8_t txBuffer[100];
	uint8_t* txPtr = txBuffer;
	uint8_t packetLength;
	uint16_t writePointer;

	writePointer = spiReadWord(REG_S3_TX_WR0) + S3_TX_START;

	switch(response) {
		default:

		case ERROR_UNKNOWN:
			// Send unknown error packet
			packetLength = TFTP_UNKNOWN_ERROR_LEN;
#if (FLASHEND > 0x10000)
			memcpy_PF(txBuffer, PROGMEM_OFFSET + (uint32_t)(uint16_t)tftp_unknown_error_packet, packetLength);
#else
			memcpy_P(txBuffer, tftp_unknown_error_packet, packetLength);
#endif
			break;

		case ERROR_INVALID:
			// Send invalid opcode packet
			packetLength = TFTP_OPCODE_ERROR_LEN;
#if (FLASHEND > 0x10000)
			memcpy_PF(txBuffer, PROGMEM_OFFSET + (uint32_t)(uint16_t)tftp_opcode_error_packet, packetLength);
#else
			memcpy_P(txBuffer, tftp_opcode_error_packet, packetLength);
#endif
			break;

		case ERROR_FULL:
			// Send unknown error packet
			packetLength = TFTP_FULL_ERROR_LEN;
#if (FLASHEND > 0x10000)
			memcpy_PF(txBuffer, PROGMEM_OFFSET + (uint32_t)(uint16_t)tftp_full_error_packet, packetLength);
#else
			memcpy_P(txBuffer, tftp_full_error_packet, packetLength);
#endif
			break;
	case INVALID_IMAGE:
			packetLength = TFTP_INVALID_IMAGE_LEN;
#if (FLASHEND > 0x10000)
			memcpy_PF(txBuffer, PROGMEM_OFFSET + (uint32_t)(uint16_t)tftp_invalid_image_packet, packetLength);
#else
			memcpy_P(txBuffer, tftp_invalid_image_packet, packetLength);
#endif		
			break;
	case ACK:
			if(lastPacket > highPacket) highPacket = lastPacket;

			DBG_TFTP(tracePGMlnTftp(mDebugTftp_SACK);)
			/* no break */

		case FINAL_ACK:

			DBG_TFTP(
				if(response == FINAL_ACK)
					tracePGMlnTftp(mDebugTftp_SFACK);
			)

			packetLength = 4;
			*txPtr++ = TFTP_OPCODE_ACK >> 8;
			*txPtr++ = TFTP_OPCODE_ACK & 0xff;
			// lastPacket is block code
			*txPtr++ = lastPacket >> 8;
			*txPtr = lastPacket & 0xff;
			break;
	}

	txPtr = txBuffer;

	while(packetLength--) {
		spiWriteReg(writePointer++, *txPtr++);

		if(writePointer == S3_TX_END) writePointer = S3_TX_START;
	}

	spiWriteWord(REG_S3_TX_WR0, writePointer - S3_TX_START);
	spiWriteReg(REG_S3_CR, CR_SEND);

	while(spiReadReg(REG_S3_CR));

	DBG_TFTP(tracePGMlnTftp(mDebugTftp_RESP);)
}


/**
 * Initializes the network controller
 */
void tftpInit(void)
{
	// Open socket
	sockInit(TFTP_PORT);

	DBG_TFTP(
		tracePGMlnTftp(mDebugTftp_INIT);
		tracePGMlnTftp(mDebugTftp_PORT);
		tracenum(tftpTransferPort);
	)
}


/**
 * Looks for a connection
 */
uint8_t tftpPoll(void)
{
	uint8_t response = ACK;
	// Get the size of the recieved data
	uint16_t packetSize = spiReadWord(REG_S3_RX_RSR0);
	if (packetSize==0x800) {
		sockInit(tftpTransferPort);
		uint16_t retry=65536;
		while ((packetSize=spiReadWord(REG_S3_RX_RSR0))==0&&retry-->0);
	}
  packetSize=packetSize&0x07FF;
	if(packetSize) {
		tftpFlashing = TRUE;
		resetTick();
		// Process Packet and get TFTP response code
#if (DEBUG_TFTP > 0)
		response = processPacket(packetSize);
#else
		response = processPacket();
#endif
		// Send the response
		sendResponse(response);
	} 

	if(response == FINAL_ACK || response == INVALID_IMAGE) {
		spiWriteReg(REG_S3_CR, CR_CLOSE);
		tftpFlashing=FALSE;
		// Complete
		return(0);
	}

	// Tftp continues
	return(1);
}

