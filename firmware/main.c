#include <avr/interrupt.h>
#include <util/delay.h>
#include <avr/io.h>
#include <util/twi.h>
//#include <math.h>
//#include "clock.h"

#define DATA_LEN 20

volatile uint8_t kint, l = ' ', kurisec = 59;
volatile uint8_t buff[8], ilg;
volatile uint8_t *Buffer = buff;

typedef struct {
  uint8_t seconds;
  uint8_t minutes;
  uint8_t hours;
  uint8_t day;
  uint8_t date;
  uint8_t month;
  uint8_t year;
  uint8_t out;
} ds1307Reg_t;

typedef struct {
	uint8_t issaugota;			//nuo 0x08
	uint8_t tipas;					//0x09

  uint8_t ilgi;           //0x0A
	uint8_t trumpi;					//0x0B

  uint8_t ilg1;           //0x0C
	uint8_t ilg2;
	uint8_t ilg3;

	uint8_t trum1;					//0x0F
	uint8_t trum2;
	uint8_t trum3;
	uint8_t trum4;
	uint8_t trum5;
	uint8_t trum6;

  uint8_t minus;          //0x15 enable or disable. Jei disable tada plius tiek.
	uint8_t seconds;				//0x16
	uint8_t	minutes;
	uint8_t hours;

  uint8_t diff_sec;       //0x19
  uint8_t diff_min;
  uint8_t diff_hour;

} start_t;


volatile ds1307Reg_t tdClock;
volatile start_t oStart;

void USART_vInit(void)
{
	// Set baud rate
	UBRRH = (uint8_t)(12>>8);
	UBRRL = (uint8_t)12;
	// Set frame format to 8 data bits, no parity, 1 stop bit
	// Enable receiver and transmitter
	UCSRB = (1<<RXEN)|(1<<TXEN)|(1<<RXCIE);
	UCSRC = (1<<URSEL)|(1<<USBS)|(3<<UCSZ0);
}


void sendf(uint8_t *idata, uint8_t kiek)
{
	//unsigned char * idata = (unsigned char *)(&data);
	for (uint8_t i = 1;i <= kiek;i++){
		while(!(UCSRA&(1<<UDRE)));
		UDR=*idata++;
	}
}

void generateStop(void)
{
  // generate a STOP condition
  TWCR = (1<<TWINT)|(1<<TWSTO)|(1<<TWEN);
  // wait for STOP generated
  _delay_ms(40);
}

void send(uint8_t data)
{
	while(!(UCSRA&(1<<UDRE)));
	UDR=data;
}

void sla(uint8_t mode)
{
	TWCR = (1<<TWINT)|(1<<TWSTA)|(1<<TWEN);	// send START condition
	while (!(TWCR & (1<<TWINT)));				// wait for start condition transmitted
	if (TW_STATUS != TW_START) send(0xF1);		// check status code
	TWCR= (1<<TWEN);
	TWDR = 0B11010000 | mode;					// load SLA+R =1 or SLA+W =0
	TWCR = (1<<TWINT) | (1<<TWEN);				// transmit SLA
	while (!(TWCR & (1<<TWINT)));				// wait for SLA transmitted
// check status code
	if (mode == 0){
		if (TW_STATUS != TW_MT_SLA_ACK)
		{
			send(0xF2);
		}else{;}
	}else if (TW_STATUS != TW_MR_SLA_ACK){
		send(0xF3);
	}
}


void twiWrite(uint8_t* data, uint8_t count)
{
  sla(0);//write mode 0
  // send data byte(s)
  do
  {
    // send next data byte
    TWDR = *data++;
    TWCR = (1<<TWINT) | (1<<TWEN);
    // wait for data byte transmitted
    while (!(TWCR & (1<<TWINT)));
	if ( TW_STATUS != TW_MT_DATA_ACK ) send(0xF4);
  } while ( --count != 0 );
  // generate STOP
  generateStop( );
} // end twiWrite

