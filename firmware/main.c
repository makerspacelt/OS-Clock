#include <avr/interrupt.h>
#include <util/delay.h>
#include <avr/io.h>
#include <util/twi.h>

#define NUMBER_0 0x77
#define NUMBER_1 0x24
#define NUMBER_2 0x3B
#define NUMBER_3 0x3E
#define NUMBER_4 0x6C
#define NUMBER_5 0x5E
#define NUMBER_6 0x5F
#define NUMBER_7 0x34
#define NUMBER_8 0x7F
#define NUMBER_9 0x7E

#define FALSE 0
#define TRUE 1

#define PRESSED_UP      0x10    // sv3 up       - atmega pin 6   PD4
#define PRESSED_DOWN    0x04    // sv1 down     - atmega pin 4   PD2 INT0
#define PRESSED_ENTER   0x08    // sv2 left     - atmega pin 5   PD3 INT1
#define PRESSED_CANCEL  0x20    // sv4 right    - atmega pin 11  PD5

#define BUZZER_LONG     PD6     // PD6
#define BUZZER_SHORT    PD7     // PD7

//volatile uint8_t kint, l = ' ', kurisec = 59;
//volatile uint8_t buff[8], ilg;
//volatile uint8_t *Buffer = buff;

#define STATUS_REAL_TIME        0x00
#define STATUS_CONFIG_MENU      0x01
#define STATUS_REAL_TIME_START  0x02
#define STATUS_ZERO_TIME_START  0x03
#define STATUS_PLUS_TIME_START  0x04
#define STATUS_MINUS_TIME_START 0x05

#define CONFIG_START    0x01
#define CONFIG_BEEPS    0x02
#define CONFIG_TIME     0x03

volatile uint8_t oldStatus, lastSecond = 0x00;

typedef struct {
    uint8_t seconds;
    uint8_t minutes;
    uint8_t hours;
    uint8_t weekDay;
    uint8_t day;
    uint8_t month;
    uint8_t year;
    uint8_t control;
} ds1307Reg_t;

typedef struct {
    //uint8_t issaugota;      //from 0x08
    uint8_t status;         //0x09

    uint8_t longCount;      //0x0A
    uint8_t shortCount;     //0x0B

    uint8_t long1;          //0x0C
    uint8_t long2;
    uint8_t long3;
    uint8_t long4;

    uint8_t short1;         //0x10
    uint8_t short2;
    uint8_t short3;
    uint8_t short4;
    uint8_t short5;
    uint8_t short6;
    uint8_t short7;
    uint8_t short8;

    uint8_t minus;          //0x18 start from minus or plus
    uint8_t seconds;        //0x19
    uint8_t minutes;
    uint8_t hours;

    uint8_t diff_sec;       //x1C
    uint8_t diff_min;
    uint8_t diff_hour;

} setting_t;

volatile ds1307Reg_t time;
volatile setting_t deviceSetting;

void spiMasterInit(void)
{
    /** The Serial Peripheral Interface (SPI) allows high-speed synchronous data transfer between the
    ATmega8 and peripheral devices or between several AVR devices. */
    /* Enable SPI, Master, set clock rate fck/16 */
    SPCR = (1<<SPE)|(1<<MSTR)|(1<<SPR0);
}

void init(void)
{
    // set all pins as output
    DDRB = 0xFF; //sets all bits of port B for output
    DDRC = 0xFF;
    DDRD = 0xFF;
    PORTB = 0x00; //sets a output of port B to LOW
    PORTC = 0x00;
    PORTD = 0x00;

    // set buttons as input
    DDRD &= ~((1<<PD2)|(1<<PD3)|(1<<PD4)|(1<<PD5));
    PORTD |= (1<<PD2)|(1<<PD3)|(1<<PD4)|(1<<PD5);
    // enable pull up resistors
    SFIOR &= ~(1<<PUD);

    // Enable Serial Peripheral Interface (SPI)
    spiMasterInit();

    // Enable external button interrupt
    GICR |= (1<<INT1);
    MCUCR &= ~((1<<ISC11)|(0<<ISC10));
    sei();
    asm volatile ("nop");

    // Relax after exhausting work
    asm volatile ("nop");
}

