#define F_CPU 9216000UL
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/delay.h>

const uint8_t DCF77values[] PROGMEM = {
	/* first bit 20 is always 1 and thus to be ignored */
	1, 2, 4, 8, 10, 20, 40, /* second values of bits 21 - 27 */
	0, /* parity bit 28 */
	1, 2, 4, 8, 10, 20, /* hour values of bits 29 - 34 */
	0, /* parity bit 35 */
	1, 2, 4, 8, 10, 20, /* day values of bits 36 - 41 */
	1, 2, 4, /* day of week values of bits 42 - 44 */
	1, 2, 4, 8, 10, /* month values of bits 45 - 49 */
	1, 2, 4, 8, 10, 20, 40, 80, /* year values of bits 50 - 57 */
	0 /* parity bit 58 */
	/* bit 59 has no second mark */
};

uint8_t matrixSelect = 1, currentMatrixChar, bcdNibble, counter10ms;

uint8_t DCFtick, DCFminute, DCFhour, DCFday, DCFmonth, DCFyear;

uint8_t currentDCFvalue, DCFerrors;
/* DCFerrors: Bit 1: signal error, Bit 0: parity error */

uint8_t second, minute, hour, day, month, year, DCFinput, lastDCFinput;
uint8_t clocktick10ms, DCFparity;

uint8_t bcdMinute, bcdSecond, newSecond, DCFtick10ms, lastDCFtick10ms, bcdHour;

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
		DCFtick10ms++;
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

ISR(INT0_vect) {
	lastDCFtick10ms = clocktick10ms;

	if (MCUCR & (1 << ISC00)) {
		/* rising edge: DCF state is low (connected to negative DCF output) */
		MCUCR &= ~(1 << ISC00);
		DCFinput = 0;
	} else {
		/* falling edge: DCF state is high (connected to negative DCF output) */
		MCUCR |= (1 << ISC00);
		DCFinput = 1;
	}
	
	clocktick10ms = 0;
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

	/* enable INT0 as falling edge interrupt */
	MCUCR = (1 << ISC01);
	GIMSK = (1 << INT0);

	/* run Timer 1 with prescaler CLK/64 (every interrupt each 2 ms) */
	TCCR1A = 0;
	#ifdef __AVR_AT90S2313__
		TCCR1B = (1 << CTC1) | (1 << CS11) | (1 << CS10);
	#else
		TCCR1B = (1 << WGM12) | (1 << CS11) | (1 << CS10);
	#endif
	OCR1A = 0x0120;
	TIMSK = (1 << OCIE1A);

	/* enable serial port at 9600 baud */
	#ifdef __AVR_AT90S2313__
		UCR = (1 << RXEN) | (1 << TXEN);
		UBRR = 0x3B;
	#else
		UCSRB = (1 << RXEN) | (1 << TXEN);
		UBRRL = 0x3B;
	#endif
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

void DCF77handler(uint8_t DCF77bit) {
	if (DCFtick >= 21) {
		if (DCF77bit & 0x01) {
			currentDCFvalue += pgm_read_byte(&(DCF77values[DCFtick - 21]));
		}
		DCFparity ^= DCF77bit;
	}

	switch (DCFtick) {
		case 21:
		case 29:
		case 36:
		/* reset parity bit at segment begin (unnecessary) */
		DCFparity = DCF77bit & 0x01;
		break;

		case 27:
		/* minute value complete */
		DCFminute = currentDCFvalue;
		currentDCFvalue = 0;
		break;

		case 34:
		/* hour value complete */
		DCFhour = currentDCFvalue;
		currentDCFvalue = 0;
		break;

		case 28:
		case 35:
		/* check parity bit */
		if (DCFparity != 0) {
			DCFerrors |= 0x01;
			DCFparity = 0;
		}
		break;

		case 41:
		/* day value complete */
		DCFday = currentDCFvalue;
		currentDCFvalue = 0;
		break;

		case 49:
		/* month value complete */
		DCFmonth = currentDCFvalue;
		case 44:
		/* weekday value complete (ignore) */
		currentDCFvalue = 0;
		break;

		case 58:
		/* year value complete, check date parity */
		DCFyear = currentDCFvalue;
		currentDCFvalue = 0;
		if (DCFparity != 0) {
			DCFerrors |= 0x01;
			DCFparity = 0;
		}
		break;
	}
}

int main(void) {
	setup();
	sei();
	
	while (1) {
		if (DCFtick10ms >= 100) {
			DCFtick10ms -= 100;
			addSecond();
			newSecond |= 0x01;
		}
		if (!(PIND & (1 << PD0))) {
			bcdSecond = ((lastDCFtick10ms / 10) << 4) + (lastDCFtick10ms % 10);
		}
		if (newSecond & 0x01) {
			bcdHour = hour - ((hour / 10) * 10) + ((hour / 10) << 4);
			bcdMinute = minute - ((minute / 10) * 10) + ((minute / 10) << 4);
			if (PIND & (1 << PD0)) {
				bcdSecond = second - ((second / 10) * 10) +
					((second / 10) << 4);
			}
		}
		if (lastDCFinput != DCFinput) {
			lastDCFinput = DCFinput;
			if (DCFinput == 1) {
				if (lastDCFtick10ms >= 7 && lastDCFtick10ms < 12) {
					/* ~100 ms pause: bit is 0 */
					DCF77handler(0);
				} else if (lastDCFtick10ms >= 17 && lastDCFtick10ms < 22) {
					/* ~200 ms pause: bit is 1 */
					DCF77handler(1);
				} else {
					/* signal inconsistency */
					DCFerrors |= 0x02;
				}
			} else if (DCFinput == 0) {
				DCFtick++;
				if (lastDCFtick10ms >= 170 && lastDCFtick10ms < 201) {
					/* minute begin detected */
					DCFtick = 0;
					if (DCFerrors == 0) {
						DCFtick10ms = 0;
						second = 0;
						minute = DCFminute;
						hour = DCFhour;
						day = DCFday;
						month = DCFmonth;
						year = DCFyear;
					}
					DCFerrors = 0;
				}
			}
		}
		newSecond &= ~(0x01);
	}

	return 0;
}