void twiRead(uint8_t* data, uint8_t count)
{
  // send SLA+R
  sla(1);//write mode 1
  do
  {
    // set up to receive next data byte
    TWCR = (1<<TWINT) | (1<<TWEN) | ((count != 1) ? (1<<TWEA) : 0);
    // wait for data byte
    while (!(TWCR & (1<<TWINT)));
    // check for timeout and check status
    if (TW_STATUS != ( (count != 1) ? TW_MR_DATA_ACK : TW_MR_DATA_NACK)) send(0xF5);
    // read data byte
    *(data++) = TWDR;
  } while ( --count != 0 );

  // generate STOP
  generateStop( );
} // end read data

void ReadTime(void) //nuskaito is laikrodzio tik laika hh:mm:ss
{
	uint8_t tmp[3];
	uint8_t reg = 0;
	// write DS1307 register address
	twiWrite(&reg, 1);
	// read DS1307 registers
	twiRead(tmp, 3);
	tmp[0] &= 0x7F;
	tdClock.seconds = (tmp[0] >> 4) * 10 + (tmp[0] & 0x0F);
	tdClock.minutes = (tmp[1] >> 4) * 10 + (tmp[1] & 0x0F);
	tmp[2] &= 0x3F;
	tdClock.hours = (tmp[2] >> 4) * 10 + (tmp[2] & 0x0F);
}

void ReadDate(void)		//nuskaito ir data ir likusia informacija
{
	uint8_t tmp[5];
	uint8_t reg = 3;
	// write DS1307 register address
	twiWrite(&reg, 1);
	// read DS1307 registers
	twiRead(tmp, 5);
	tdClock.day = tmp[0];
	tdClock.date = (tmp[1] >> 4) * 10 + (tmp[1] & 0x0F);
	tdClock.month = (tmp[2] >> 4) * 10 + (tmp[2] & 0x0F);
	tdClock.year = (tmp[3] >> 4) * 10 + (tmp[3] & 0x0F);
	tdClock.out = tmp[4];
}

void ReadOption(void)
{
	uint8_t reg = 8;
	twiWrite(&reg, 1);
	twiRead((uint8_t *) &oStart, DATA_LEN);
}


/*void ReadAll(uint8_t* Kur, uint8_t nuo, uint8_t kiek)
{
	uint8_t reg = nuo;
	// write DS1307 register address
	twiWrite(&reg, 1);
	// read DS1307 registers
	twiRead((uint8_t*) &Kur, kiek);
}*/
/*
void WriteAll(uint8_t* ka, uint8_t nuo, uint8_t kiek){
	uint8_t data[12], i = 1;
	data[0] = nuo;
	do{
	  data[i] = *ka++;
		i++;
	} while(--kiek != 0);
	twiWrite(data, kiek);
}*/

void ShowAllMemory(void)
{
	ReadOption();
	sendf((uint8_t *) &oStart, DATA_LEN);
}

void showDateTime(void)
{
  //ReadAll((uint8_t *) &tdClock, 0, 8);
	ReadTime();
	ReadDate();
	send(tdClock.seconds);
	send(tdClock.minutes);
	send(tdClock.hours);
	send(tdClock.day);
	send(tdClock.date);
	send(tdClock.month);
	send(tdClock.year);
	send(tdClock.out);
}

void settime(uint8_t nuo, uint8_t kiek)
{
	for(uint8_t i = kiek; i > 0; i--)
	{
	  buff[i] = ((buff[i-1] / 10) << 4) | (buff[i-1] % 10);
	}
	buff[0]=nuo;
	twiWrite ((uint8_t *) &buff, kiek+1);
}

void skaicius(uint8_t kur, uint8_t kiek)
//kur - nurodo kuri skaitmeni uzdegti, jei 0 tada pereinu visus, jei daugiau kaip 6 tada nedegu jokio.
//kiek - nurodo koks skaitmuo turi sviesti nuo 0 iki 9, jei gaunu klaidinga koda tada schema uzgesina, todel naudoju 0x0A
{
  uint8_t tmp, k;
  if (kur <= 0x06) {
    tmp = PORTB;
    tmp &= 0xF0;
    tmp |= kiek;
    PORTB = tmp;
    tmp = PORTC;
    if (kur == 0)
    {
      k = 6;
      kur = 1;
    } else {
      k = kur;
    }
    for(uint8_t i = kur; i<=k; i++)
    {
      tmp &= 0xF8;
      tmp |= i;
      PORTC = tmp;
      tmp &= 0xF8;
      PORTC = tmp;
    }
  }
	/*if((kur == 0x03) & (kas != kiek) & ((kiek == 8) | (kiek == 9))){
	PORTD |= 0x40;
	tmp = PORTD;
	_delay_ms(250);
	tmp &= 0x3F;
	PORTD = tmp;
  kas	= kiek;
	}
	if((kur == 0x03) & (kas != kiek) & (kiek == 0)){
	PORTD |= 0xC0;
	tmp = PORTD;
	_delay_ms(250);
	tmp &= 0x3F;
	PORTD = tmp;
	kas	= kiek;
	}*/
}

