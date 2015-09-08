#ifndef _CONFIG_H
#define _CONFIG_H

// SPI
#define MCP_CS_BIT    PD0
#define MCP_CS_PORT   PORTD

#define SPI_REG_PIN_MCP_INT  PIND
#define SPI_PIN_MCP_INT      PD2

#define SPI_SOFTWARE

#define SPI_SS_MOSI_PORT PORTD
#define SPI_SS_MOSI_DDR DDRD
#define SPI_PIN_MOSI PD1

#define SPI_PORT PORTB
#define SPI_PIN PORTB
#define SPI_DDR DDRD
#define SPI_PIN_MISO PB6
#define SPI_PIN_SCK PB7

//#define CAN_INTERRUPT
//#define CAN_INT_NOBLOCK
//#define OPTIMISED_LAP

#define   ENABLE_CAN_INT()    GIMSK |= _BV(INT0)
#define   DISABLE_CAN_INT()   GIMSK &= ~_BV(INT0)
#define   SETUP_CAN_INT()     MCUCR |= 	_BV(ISC01)
#define   MCP_INT_VEC         INT0_vect

#define CAN_TX_BUFFER_SIZE 4
#define CAN_RX_BUFFER_SIZE 4

#define F_MCP 16000000UL


#endif // ifndef CONFIG_H