void setFactorySetting(void)
{
    deviceSetting.status = STATUS_REAL_TIME_START;
    deviceSetting.longCount = 0x01;
    deviceSetting.shortCount = 0x02;
    deviceSetting.long1 = 0x00;
    deviceSetting.long2 = 0;
    deviceSetting.long3 = 0;
    deviceSetting.long4 = 0;
    deviceSetting.short1 = 0x58;
    deviceSetting.short2 = 0x59;
    deviceSetting.short3 = 0;
    deviceSetting.short4 = 0;
    deviceSetting.short5 = 0;
    deviceSetting.short6 = 0;
    deviceSetting.short7 = 0;
    deviceSetting.short8 = 0;
    deviceSetting.minus = 0;
    deviceSetting.seconds = 0;
    deviceSetting.minutes = 0;
    deviceSetting.hours = 0;
    deviceSetting.diff_sec = 59;
    deviceSetting.diff_min = 59;
    deviceSetting.diff_hour = 1;
}

uint8_t prepareTwi(uint8_t mode)
{
    // 1 - read mode; 0 - write mode;
    // send START condition
    TWCR = (1<<TWINT)|(1<<TWSTA)|(1<<TWEN);
    // wait for start condition transmitted
    while (!(TWCR & (1<<TWINT)));
    // check status code
    if (TW_STATUS != TW_START) {
        // error
        return FALSE;
    };
    // set device address and twi mode SLA+R = 1 or SLA+W = 0
    TWDR = 0B11010000 | mode;
    // transmit SLA
    TWCR = (1<<TWINT) | (1<<TWEN);
    // wait for SLA transmitted
    while (!(TWCR & (1<<TWINT)));
    // check status code
    if (mode == 0 && TW_STATUS == TW_MT_SLA_ACK){
        return TRUE;
    }
    if (mode == 1 && TW_STATUS == TW_MR_SLA_ACK){
        return TRUE;
    }
    return FALSE;
}

void generateStop(void)
{
    // generate a STOP condition
    TWCR = (1<<TWINT)|(1<<TWSTO)|(1<<TWEN);
    // wait for STOP generated
    asm volatile ("nop");
}

uint8_t twiRead(uint8_t* data, uint8_t count)
{
    if (!prepareTwi(1)) {
        return FALSE;
    }
    do {
        // set up to receive next data byte
        TWCR = (1<<TWINT) | (1<<TWEN) | ((count != 1) ? (1<<TWEA) : 0);
        // wait for data byte
        while (!(TWCR & (1<<TWINT)));
        // check for timeout and check status
        if (TW_STATUS != ( (count != 1) ? TW_MR_DATA_ACK : TW_MR_DATA_NACK))
        {
            return FALSE;
        }
        // read data byte
        *(data++) = TWDR;
    } while ( --count != 0 );

    generateStop();
    return TRUE;
}

uint8_t twiWrite(uint8_t *data, uint8_t count)
{
    if (!prepareTwi(0)) {
        return FALSE;
    }
    do {
        // send data byte
        TWDR = *data++;
        TWCR = (1<<TWINT) | (1<<TWEN);
        // wait for data byte transmitted
        while (!(TWCR & (1<<TWINT)));
        if ( TW_STATUS != TW_MT_DATA_ACK ) {
            return FALSE;
        }
    } while ( --count != 0 );
    generateStop( );
    return TRUE;
}

uint8_t readTime(void)
{
    // read time from RTC
    uint8_t tmp[3];
    uint8_t reg = 0;
    // write DS1307 from which register will begin read
    if (!twiWrite(&reg, 1)) {
        return FALSE;
    }
    // read DS1307 registers
    if (!twiRead(tmp, 3)) {
        return FALSE;
    }
    time.seconds = tmp[0] & 0x7F;
    time.minutes = tmp[1];
    time.hours = tmp[2] & 0x3F;

    return TRUE;
}

void spiMasterTransmit(uint8_t cData)
{
    switch (cData)
    {
        case 0x00:
            cData = NUMBER_0;
            break;
        case 0x01:
            cData = NUMBER_1;
            break;
        case 0x02:
            cData = NUMBER_2;
            break;
        case 0x03:
            cData = NUMBER_3;
            break;
        case 0x04:
            cData = NUMBER_4;
            break;
        case 0x05:
            cData = NUMBER_5;
            break;
        case 0x06:
            cData = NUMBER_6;
            break;
        case 0x07:
            cData = NUMBER_7;
            break;
        case 0x08:
            cData = NUMBER_8;
            break;
        case 0x09:
            cData = NUMBER_9;
            break;
        default:
            cData = 0x00;
    }
    /* Start transmission */
    SPDR = cData | 0x80;
    /* Wait for transmission complete */
    while(!(SPSR & (1<<SPIF)))
        ;
}

void renewDisplay(void)
{
    // display data
    PORTB |= (1<<PB2);
    PORTB &= ~(1<<PB2);
}