void PypSk(uint8_t kas, uint8_t nuo, uint8_t kiek)
{
  uint8_t ret = 0, mygtukas, reg[2] = {nuo, kas}, i = 0, j = 0;
	skaicius(0x03, reg[1]);
	while (ret == 0)
	{
		while((PIND & 0x3C) != 0x3C) {_delay_ms(10);} 	//laukti atleidimo
    while((PIND & 0x3C) == 0x3C) {
      if(i == 250){
        i = 0;
        if(j >= 2){
          skaicius(0x03, 0x0A);
          j = 0;
        } else {
          skaicius(0x03, reg[1]);
          j++;
        }
      } else {
        _delay_ms(1);
        i++;
      }
    }                 //laukti paspaudimo
		_delay_ms(10);
		mygtukas = (0x3c & ~PIND);
		switch (mygtukas)
		{
			case 4: 	//ENTER
				twiWrite(reg, 2);
				ret = 1;
				break;
			case 8:		//EXIT
				ret = 1;
				break;
			case 16:	//UP
				if(reg[1] == kiek){
					reg[1] = 0x00;
				} else {
					reg[1] += 0x01;
				}
				break;
			case 32:	//DOWN
				if(reg[1] == 0x00){
					reg[1] = kiek;
				} else {
					reg[1] -= 0x01;
				}
				break;
		}
		skaicius(0x03, reg[1]);
	}
	skaicius(0x00, 0x0A);
	ReadOption();
}

void Pyp(uint8_t *kur, uint8_t nuo, uint8_t kiek)
{
  uint8_t ret = 0, mygtukas, reg[kiek+1], k = 1, i = 0, j = 0;
	reg[0] = nuo;
	for (uint8_t i = 1; i<= kiek; i++)
	{
		reg[i] = *kur++;
	}
	skaicius(0x03, reg[1]%10);
	skaicius(0x05, reg[1]/10);
	skaicius(0x06, kiek);
	skaicius(0x01, k);
	while (ret == 0)
	{
		while((PIND & 0x3C) != 0x3C) {_delay_ms(10);} 	//laukti atleidimo
    while((PIND & 0x3C) == 0x3C) {
      if(i == 250){
        i = 0;
        if(j >= 2){
          skaicius(0x03, 0x0A);
          skaicius(0x05, 0x0A);
          j = 0;
        } else {
          skaicius(0x03, reg[k]%10);
          skaicius(0x05, reg[k]/10);
          j++;
        }
      } else {
        _delay_ms(1);
        i++;
      }
    }                 //laukti paspaudimo
		_delay_ms(10);
		mygtukas = (0x3c & ~PIND);
		switch (mygtukas)
		{
			case 4: 	//ENTER
				if( k == kiek){
					twiWrite(reg, kiek+1);
					ret = 1;
				} else {
					k ++;
				}
				break;
			case 8:		//EXIT
				ret = 1;
				break;
			case 16:	//UP
				if(reg[k] == 0x3B){ //59
					reg[k] = 0x00;
				} else {
					reg[k] += 0x01;
				}
				break;
			case 32:	//DOWN
				if(reg[k] == 0x00){
					reg[k] = 0x3B;
				} else {
					reg[k] -= 0x01;
				}
				break;
		}
		skaicius(0x03, reg[k]%10);
		skaicius(0x05, reg[k]/10);
		skaicius(0x01, k);
	}
	skaicius(0x00, 0x0A);
	ReadOption();
}

