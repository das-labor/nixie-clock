#define F_CPU 9216000UL
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/delay.h>

#include "can_handler.h"
#include "can/can.h"
#include "can/lap.h"
#include "can/spi.h"

uint8_t matrixSelect = 1, currentMatrixChar, bcdNibble, counter10ms;

uint8_t second, minute, hour, day, month, year;

uint8_t bcdMinute, bcdSecond, newSecond, clocktick10ms, bcdHour;

#ifdef __AVR_AT90S2313__
ISR(TIMER1_COMP1_vect)
#else
ISR(TIMER1_COMPA_vect)
#endif
{
	counter10ms++;
	if (counter10ms == 5) {
		/* each 10ms */
		counter10ms = 0;
		clocktick10ms++;
	}

	PORTB &= 0xC0; /* reset all character anodes */
	PORTD |= 0x78; /* activate all cathodes */

	matrixSelect <<= 1;
	currentMatrixChar++;
	if (currentMatrixChar == 6) {
		currentMatrixChar = 0;
		matrixSelect = 1;
	}

	if (currentMatrixChar < 2) {
		bcdNibble = bcdHour;
	} else if (currentMatrixChar - 2 < 2) {
		bcdNibble = bcdMinute;
	} else if (currentMatrixChar - 4 < 2) {
		bcdNibble = bcdSecond;
	}

	if (currentMatrixChar & 0x01) {
		bcdNibble &= 0x0F; /* select low nibble */
	} else {
		bcdNibble >>= 4; /* select high nibble */
	}

	_delay_us(54.5); /* wait safe time before switching the anodes */

	PORTD &= 0x03; 
	PORTD |= bcdNibble << 3; /* activate cathode selected by BCD value */
	PORTB |= matrixSelect; /* activate matching anode */
}

void time_request(void) {
	can_message *msg;
	msg = can_buffer_get();
	msg->addr_src = myaddr;
	msg->addr_dst = 0x00;
	msg->port_src = PORT_MGT;
	msg->port_dst = PORT_MGT;
	msg->dlc = 1;
	msg->data[0] = FKT_MGT_TIMEREQUEST2;
	can_transmit(msg);
}

void setup(void) {
	/* PB0-PB5 = Character Anodes
	 * PB6-PB7 = Outputs */
	DDRB = 0xFF;
	PORTB = 0x00; /* anode outputs off */

	/* PD0-PD1 = Inputs Pull-Up
	 * PD2 = DCF-Input Signal Pull-Up (INT0)
	 * PD3-PD6 = BCD Character Cathode Outputs */
	DDRD = 0x78;
	PORTD = 0x07; /* cathode outputs off */

	/* run Timer 1 with prescaler CLK/64 (every interrupt each 2 ms) */
	TCCR1A = 0;
	#ifdef __AVR_AT90S2313__
		TCCR1B = (1 << CTC1) | (1 << CS11) | (1 << CS10);
	#else
		TCCR1B = (1 << WGM12) | (1 << CS11) | (1 << CS10);
	#endif
	OCR1A = 0x0120;
	TIMSK = (1 << OCIE1A);

	//initialize spi port
	spi_init();
	
	//initialize can communication
	can_init();

	read_can_addr();

	time_request();
}

void addSecond(void) {
	second++;
	if (second == 60) {
		second = 0;
		minute++;
		if (minute == 60) {
			minute = 0;
			hour++;
			if (hour == 24) {
				hour = 0;
				day++;
				if ((day == 32) ||
				    (day == 31 && (month == 4 || month == 6 ||
				    month == 9 || month == 11)) ||
				    (day == 30 && month == 2) ||
				    (day == 29 && month == 2 && !(year & 0x03))) {
					month++;
					day = 1;
				}
				if (month == 13) {
					month = 1;
					year++;
				}
			}
		}
	}
}
int main(void) {
	setup();
	sei();

	while (1) {
		if (clocktick10ms >= 100) {
			clocktick10ms -= 100;
			addSecond();
			newSecond |= 0x01;
			if (minute == 0 && second == 0) {
				time_request();
			}
		}
		if (newSecond & 0x01) {
			bcdHour = hour - ((hour / 10) * 10) + ((hour / 10) << 4);
			bcdMinute = minute - ((minute / 10) * 10) + ((minute / 10) << 4);
			bcdSecond = second - ((second / 10) * 10) + ((second / 10) << 4);
			newSecond &= ~(0x01);
		}
		can_handler();
	}

	return 0;
}