void clearDisplay(void)
{
    spiMasterTransmit(0x10);
    spiMasterTransmit(0x10);
    spiMasterTransmit(0x10);
    spiMasterTransmit(0x10);
    spiMasterTransmit(0x10);
    spiMasterTransmit(0x10);
    renewDisplay();
}

void displayTime(void)
{
    // send data for display
    spiMasterTransmit(time.hours >> 4);
    spiMasterTransmit(time.hours & 0x0F);
    spiMasterTransmit(time.minutes >> 4);
    spiMasterTransmit(time.minutes & 0x0F);
    spiMasterTransmit(time.seconds >> 4);
    spiMasterTransmit(time.seconds & 0x0F);
    renewDisplay();
}

void displayChar(uint8_t data)
{
    if ((data >> 4) != 0x00) {
        spiMasterTransmit(data >> 4);
    }
    spiMasterTransmit(data & 0x0F);
    renewDisplay();
}

uint8_t getPressedButton(void)
{
    uint8_t pressedButton;
    // read pressed button
    while((PIND & 0x3C) != 0x3C) {_delay_ms(10);}
    while((PIND & 0x3C) == 0x3C) {;}
    _delay_ms(10);
    pressedButton = (0x3C & ~PIND);
    while((PIND & 0x3C) != 0x3C) {;}

    return pressedButton;
}

void configureDevice(void)
{
    uint8_t pressedButton, configStatus = CONFIG_START, minValue = 0x01, maxValue = 0x03;
    while (configStatus)
    {
        clearDisplay();
        //display status
        displayChar(configStatus);
        pressedButton = getPressedButton();

        switch (pressedButton)
        {
            case PRESSED_UP:
                configStatus++;
                if (configStatus > maxValue) {
                    configStatus = minValue;
                }
                break;
            case PRESSED_DOWN:
                configStatus--;
                if (configStatus < minValue) {
                    configStatus = maxValue;
                }
                break;
            case PRESSED_ENTER:
                if (configStatus == CONFIG_START) {
                    minValue = (CONFIG_START << 4) | 0x01;
                    maxValue = (CONFIG_START << 4) | 0x05;
                }
                if (configStatus == CONFIG_BEEPS) {
                    minValue = (CONFIG_BEEPS << 4) | 0x01;
                    maxValue = (CONFIG_BEEPS << 4)| 0x05;
                }
                if (configStatus == CONFIG_TIME) {
                    minValue = (CONFIG_TIME << 4) | 0x01;
                    maxValue = (CONFIG_TIME << 4) | 0x03;
                }
                if (!(configStatus & 0xF0)) {
                    configStatus = (configStatus << 4) | 0x01;
                }
                break;
            case PRESSED_CANCEL:
                configStatus = configStatus >> 4;
                minValue = 0x01;
                maxValue = 0x03;
                break;
        }
    }
    deviceSetting.status = oldStatus;
    sei();
}

void makeBeep(void){
    uint8_t *tmp;
    tmp = (uint8_t *) &deviceSetting.short1;
    for(uint8_t i = 0; i < deviceSetting.shortCount; i++){
        if((time.seconds == *tmp) && (lastSecond != time.seconds)){
            PORTD |= (1<<BUZZER_SHORT);
            _delay_ms(150);
            PORTD &= ~(1<<BUZZER_SHORT);
            lastSecond = time.seconds;
            break;
        }
        tmp++;
    }
    tmp = (uint8_t *) &deviceSetting.long1;
    for(uint8_t i = 0; i < deviceSetting.longCount; i++){
        if((time.seconds == *tmp) && (lastSecond != time.seconds)){
            PORTD |= (1<<BUZZER_LONG);
            _delay_ms(350);
            PORTD &= ~(1<<BUZZER_LONG);
            lastSecond = time.seconds;
            break;
        }
        tmp++;
    }
}

ISR(INT1_vect)
{
    cli();
    if (deviceSetting.status != STATUS_CONFIG_MENU) {
        oldStatus = deviceSetting.status;
    }
    deviceSetting.status = STATUS_CONFIG_MENU;
}

int main(void)
{
    init();
    clearDisplay();

    // Set factory settings
    setFactorySetting();

    // Read settings from memory

    // Repeat indefinitely
    for(;;)
    {
        if (readTime()) {
            switch (deviceSetting.status)
            {
                case STATUS_REAL_TIME_START:
                    makeBeep();
                case STATUS_REAL_TIME:
                    displayTime();
                    break;
                case STATUS_CONFIG_MENU:
                    cli();
                    configureDevice();
                    break;
            }
        }
    }
}