void DisplayTime(void)
{
	skaicius(0x03, tdClock.seconds%10); 	//sekundziu vienetai
	skaicius(0x05, tdClock.seconds/10);		 	//sekundziu desimtys
	if((tdClock.minutes == 0x00) && (tdClock.hours == 0x00)){
		skaicius(0x01, 0x0A); 	//minuciu vienetai
		skaicius(0x06, 0x0A);		 	//minuciu desimtys
	} else {
		skaicius(0x01, tdClock.minutes%10); 	//minuciu vienetai
		skaicius(0x06, tdClock.minutes/10);		 	//minuciu desimtys
	}
	if(tdClock.hours == 0x00){
		skaicius(0x02, 0x0A); 	//valandu vienetai
		skaicius(0x04, 0x0A);		 	//valandu desimtys
	} else {
		skaicius(0x02, tdClock.hours%10); 	//valandu vienetai
    if ( tdClock.hours/10 == 0)
    {
      skaicius(0x04, 0x0A);     //valandu desimtys
    } else {
      skaicius(0x04, tdClock.hours/10);     //valandu desimtys
    }
	}
}

void LaikoNustatymas(void)
{
	unsigned char ret = 0, j = 0, k = 2;
	uint8_t skaitmuo[3][3] = {{0x02, 0x04, 24}, {0x01, 0x06, 59}, {0x03, 0x05, 59}};
	uint8_t mygtukas = 0x3C, i= 0;
	ReadTime();
	DisplayTime();
	buff[0] = tdClock.seconds;
	buff[1] = tdClock.minutes;
	buff[2] = tdClock.hours;
	while(ret == 0)
	{
		while((PIND & 0x3C) != 0x3C) {_delay_ms(10);} 	//laukti atleidimo
		while((PIND & 0x3C) == 0x3C) {								 	//laukti paspaudimo
      if(i == 250){
				i = 0;
        if(j >= 2){
					skaicius(skaitmuo[2-k][0], 0x0A);
					skaicius(skaitmuo[2-k][1], 0x0A);
				  j = 0;
				} else {
					for(uint8_t n = 0; n < 3; n++)
					{
						skaicius(skaitmuo[n][0], buff[2-n]%10);
						skaicius(skaitmuo[n][1], buff[2-n]/10);
					}
          j++;
				}
			} else {
				_delay_ms(1);
				i++;
			}
		}
		_delay_ms(10);
		mygtukas = (0x3c & ~PIND);
		switch (mygtukas)
		{
			case 4: 	//ENTER
				if(k == 0){
					settime(0, 3);
					ret = 1;
				} else {
				  k--;
				}
				break;
			case 8:		//EXIT
				ret = 1;
				break;
			case 16:	//UP
				if(buff[k] == skaitmuo[2-k][2]){
					buff[k] = 0x00;
				} else {
					buff[k] += 0x01;
				}
				break;
			case 32:	//DOWN
				if(buff[k] == 0x00){
					buff[k] = skaitmuo[2-k][2];
				} else {
					buff[k] -= 0x01;
				}
				break;
		}
		skaicius(skaitmuo[k][0], buff[2-k]%10);
		skaicius(skaitmuo[k][1], buff[2-k]/10);
	}
	skaicius(0x00, 0x0A);
}

