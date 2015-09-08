#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/eeprom.h>

#include "can/can.h"
#include "can_handler.h"
#include "can/lap.h"

uint8_t myaddr;

void twi_get(uint8_t *p);

extern uint8_t second, minute, hour, day, month, year;
extern uint8_t clocktick10ms, newSecond;

void can_handler()
{
	can_message *msg;
	can_message *rx_msg;
	if ((rx_msg = can_get_nb())) //get next canmessage in rx_msg
	{
		if ((rx_msg->addr_dst == myaddr))
		{
			if (rx_msg->port_dst == PORT_MGT)
			{
				switch (rx_msg->data[0])
				{
					case FKT_MGT_RESET:
						wdt_enable(0);
						while (1);
						break;
					case FKT_MGT_PING:
						msg = can_buffer_get();
						msg->addr_src = myaddr;
						msg->addr_dst = rx_msg->addr_src;
						msg->port_src = PORT_MGT;
						msg->port_dst = PORT_MGT;
						msg->dlc = 1;
						msg->data[0] = FKT_MGT_PONG;
						can_transmit(msg);
						break;
					case FKT_MGT_TIMEREPLY2:
						year = rx_msg->data[1];
						month = rx_msg->data[2];
						day = rx_msg->data[3];
						hour = rx_msg->data[4];
						minute = rx_msg->data[5];
						second = rx_msg->data[6];
						clocktick10ms = rx_msg->data[7];
						newSecond |= 0x01;
						break;
				}
			}
		}

		can_free(rx_msg);
	}
}

void read_can_addr()
{
	myaddr = eeprom_read_byte(0x00);
}

