/*
 * Copyright (C) 2021 Jeffrey Janssen - cdi@nmotion.nl
 *
 * This software may be distributed and modified under the terms of the GNU
 * General Public License version 3 (GPL3) as published by the Free Software
 * Foundation and appearing in the file GPL-3.0.TXT included in the packaging
 * of this file. Please note that GPL3 Section 2[b] requires that all works
 * based on this software must also be made publicly available under the terms
 * of the GPL3 ("Copyleft").
 */

#include "CdiSerial.h"

// SAMD timer for the Arduino Nano 33 IoT
#ifdef ARDUINO_SAMD_NANO_33_IOT
#define SAMD true
#include "avdweb_SAMDtimer.h"
SAMDtimer SAMDTimer = SAMDtimer(3, TC_COUNTER_SIZE_16BIT, 3, 830);
#endif

// SAMD timer for the RP2040
#if (defined(ARDUINO_NANO_RP2040_CONNECT) || defined(ARDUINO_RASPBERRY_PI_PICO) || defined(ARDUINO_ADAFRUIT_FEATHER_RP2040) || \
	 defined(ARDUINO_GENERIC_RP2040)) &&                                                                                       \
	defined(ARDUINO_ARCH_MBED)
#define USING_MBED_RPI_PICO_TIMER_INTERRUPT true
#define RP2040 true
#include "TimerInterrupt_Generic.h"
MBED_RPI_PICO_Timer RPTimer(1);
#endif

CdiSerial CdiPlayers[2];

void CdiSerial::initialize(uint8_t _transmitPin)
{
	transmitPin = _transmitPin;

	// Set Transmit Pin to OUTPUT, setup timer
	pinMode(transmitPin, OUTPUT);

#ifdef SAMD
	SerialTimer.attachInterrupt(timerCallback);
	SerialTimer.enable(true);
#endif

#ifdef RP2040
	RPTimer.attachInterruptInterval(830, rpTimerHandler);
#endif
}

void CdiSerial::rpTimerHandler(uint alarm_num)
{
	// Always call this for MBED RP2040 before processing ISR
	TIMER_ISR_START(alarm_num);

	timerCallback(); // call the underlatying code

	// Always call this for MBED RP2040 after processing ISR
	TIMER_ISR_END(alarm_num);
}

void CdiSerial::stop()
{
#ifdef SAMD
	SerialTimer.enable(false);
#endif

#ifdef RP2040
	RPTimer.detachInterrupt();
#endif
}

void CdiSerial::serialWrite()
{
	if (transmitPin == 0xFF)
		return;
	digitalWrite(transmitPin, (data & 1) ? LOW : HIGH);
}
void CdiSerial::serialNext()
{
	if (transmitPin == 0xFF)
		return;
	if (data == 1)
	{ // Since the stop bit is high, when curData == 1 the full byte has been sent.
		if (readIndex != writeIndex)
		{
			// Move to and prepare next byte
			data = (buffer[readIndex] << 1) | 0x200; // Add start and stop bit
			readIndex = (readIndex + 1) % sizeof(buffer);
		}
	}
	else
	{
		data = data >> 1;
	}
}

void CdiSerial::timerCallback()
{
	CdiPlayers[0].serialWrite();
	CdiPlayers[1].serialWrite();
	CdiPlayers[0].serialNext();
	CdiPlayers[1].serialNext();
}