void DiffLaikoNustatymas(void)
{
  unsigned char ret = 0, j = 0, k = 3;
  uint8_t skaitmuo[3][3] = {{0x02, 0x08, 9}, {0x01, 0x06, 59}, {0x03, 0x05, 59}}; //0x07 tam, kad nerodytu jokio skaiciaus toje pozicijoje.
	uint8_t mygtukas = 0x3C, i= 0;
  skaicius(0x03, oStart.diff_sec%10);   //sekundziu vienetai
  skaicius(0x05, oStart.diff_sec/10);     //sekundziu desimtys
  skaicius(0x01, oStart.diff_min%10);   //minuciu vienetai
  skaicius(0x06, oStart.diff_min/10);     //minuciu desimtys
  skaicius(0x02, oStart.diff_hour%10);   //valandu vienetai
  skaicius(0x04, 0x0A);               //valandu desimtys

  buff[0] = 0x19; //vieta nuo kurios saugoti laikrodzio atmintyje
  buff[1] = oStart.diff_sec;
  buff[2] = oStart.diff_min;
  buff[3] = oStart.diff_hour;
	while(ret == 0)
	{
		while((PIND & 0x3C) != 0x3C) {_delay_ms(10);} 	//laukti atleidimo
		while((PIND & 0x3C) == 0x3C) {								 	//laukti paspaudimo
			if(i == 250){
				i = 0;
        if(j >= 2){
          skaicius(skaitmuo[3-k][0], 0x0A);
          skaicius(skaitmuo[3-k][1], 0x0A);
				  j = 0;
				} else {
          for(uint8_t n = 0; n < 3; n++)
					{
            skaicius(skaitmuo[n][0], buff[3-n]%10);
            skaicius(skaitmuo[n][1], buff[3-n]/10);
					}
          j++;
				}
			} else {
				_delay_ms(1);
				i++;
			}
		}
		_delay_ms(10);
		mygtukas = (0x3c & ~PIND);
		switch (mygtukas)
		{
			case 4: 	//ENTER
        if(k == 1){
          buff[3] &= 0x0F;
          twiWrite ((uint8_t *) &buff, 4);
					ret = 1;
				} else {
				  k--;
				}
				break;
			case 8:		//EXIT
				ret = 1;
				break;
			case 16:	//UP
        if(buff[k] == skaitmuo[3-k][2]){
					buff[k] = 0x00;
				} else {
					buff[k] += 0x01;
				}
				break;
			case 32:	//DOWN
				if(buff[k] == 0x00){
          buff[k] = skaitmuo[3-k][2];
				} else {
					buff[k] -= 0x01;
				}
				break;
		}
    skaicius(skaitmuo[3-k][0], buff[k]%10);
    skaicius(skaitmuo[3-k][1], buff[k]/10);
	}
	skaicius(0x00, 0x0A);
}

void Batareika(void)
{
  unsigned char h=0, l=0, ret=0;
	int32_t rod=0;
	skaicius(0x00, 0x0A);
  while((PIND & 0x3C) != 0x3C) {_delay_ms(10);}   //laukti atleidimo
  while((PIND & 0x3C) == 0x3C) {                  //laukti paspaudimo
	  rod = 0;
	  for(uint8_t i = 1; i < 100; i++){
			ADCSRA = ADCSRA | (1<<ADSC);
			while((ADCSRA & 0x10) != 0x10){;}
			l=ADCL;
			h=ADCH;
			ADCSRA = ADCSRA & ~(1<<ADIF);
			if(h == 0x03){
				ret = l - 0x02;
			} else {
			  ret = 0;
			}
			rod += ret;
			//_delay_ms(0);
		}
    ret = (uint8_t) (rod/120);
		skaicius(0x01, ret / 100);
		skaicius(0x05, ret / 10 % 10);
		skaicius(0x03, ret % 10);
		_delay_ms(200);
  }
  skaicius(0x00, 0x0A);
}

/*ISR(ADC_vect)
{
  send(ADCL);
  send(ADCH);
  send('p');
}*/

void writeNustatymus(void)
{
	uint8_t reg[DATA_LEN+1], *tmp;
	tmp = (uint8_t *) &oStart;
	reg[0] = 8;
	for (uint8_t i = 1; i<=DATA_LEN+1; i++){
		reg[i] = *tmp++;
	}
		twiWrite(reg, DATA_LEN+1);
}

void NustatytiDefault(void)
{
	oStart.issaugota = 1;
  oStart.tipas    = 5;
  oStart.ilgi     = 1;
	oStart.trumpi		= 2;
	oStart.ilg1			= 0;
	oStart.ilg2			= 0;
	oStart.ilg3			= 0;
	oStart.trum1		= 58;
	oStart.trum2		= 59;
	oStart.trum3		= 0;
	oStart.trum4		= 0;
	oStart.trum5		= 0;
	oStart.trum6		= 0;
  oStart.minus    = 0;
  oStart.seconds  = 0;
  oStart.minutes  = 0;
  oStart.hours    = 0;
  oStart.diff_sec = 59;
  oStart.diff_min = 59;
  oStart.diff_hour= 1;
	writeNustatymus();
}

