//#include <avr/interrupt.h>
#include <util/delay.h>
#include <avr/io.h>
//#include <util/twi.h>

//#define DATA_LEN 20

//volatile uint8_t kint, l = ' ', kurisec = 59;
//volatile uint8_t buff[8], ilg;
//volatile uint8_t *Buffer = buff;

//typedef struct {
//  uint8_t seconds;
//  uint8_t minutes;
//  uint8_t hours;
//  uint8_t day;
//  uint8_t date;
//  uint8_t month;
//  uint8_t year;
//  uint8_t out;
//} ds1307Reg_t;

void SPI_MasterTransmit(char cData)
{
    /* Start transmission */
    SPDR = cData;
    /* Wait for transmission complete */
    while(!(SPSR & (1<<SPIF)))
        ;

    PORTB |= (1<<PB2);
    PORTB &= ~(1<<PB2);
    _delay_ms(100);
}

void SPI_MasterInit(void)
{
    /* Set MOSI and SCK output, all others input */
//    DDRB = (1<<DDB3)|(1<<DDB5);

    /* Enable SPI, Master, set clock rate fck/16 */
    SPCR = (1<<SPE)|(1<<MSTR)|(1<<SPR0);
}

int main(void)
{
	DDRB = 0xff; //sets all bits of port B for output
	DDRC = 0xff;
	DDRD = 0xff;
	PORTB = 0x00; //sets a output of port B to LOW
	PORTC = 0x00;
	PORTD = 0x00;
	asm volatile ("nop");

    SPI_MasterInit();

	// Repeat indefinitely
	for(;;)
	{
	    SPI_MasterTransmit(0x00);
	    SPI_MasterTransmit(0x00);
	    SPI_MasterTransmit(0x00);
	    SPI_MasterTransmit(0x00);
	    SPI_MasterTransmit(0x00);
	    SPI_MasterTransmit(0x00);
	    SPI_MasterTransmit(0x77|0x80); //0
	    SPI_MasterTransmit(0x41|0x80); //1
	    SPI_MasterTransmit(0x3B|0x80); //2
	    SPI_MasterTransmit(0x5B|0x80); //3
	    SPI_MasterTransmit(0x4D|0x80); //4
	    SPI_MasterTransmit(0x5E|0x80); //5
	    SPI_MasterTransmit(0x7E|0x80); //6
	    SPI_MasterTransmit(0x43|0x80); //7
	    SPI_MasterTransmit(0x7F|0x80); //8
	    SPI_MasterTransmit(0x5F|0x80); //9
//	    SPI_MasterTransmit(0x80); //taskas



//	    SPI_MasterTransmit(0x00);
//	    SPI_MasterTransmit(0xff);
//	    SPI_MasterTransmit(0x01);
//	    SPI_MasterTransmit(0x02);
//	    SPI_MasterTransmit(0x04);
//	    SPI_MasterTransmit(0x08);
//	    SPI_MasterTransmit(0x10);
//	    SPI_MasterTransmit(0x20);
//	    SPI_MasterTransmit(0x40);
//	    SPI_MasterTransmit(0x80);
    }
}