void garsas(void)
{
	uint8_t *tmp;
	tmp = (uint8_t *) &oStart.trum1;
	for(uint8_t i = 0; i < oStart.trumpi; i++){
		if(tdClock.seconds == *tmp){
			if(kurisec != tdClock.seconds){
				PORTD |= 0x40;
				_delay_ms(100);
				PORTD &= 0x3F;
				kurisec = tdClock.seconds;
			}
		}
		tmp++;
	}
	tmp = (uint8_t *) &oStart.ilg1;
	for(uint8_t i = 0; i < oStart.ilgi; i++){
		if(tdClock.seconds == *tmp){
			if(kurisec != tdClock.seconds){
				PORTD |= 0xC0;
				_delay_ms(400);
				kurisec = tdClock.seconds;
				PORTD &= 0x3F;
			}
		}
		tmp++;
	}
}

int atimtiLaikus(void)
{
	uint8_t ret = 0;
	int32_t real, start;
	real = ((int32_t) tdClock.seconds) + ((int32_t) tdClock.minutes) * 60 + ((int32_t) tdClock.hours) * 3600;
	start = ((int32_t) oStart.seconds) + ((int32_t) oStart.minutes) * 60 + ((int32_t) oStart.hours) * 3600;
  if ( oStart.tipas == 0x02){
    if ( start > real){
      start = 86400 - start + real;
    } else {
      start = real - start;
    }
  }else{
    if ( start > real ){
      start = start - real;
      ret = 1;
    } else if ( real < start ){
      start = start + (86400 - real);
      ret = 1;
    } else if ( real == start ){
      start = real - start;
      oStart.tipas = 0x02;
    }
  }
  /*if(real < start){
    start -= 43200;  //12h = 43200 sec
	}
	if(oStart.tipas == 3){
		start += (oStart.minus * 60);
	}
	start = real - start;
	if (start < 0){
		start = -1*start;
		ret = 1;
  } */
	tdClock.seconds = start % 60;
	tdClock.minutes = start % 3600 / 60;
	start = start / 3600;
	tdClock.hours = start;
	DisplayTime();
	return ret;
}

ISR(INT0_vect)
{
  cli();
  uint8_t mygtukas = 0x3C, kodas = 0x1A, low, high;
  int32_t real, start;
  unsigned char ret = 0;
  skaicius(0x00, 0x0A);
  skaicius(0x04, kodas>>4);
  PORTB &= ~(1<<4);
  //skaicius(0x02, kodas&0x0F);  sakyciau dubliuojasi su dviem eilutemis auksciau
  while(ret == 0)
  {
    while((PIND & 0x3C) != 0x3C) {_delay_ms(10);}
    while((PIND & 0x3C) == 0x3C) {;}
    _delay_ms(10);
    mygtukas = (0x3c & ~PIND);
    //skaicius(3, mygtukas>>2);
    switch (mygtukas)
    {
      case 4:     //paspaude ENTER
        if((kodas&0x0F) == 0x0A){
          kodas &= 0xF0;
          kodas |= 0x01;
        }else{
          switch(kodas)
          {
            case 0x13:      //startas nuo +x:XX:xX
              ReadTime();
              real = ((int32_t) tdClock.seconds) + ((int32_t) tdClock.minutes) * 60 + ((int32_t) tdClock.hours) * 3600;
              start = ((int32_t) oStart.diff_sec) + ((int32_t) oStart.diff_min) * 60 + ((int32_t) oStart.diff_hour) * 3600;
              if ( real < start ){
                start = 86400 - (start - real);  //86400 = 24:00:00
              } else {
                start = real - start;
              }
              oStart.seconds = start % 60;
              oStart.minutes = start % 3600 / 60;
              start = start / 3600;
              oStart.hours = start;
              oStart.minus = 0x00;
              oStart.tipas = 0x02; //startas nuo 00:00:00, tiesiog starto laika nustatau kaip turi buti.
              writeNustatymus();
              ret = 1;
              break;
            case 0x14:      //startas nuo -x:xx:xx
              ReadTime();
              real = ((int32_t) tdClock.seconds) + ((int32_t) tdClock.minutes) * 60 + ((int32_t) tdClock.hours) * 3600;
              start = ((int32_t) oStart.diff_sec) + ((int32_t) oStart.diff_min) * 60 + ((int32_t) oStart.diff_hour) * 3600;
              start = real + start;
              if ( start > 86400 ){
                start = start - 86400;
              }
              oStart.seconds = start % 60;
              oStart.minutes = start % 3600 / 60;
              start = start / 3600;
              oStart.hours = start;
              oStart.minus = 0x01;
              oStart.tipas = 0x04;
              writeNustatymus();
              ret = 1;
              break;
            case 0x12:      //startas nuo 00:00:00
              ReadTime();
              oStart.seconds = tdClock.seconds;
              oStart.minutes = tdClock.minutes;
              oStart.hours = tdClock.hours;
              oStart.minus = 0x00;
            case 0x11:    //startas realiu laiku
            case 0x15:    //laikrodis
              oStart.tipas = kodas&0x0F;
              writeNustatymus();
              ret = 1;
              break;
            case 0x21:  //nustatynesim kiek ilgu pipsejimu bus
              PypSk(oStart.ilgi, 0x0A, 3);
              break;
            case 0x22:  //nustatysim kiek trumpu pipsejimu bus
              PypSk(oStart.trumpi, 0x0B, 6);
              break;
            case 0x23:  //nustatysim ilgu pipsejimu sekundes
              Pyp((uint8_t *) &oStart.ilg1, 0x0C, oStart.ilgi);
              break;
            case 0x24:  //nustatysim trumpu pipsejimu sekundes
              Pyp((uint8_t *) &oStart.trum1, 0x0F, oStart.trumpi);
              break;
            case 0x25:  //nustatom startas nuo nustatyto laiko
              DiffLaikoNustatymas();
              ReadOption();
              break;
            case 0x91:  //nustatomas laikrodzio vidinis laikas
              LaikoNustatymas();
              break;
            case 0x92:  //rodo kiek liko akumuliatoriaus procentais
              Batareika();
              break;
            case 0x93:  //resetinam visus nustatymus i gamyklinius
              NustatytiDefault();
              //ReadOption();
              ret = 1;
              break;
          }
        }
        break;
      case 8:     //paspaude EXIT
        if((kodas&0x0F) == 0x0A){
          ret = 1;
        }else{
          kodas &= 0xF0;
          kodas |= 0x0A;
        }
        break;
      case 16:    //paspaude AUKSTYN
        low = kodas & 0x0F;
        high = kodas & 0XF0;
        if(low == 0x0A){
          kodas -= 0x10;
          if(high == 0x10){ //tikrinu ar neislipau is reziu pirmo lygio meniu
            kodas &= 0x0F;
            kodas |= 0x90;
          } else if(high == 0x90){ //tikrinu ar neislipau is reziu pirmo lygio meniu
            kodas &= 0x0F;
            kodas |= 0x20;
          }
        } else {
          kodas -= 0x01;
          if(low == 0x01){
            if(high == 0x10){ //tikrinu ar neislipau is reziu antro lygio meniu
              kodas &= 0xF0;
              kodas |= 0x05;
            } else if(high == 0x20){ //tikrinu ar neislipau is reziu antro lygio meniu
              kodas &= 0xF0;
              kodas |= 0x05;
            } else if(high == 0x90){ //tikrinu ar neislipau is reziu antro lygio meniu
              kodas &= 0xF0;
              kodas |= 0x03;
            }
          }
        }
        break;
      case 32:    //paspaude ZEMYN
        low = kodas & 0x0F;
        high = kodas & 0XF0;
        if(low == 0X0A){
          kodas += 0x10;
          if(high == 0x20){ //tikrinu ar neislipau is reziu pirmo lygio meniu
            kodas &= 0x0F;
            kodas |= 0X90;
          }else if(high == 0x90){ //tikrinu ar neislipau is reziu pirmo lygio meniu
            kodas &= 0x0F;
            kodas |= 0X10;
          }
        } else {
          kodas += 0x01;
          if(((high == 0x10) && (low == 0x05)) || ((high == 0x20) && (low == 0x05)) || ((high == 0x90) && (low == 0x03))){ //tikrinu neislipau is reziu antro lygio meniu
            kodas &= 0xF0;
            kodas |= 0X01;
          }
        }
        break;
    }
    skaicius(4, kodas>>4);
    skaicius(2, kodas&0x0F);
  }
  skaicius(0x00, 0x0A);
  _delay_ms(300);
  sei();
}

ISR(USART_RXC_vect)
{
//sendf((unsigned char *)(&did));
//send(';');
//sendf((unsigned char *)(&kabl));
kint=UDR;
switch (kint)
	{
	case 'y':
	case 'm':
  case 't':
		settime(0, 3);
		l=' ';
		Buffer=&buff[0];
    break;
  case 'w':
		twiWrite((uint8_t *) &buff[1], buff[0]);
		l=' ';
		Buffer=&buff[0];
    break;
  case 'b':
		l='b';
    break;
  case '!':
		ShowAllMemory();
    break;
  case '?':
		showDateTime();
    break;
  case 'v':
    ADCSRA = ADCSRA | (1<<ADSC);
    break;
	default:{
		if (l=='b') {
		*(Buffer++)=kint;
		//send(*Buffer);
		send(kint);}
		}
	}
//send(kint);
//showDateTime();
//if (kint==(uint8_t)0xFF) PORTD |=(1<<4);
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
	// initialize TWI Bit Rate Prescaler
	TWSR = 0;
	// initialize TWI Bit Rate
	TWBR = 0x0C;
	USART_vInit();
	//visus nustatyti isejimais ir tie kas nenaudojami
  DDRC |= ( (1<<0)|(1<<1)|(1<<2));
  DDRC &= ~(1<<3);
	DDRD |= ( (1<<6)|(1<<7) );
	DDRD &= ~( (1<<2)|(1<<3)|(1<<4)|(1<<5) ); //input
	DDRB |= ( (1<<0)|(1<<1)|(1<<2)|(1<<3)|(1<<4) );
	PORTD |= 0x3C; //input pull-up resistor on
	//PORTB |= 0x10; //minuso zenklas nesviecia

  //nustatymai del ADC, tai yra analog to digital converter
  ADMUX = ((1<<REFS0)|(0<<REFS0)|(0<<ADLAR)|(0<<MUX3)|(0<<MUX2)|(1<<MUX1)|(1<<MUX0)); //MUX3-0 nustatom kad skaitysim is ADC3
  ADCSRA = (1<<ADEN)|(0<<ADIE)|(1<<ADPS2)|(1<<ADPS1)|(0<<ADPS0); //Accuracy is best between 50 and 200 Khz clock frequency.
                   //^^ interupto valdymas
	//interruput INT0
	GICR |= (1<<INT0);
	MCUCR &= ~( (1<<ISC01)|(1<<ISC00) );
	sei();
	asm volatile ("nop");
	/*ReadAll((uint8_t *) &tdClock, 0, 8);
	ReadAll((uint8_t *) &oStart, 8, 11);*/
	ReadTime();
	ReadDate();
	ReadOption();
	if((oStart.issaugota&0x01) == 0x00){
		NustatytiDefault();
	}
	// Repeat indefinitely
	for(;;)
	{
	  ReadTime();
		if (oStart.tipas == 0x01) {				//startas realiu laiku
		  DisplayTime();
      garsas();
    } else if ((oStart.tipas == 0x02) || (oStart.tipas == 0x03)) {   //startas nuo 00:00:00  //startas nuo +x:xx:xx
      atimtiLaikus();
      garsas();
    } else if (oStart.tipas == 0x04){   //startas nuo -x:xx:xx
      if (atimtiLaikus() == 0){
        PORTB &= ~(1<<4);
        garsas();
      } else {
        PORTB |= (1<<4);
      }
    } else if (oStart.tipas == 0x05){  //laikrodis
			DisplayTime();
		}
		//skaicius(0x00, 0x00);
		//send(0xFF);
		//_delay_ms(400);
	}
}
