#include <avr/interrupt.h>
#include <util/delay.h>
#include <avr/io.h>
#include <util/twi.h>

//  --          0x10
// |  |     0x40    0x20
//  --          0x08
// |  |     0x01    0x04
//  --          0x02

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
#define CHAR_MINUS 0xFE
#define CHAR_SPACE 0xFF
#define CHAR_A 0x7D
#define CHAR_B 0x4F
#define CHAR_C 0x53
#define CHAR_D 0x2F
#define CHAR_E 0x5B
#define CHAR_F 0x59
#define CHAR_G 0x57
#define CHAR_H 0x6D
#define CHAR_I 0x41
#define CHAR_Y 0x6C
#define CHAR_J 0x27
#define CHAR_K 0x6B
#define CHAR_L 0x43
#define CHAR_M 0x15
#define CHAR_N 0x75
#define CHAR_O 0x77
#define CHAR_P 0x79
#define CHAR_R 0x51
#define CHAR_S 0x5E
#define CHAR_T 0x4B
#define CHAR_U 0x67
#define CHAR_V 0x07
#define CHAR_Z 0x33



#define FALSE 0
#define TRUE 1

#define PRESSED_UP      0x10    // sv3 up       - atmega pin 6   PD4
#define PRESSED_DOWN    0x04    // sv1 down     - atmega pin 4   PD2 INT0
#define PRESSED_ENTER   0x08    // sv2 left     - atmega pin 5   PD3 INT1
#define PRESSED_CANCEL  0x20    // sv4 right    - atmega pin 11  PD5

#define LED_GREEN   PC3    // atmega pin 26
#define LED_YELLOW  PC2    // atmega pin 25
#define LED_RED     PC1    // atmega pin 24

#define BATTERY_YELLOW_LEVEL  30 // 60 70
#define BATTERY_RED_LEVEL     18 // 40 58

#define BUZZER_LONG     PD6     // PD6
#define BUZZER_SHORT    PD7     // PD7

#define STATUS_REAL_TIME        0x00
#define STATUS_CONFIG_MENU      0x01
#define STATUS_REAL_TIME_START  0x02
#define STATUS_ZERO_TIME_START  0x03
#define STATUS_PLUS_TIME_START  0x04
#define STATUS_MINUS_TIME_START 0x05
#define STATUS_LOW_BATTERY      0xFF

#define CONFIG_EXIT              0x00
#define CONFIG_START             0x01
#define CONFIG_START_REAL_TIME   0x11
#define CONFIG_START_ZERO_TIME   0x12
#define CONFIG_START_PLUS_TIME   0x13
#define CONFIG_START_MINUS_TIME  0x14
#define CONFIG_SHOW_REAL_TIME    0x15
#define CONFIG_BEEPS             0x02
#define CONFIG_LONG_BEEPS_COUNT  0x21
#define CONFIG_SHORT_BEEPS_COUNT 0x22
#define CONFIG_LONG_BEEPS        0x23
#define CONFIG_SHORT_BEEPS       0x24
#define CONFIG_TIME              0x03
#define CONFIG_DELTA_TIME        0x31
#define CONFIG_REAL_TIME         0x32
#define CONFIG_BATTERY_LEVEL     0x33
#define CONFIG_RESET_FACTORY     0x34

volatile uint8_t oldStatus,
    lastBeepSecond = 0xFF,
    lastBatterySecond = 0xFF,
    isMinus = FALSE,
    showTime = FALSE;

typedef struct {
    uint8_t seconds;
    uint8_t minutes;
    uint8_t hours;
//    uint8_t weekDay;
//    uint8_t day;
//    uint8_t month;
//    uint8_t year;
//    uint8_t control;
} ds1307Reg_t;

#define DATA_LEN 25
#define SETTING_ADDRESS 0x08

typedef struct {
    uint8_t firstAddress;   //value are not saved, only for saving addressing
    uint8_t saved;          //from 0x08
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

    uint32_t zeroTime;     //0x18

    uint32_t deltaTime;    //0x1C

} setting_t;

volatile ds1307Reg_t time;
volatile setting_t deviceSetting;

void initSpiMaster(void)
{
    /** The Serial Peripheral Interface (SPI) allows high-speed synchronous data transfer between the
    ATmega8 and peripheral devices or between several AVR devices. */
    /* Enable SPI, Master, set clock rate fck/128 */
    SPCR = (1<<SPE)|(1<<MSTR)|(1<<SPR0)|(1<<SPR1);
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
    MCUCR &= ~(1<<PUD);
}

void initFeatures(void)
{
    //Configure ADC
    DDRC &= ~(1<<PC0);
    ADMUX = (0<<REFS1)|(1<<REFS0)|(0<<ADLAR)|(0<<MUX3)|(0<<MUX2)|(0<<MUX1)|(0<<MUX0); //ADC0 is connected to battery
    ADCSRA = (1<<ADEN)|(0<<ADATE)|(0<<ADIF)|(0<<ADIE)|(1<<ADPS2)|(1<<ADPS1)|(0<<ADPS0); //Accuracy is best between 50 and 200 Khz clock frequency.

    // Enable external button interrupt
    EIMSK |= (1<<INT1);
    EICRA &= ~((1<<ISC11)|(1<<ISC10));
    sei();
    asm volatile ("nop");

    //Configure TWI speed
    TWBR = (uint8_t)85;
}

void initUSART(void)
{
    // Set baud rate 38400
    UBRR0H = (uint8_t)(29>>8);
    UBRR0L = (uint8_t)29;
    // Enable receiver and transmitter and Interup on RXC
    UCSR0B = (1<<RXEN0)|(1<<TXEN0)|(1<<RXCIE0);
    // Set frame format: 8data, 2stop bit
    UCSR0C = (1<<USBS0)|(3<<UCSZ00);
    asm volatile ("nop");
}

void setFactorySetting(void)
{
    deviceSetting.firstAddress = SETTING_ADDRESS;   //0x08
    deviceSetting.saved = FALSE;                    //0x00
    deviceSetting.status = STATUS_REAL_TIME;        //0x00
    deviceSetting.longCount = 0x01;
    deviceSetting.shortCount = 0x02;
    deviceSetting.long1 = 0x00;
    deviceSetting.long2 = 0x15;
    deviceSetting.long3 = 0x30;
    deviceSetting.long4 = 0x45;
    deviceSetting.short1 = 0x58;
    deviceSetting.short2 = 0x59;
    deviceSetting.short3 = 0x13;
    deviceSetting.short4 = 0x14;
    deviceSetting.short5 = 0x28;
    deviceSetting.short6 = 0x29;
    deviceSetting.short7 = 0x43;
    deviceSetting.short8 = 0x44;
    deviceSetting.zeroTime = 0;
    deviceSetting.deltaTime = 900;
}

void sendBuffer(uint8_t *idata, uint8_t kiek)
{
    for (uint8_t i = 1;i <= kiek;i++){
        while(!(UCSR0A&(1<<UDRE0)));
        UDR0=*idata++;
    }
    while(!(UCSR0A&(1<<UDRE0)));
    UDR0=13;
    while(!(UCSR0A&(1<<UDRE0)));
    UDR0=10;
}

void sendLetter(uint8_t data)
{
    while(!(UCSR0A&(1<<UDRE0)));
    UDR0 = data;
    while(!(UCSR0A&(1<<UDRE0)));
    UDR0=13;
    while(!(UCSR0A&(1<<UDRE0)));
    UDR0=10;
}


uint8_t prepareTwi(uint8_t mode)
{
    // 1 - read mode; 0 - write mode;
    // send START condition
    TWCR = (1<<TWINT)|(1<<TWSTA)|(1<<TWEN);
    // wait for start condition transmitted
    while (!(TWCR & (1<<TWINT)));
    // check status code
    if (TW_STATUS != TW_START && TW_STATUS != TW_REP_START) {
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
    if ((mode == 0 && TW_STATUS == TW_MT_SLA_ACK) || (mode == 1 && TW_STATUS == TW_MR_SLA_ACK)){
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
    if (!prepareTwi(TW_READ)) {
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
    if (!prepareTwi(TW_WRITE)) {
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

void writeTime(void)
{
    uint8_t tmp[4];
    tmp[0] = 0x00;
    tmp[1] = time.seconds;
    tmp[2] = time.minutes;
    tmp[3] = time.hours;
    twiWrite(tmp, 4);
}

void readSettings(void)
{
    uint8_t reg = SETTING_ADDRESS;
    while(!twiWrite(&reg, 1)) {}
    twiRead((uint8_t *) &deviceSetting.saved, DATA_LEN);
}

void saveSettings(void)
{
    deviceSetting.saved = TRUE;
    deviceSetting.firstAddress = SETTING_ADDRESS;
    twiWrite((uint8_t *) &deviceSetting, DATA_LEN + 1);
}

void spiMasterTransmit(uint8_t cData)
{
    switch (cData)
    {
        case 0x00:
        case 0x30:
            cData = NUMBER_0;
            break;
        case 0x01:
        case 0x31:
            cData = NUMBER_1;
            break;
        case 0x02:
        case 0x32:
            cData = NUMBER_2;
            break;
        case 0x03:
        case 0x33:
            cData = NUMBER_3;
            break;
        case 0x04:
        case 0x34:
            cData = NUMBER_4;
            break;
        case 0x05:
        case 0x35:
            cData = NUMBER_5;
            break;
        case 0x06:
        case 0x36:
            cData = NUMBER_6;
            break;
        case 0x07:
        case 0x37:
            cData = NUMBER_7;
            break;
        case 0x08:
        case 0x38:
            cData = NUMBER_8;
            break;
        case 0x09:
        case 0x39:
            cData = NUMBER_9;
            break;
        case 0x0A:
        case 0x41:
            cData = CHAR_A;
            break;
        case 0x0B:
        case 0x42:
            cData = CHAR_B;
            break;
        case 0x0C:
        case 0x43:
            cData = CHAR_C;
            break;
        case 0x0D:
        case 0x44:
            cData = CHAR_D;
            break;
        case 0x0E:
        case 0x45:
            cData = CHAR_E;
            break;
        case 0x0F:
        case 0x46:
            cData = CHAR_F;
            break;
        case 0x47:
            cData = CHAR_G;
            break;
        case 0x48:
            cData = CHAR_H;
            break;
        case 0x49:
            cData = CHAR_I;
            break;
        case 0x4A:
            cData = CHAR_J;
            break;
        case 0x4B:
            cData = CHAR_K;
            break;
        case 0x4C:
            cData = CHAR_L;
            break;
        case 0x4D:
            cData = CHAR_M;
            break;
        case 0x4E:
            cData = CHAR_N;
            break;
        case 0x4F:
            cData = CHAR_O;
            break;
        case 0x50:
            cData = CHAR_P;
            break;
        case 0x52:
            cData = CHAR_R;
            break;
        case 0x53:
            cData = CHAR_S;
            break;
        case 0x54:
            cData = CHAR_T;
            break;
        case 0x55:
            cData = CHAR_U;
            break;
        case 0x56:
            cData = CHAR_V;
            break;
        case 0x59:
            cData = CHAR_Y;
            break;
        case 0x5A:
            cData = CHAR_Z;
            break;
        case CHAR_MINUS:
        case 0x2D:
            cData = 0x08;
            break;
        case CHAR_SPACE:
        case 0x20:
            cData = 0x00;
            break;
        default :
            cData = 0x00;
    }
    if (showTime) {
        cData |= 0x80;
    }
    /* Start transmission */
    SPDR = cData;
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
    uint8_t tmp;
    tmp = showTime;
    showTime = FALSE;
    spiMasterTransmit(CHAR_SPACE);
    spiMasterTransmit(CHAR_SPACE);
    spiMasterTransmit(CHAR_SPACE);
    spiMasterTransmit(CHAR_SPACE);
    spiMasterTransmit(CHAR_SPACE);
    spiMasterTransmit(CHAR_SPACE);
    spiMasterTransmit(CHAR_SPACE);
    spiMasterTransmit(CHAR_SPACE);
    renewDisplay();
    showTime = tmp;
}

void displayTime(void)
{
    showTime = TRUE;
    // send data for display
    if ((time.hours & 0xF0) == 0x00) {
        if (isMinus == TRUE) {
            spiMasterTransmit(CHAR_MINUS);
        } else {
            spiMasterTransmit(CHAR_SPACE);
        }
        if ((time.hours & 0x0F) == 0x00) {
            spiMasterTransmit(CHAR_SPACE);
        } else {
            spiMasterTransmit(time.hours & 0x0F);
        }
    } else {
        spiMasterTransmit(time.hours >> 4);
        spiMasterTransmit(time.hours & 0x0F);
    }
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

void sendFullChar(uint8_t data)
{
    if (data == CHAR_SPACE) {
        spiMasterTransmit(CHAR_SPACE);
        spiMasterTransmit(CHAR_SPACE);
    } else {
        spiMasterTransmit(data >> 4);
        spiMasterTransmit(data & 0x0F);
    }
}

//void displayData(uint8_t *data, uint8_t count)
//{
//    uint8_t tmp;
//    do {
//        tmp = *data++;
//        displayFullChar(tmp);
//        _delay_ms(3000);
//    } while ( --count != 0 );
//}

uint8_t getPressedButton(void)
{
    uint8_t pressedButton;
    // read pressed button
    while((PIND & 0x3C) != 0x3C) {_delay_ms(30);}
    while((PIND & 0x3C) == 0x3C) {;}
    _delay_ms(30);
    pressedButton = (0x3C & ~PIND);
    while((PIND & 0x3C) != 0x3C) {;}

    return pressedButton;
}

uint8_t getNonBlockingPressedButton(void)
{
    uint8_t pressedButton = 0;
    if((PIND & 0x3C) != 0x3C) {
        _delay_ms(30);
        pressedButton = (0x3C & ~PIND);
        while((PIND & 0x3C) != 0x3C) {;}
    }

    return pressedButton;
}

uint32_t getTimeStamp(void)
{
    uint32_t full = 0, tmp;
    tmp = (time.seconds >> 4) * 10;
    full += tmp;
    full += (time.seconds & 0x0F);
    tmp = (time.minutes >> 4) * 10;
    tmp *= 60;
    full += tmp;
    full += (time.minutes & 0x0F) * 60;
    tmp = (time.hours >> 4);
    tmp *= 10;
    tmp *= 60;
    tmp *= 60;
    full += tmp;
    tmp = (time.hours & 0x0F);
    tmp *= 60;
    tmp *= 60;
    full += tmp;

    return full;
}

uint8_t configBeepsCount(int8_t minValue, int8_t maxValue, int8_t value)
{
    uint8_t pressedButton, configStatus = CONFIG_START, oldValue = value;
    while (configStatus != CONFIG_EXIT)
    {
        clearDisplay();
        displayChar(value);
        pressedButton = getPressedButton();

        switch (pressedButton)
        {
            case PRESSED_UP:
                value++;
                if (value > maxValue) {
                    value = minValue;
                }
                break;
            case PRESSED_DOWN:
                value--;
                if (value < minValue) {
                    value = maxValue;
                }
                break;
            case PRESSED_CANCEL:
                value = oldValue; // exit will be assign on ENTER
            case PRESSED_ENTER:
                configStatus = CONFIG_EXIT;
                break;
        }
    }

    return value;
}

void configBeeps(uint8_t minValue, uint8_t maxValue, uint8_t count, uint8_t *place)
{
    uint8_t pressedButton, configStatus = CONFIG_START, hasChange = FALSE, step = 1;
    int8_t value = *place;
    while (configStatus != CONFIG_EXIT)
    {
        clearDisplay();
        displayChar(step);
        spiMasterTransmit(CHAR_SPACE);
        spiMasterTransmit(CHAR_SPACE);
        displayChar(value);
        pressedButton = getPressedButton();

        switch (pressedButton)
        {
            case PRESSED_UP:
                value++;
                if (value > maxValue) {
                    value = minValue;
                } else {
                    if ((value & 0x0F) > 0x09) {
                        value = (value & 0xF0) + 0x10;
                    }
                }
                break;
            case PRESSED_DOWN:
                value--;
                if (value < minValue) {
                    value = maxValue;
                } else {
                    if ((value & 0x0F) > 0x09) {
                        value = (value & 0xF0) + 0x09;
                    }
                }
                break;
            case PRESSED_CANCEL:
                configStatus = CONFIG_EXIT;
                hasChange = FALSE;
                break;
            case PRESSED_ENTER:
                if (*place != value) {
                    *place = value;
                    hasChange = TRUE;
                }
                place++;
                value = *place;
                if (step >= count) {
                    configStatus = CONFIG_EXIT;
                }
                step++;
                break;
        }
    }
    if (hasChange == TRUE ) {
        saveSettings();
    }
    readSettings();
}

void configTime(int8_t cache[3][2])
{
    uint8_t configStatus = CONFIG_START, pressedButton, pause = 15, value;
    int8_t place = 2;

    showTime = TRUE;
    value = cache[2][0];
    while (configStatus != CONFIG_EXIT) {
        _delay_ms(30);
        clearDisplay();
        for(uint8_t i = 0; i < 3; i++){
            if (i == place) {
                sendFullChar(value);
                if (pause == 0) {
                    pause = 15;
                    if (value == CHAR_SPACE) {
                        value = cache[i][0];
                    } else {
                        value = CHAR_SPACE;
                    }
                }
            } else {
                sendFullChar(cache[i][0]);
            }

        }
        renewDisplay();
        pause--;

        pressedButton = getNonBlockingPressedButton();
        switch (pressedButton)
        {
            case PRESSED_UP:
                cache[place][0]++;
                if (cache[place][0] > cache[place][1]) {
                    cache[place][0] = 0;
                } else {
                    if ((cache[place][0] & 0x0F) > 0x09) {
                        cache[place][0] = (cache[place][0] & 0xF0) + 0x10;
                    }
                }
                value = cache[place][0];
                pause = 15;
                break;
            case PRESSED_DOWN:
                cache[place][0]--;
                if (cache[place][0] < 0) {
                    cache[place][0] = cache[place][1];
                } else {
                    if ((cache[place][0] & 0x0F) > 0x09) {
                        cache[place][0] = (cache[place][0] & 0xF0) + 0x09;
                    }
                }
                value = cache[place][0];
                pause = 15;
                break;
            case PRESSED_CANCEL:
                place++;
                value = cache[place][0];
                pause = 15;
                if (place > 2) {
                    configStatus = CONFIG_EXIT;
                }
                break;
            case PRESSED_ENTER:
                place--;
                value = cache[place][0];
                pause = 15;
                if (place < 0) {
                    configStatus = CONFIG_EXIT;
                }
                break;
        }
    }
    showTime = FALSE;
}

void configDeltaTime(void)
{
    uint8_t tmp;
    int8_t cache[3][2] = {{0, 0x23}, {0, 0x59}, {0, 0x59}};
    uint32_t result;

    tmp = deviceSetting.deltaTime / 3600;
    cache[0][0] = ((tmp / 10) << 4) + (tmp % 10);
    tmp = (deviceSetting.deltaTime % 3600) / 60;
    cache[1][0] = ((tmp / 10) << 4) + (tmp % 10);
    tmp = deviceSetting.deltaTime % 60;
    cache[2][0] = ((tmp / 10) << 4) + (tmp % 10);

    configTime(cache);

    result = ((uint32_t)((cache[0][0] >> 4) * 10 + (cache[0][0] & 0x0F))) * 3600 +
             ((uint32_t)((cache[1][0] >> 4) * 10 + (cache[1][0] & 0x0F))) * 60 +
             ((uint32_t)((cache[2][0] >> 4) * 10 + (cache[2][0] & 0x0F)));
    if (result != deviceSetting.deltaTime) {
        deviceSetting.deltaTime = result;
        saveSettings();
    }
    readSettings();
}

void configRealTime(void)
{
    int8_t cache[3][2] = {{0, 0x23}, {0, 0x59}, {0, 0x59}};
    while (!readTime()) {
        _delay_ms(30);
    }
    cache[0][0] = time.hours;
    cache[1][0] = time.minutes;
    cache[2][0] = time.seconds;
    configTime(cache);

    if (cache[0][0] != time.hours ||
        cache[1][0] != time.minutes ||
        cache[2][0] != time.seconds) {
        time.hours = cache[0][0];
        time.minutes = cache[1][0];
        time.seconds = cache[2][0];
        writeTime();
    }
}

uint32_t getRawBatteryLevel(void)
{
    uint8_t low, high;
    uint32_t res = 0;
    ADCSRA &= ~(1<<ADIF);
    ADCSRA |= (1 << ADSC);
    while ((ADCSRA & 0x10) != 0x10) { ; }
    low = ADCL;
    high = ADCH;

    res = high;
    res = res << 8;
    res += low;

    return res;
}

void showBatteryLevel(uint32_t number)
{
    clearDisplay();
    displayChar(number / 1000);
    displayChar(number / 100 % 10);
    displayChar(number / 10 % 10);
    displayChar(number % 10);
    renewDisplay();
}

uint8_t getBatteryLevel(void)
{
    uint8_t freq = 1;
    uint32_t res = 0, sum = 0;

    while (freq % 100 != 0) {
        res = getRawBatteryLevel();
        res = ((75 * res) / 100) - 545;
        if (res > 100) {
            res = 100;
        } else if (res < 0) {
            res = 0;
        }
        sum += res;
        freq++;
    }
    sum = sum / 100;

    return sum;
}

void batteryLevelMenu(void)
{
    while (TRUE) {
        if (0 != getNonBlockingPressedButton()) {
            return;
        }
        showBatteryLevel(getBatteryLevel());
        _delay_ms(1000);
    }
}

void displayBatteryLevelInTrafficLight(void)
{
    uint8_t level;
    level = getBatteryLevel();
    PORTC &= ~(1<<LED_GREEN);
    PORTC &= ~(1<<LED_YELLOW);
    PORTC &= ~(1<<LED_RED);
    if (level < BATTERY_RED_LEVEL) {
       PORTC |= (1<<LED_RED);
    } else {
        if (level < BATTERY_YELLOW_LEVEL) {
            PORTC |= (1<<LED_YELLOW);
        } else {
            PORTC |= (1<<LED_GREEN);
        }
    }
}

void renewBatteryLevelInTrafficLight(void)
{
    if((time.seconds == 2) && (lastBatterySecond != time.seconds)) {
        displayBatteryLevelInTrafficLight();
    }
    lastBatterySecond = time.seconds;
}

void isBatteryLow(void)
{
    uint8_t level;
    level = getBatteryLevel();
    if (level < BATTERY_RED_LEVEL) {
        deviceSetting.status = STATUS_LOW_BATTERY;
    }
}

void makeLowBatterySound(void)
{
    uint8_t j;
    if ((time.seconds % 2 == 0 ) && (lastBeepSecond != time.seconds)) {
        showTime = TRUE;
        sendFullChar(CHAR_SPACE);
        sendFullChar(CHAR_SPACE);
        sendFullChar(CHAR_SPACE);
        sendFullChar(CHAR_SPACE);
        sendFullChar(CHAR_SPACE);
        sendFullChar(CHAR_SPACE);
        sendFullChar(CHAR_SPACE);
        renewDisplay();

        for (j = 1; j <= 5; j++) {
            PORTD |= (1<<BUZZER_SHORT);
            _delay_ms(50);
            PORTD &= ~(1<<BUZZER_SHORT);
            _delay_ms(50);
        }
        for (j = 1; j <= 2; j++) {
            PORTD |= (1<<BUZZER_SHORT);
            _delay_ms(80);
            PORTD &= ~(1<<BUZZER_SHORT);
            _delay_ms(80);
        }
    } else {
       clearDisplay();
    }
    lastBeepSecond = time.seconds;
}

void configureDevice(void)
{
    uint8_t pressedButton, configStatus = CONFIG_START, minValue = 0x01, maxValue = 0x03, value;
    char mark = 'z';
    deviceSetting.status = oldStatus;
    showTime = FALSE;
    while (configStatus != CONFIG_EXIT)
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
                switch (configStatus)
                {
                    case CONFIG_START: //0x01
                    case CONFIG_BEEPS: //0x02
                        minValue = (configStatus << 4) | 0x01;
                        maxValue = (configStatus << 4) | 0x05;
                        configStatus = minValue;
                        break;
                    case CONFIG_TIME: //0x03
                        minValue = (CONFIG_TIME << 4) | 0x01;
                        maxValue = (CONFIG_TIME << 4) | 0x04;
                        configStatus = minValue;
                        break;
                    case CONFIG_START_REAL_TIME: //0x11
                        deviceSetting.status = STATUS_REAL_TIME_START;
                        deviceSetting.zeroTime = 0;
                        saveSettings();
                        configStatus = CONFIG_EXIT;
                        break;
                    case CONFIG_START_ZERO_TIME: //0x12
                        if(readTime()){
                            deviceSetting.status = STATUS_ZERO_TIME_START;
                            deviceSetting.zeroTime = getTimeStamp();
                            saveSettings();
                            configStatus = CONFIG_EXIT;
                            sendBuffer((uint8_t *) &mark, 1);
                            sendBuffer((uint8_t *) &deviceSetting.zeroTime, 4);
                        }
                        break;
                    case CONFIG_START_PLUS_TIME: //0x13
                        if(readTime()){
                            deviceSetting.status = STATUS_PLUS_TIME_START;
                            deviceSetting.zeroTime = getTimeStamp();
                            if (deviceSetting.zeroTime < deviceSetting.deltaTime) {
                                deviceSetting.zeroTime = 86399 - deviceSetting.deltaTime + deviceSetting.zeroTime;
                            } else {
                                deviceSetting.zeroTime -= deviceSetting.deltaTime;
                            }
                            saveSettings();
                            oldStatus = STATUS_PLUS_TIME_START;
                            configStatus = CONFIG_EXIT;
                        }
                        break;
                    case CONFIG_START_MINUS_TIME: //0x14
                        if(readTime()){
                            deviceSetting.status = STATUS_MINUS_TIME_START;
                            deviceSetting.zeroTime = getTimeStamp() + deviceSetting.deltaTime;
                            if (deviceSetting.zeroTime >= 86399) {
                                deviceSetting.zeroTime -= 86399;
                            }
                            saveSettings();
                            oldStatus = STATUS_MINUS_TIME_START;
                            configStatus = CONFIG_EXIT;
                        }
                        break;
                    case CONFIG_SHOW_REAL_TIME: //0x15
                        deviceSetting.status = STATUS_REAL_TIME;
                        saveSettings();
                        oldStatus = STATUS_REAL_TIME;
                        configStatus = CONFIG_EXIT;
                        break;
                    case CONFIG_LONG_BEEPS_COUNT: //0x21
                        value = configBeepsCount(0, 4, deviceSetting.longCount);
                        if (value != deviceSetting.longCount) {
                            deviceSetting.longCount = value;
                            saveSettings();
                        }
                        break;
                    case CONFIG_SHORT_BEEPS_COUNT: //0x22
                        value = configBeepsCount(0, 8, deviceSetting.shortCount);
                        if (value != deviceSetting.shortCount) {
                            deviceSetting.shortCount = value;
                            saveSettings();
                        }
                        break;
                    case CONFIG_LONG_BEEPS: //0x23
                        configBeeps(0, 0x59, deviceSetting.longCount, (uint8_t *) &deviceSetting.long1);
                        break;
                    case CONFIG_SHORT_BEEPS: //0x24
                        configBeeps(0, 0x59, deviceSetting.shortCount, (uint8_t *) &deviceSetting.short1);
                        break;
                    case CONFIG_DELTA_TIME: //0x31
                        configDeltaTime();
                        break;
                    case CONFIG_REAL_TIME: //0x32
                        configRealTime();
                        break;
                    case CONFIG_BATTERY_LEVEL: //0x33
                        batteryLevelMenu();
                        break;
                    case CONFIG_RESET_FACTORY: //0x34
                        setFactorySetting();
                        saveSettings();
                        configStatus = CONFIG_EXIT;
                        break;
                }
                break;
            case PRESSED_CANCEL:
                configStatus = configStatus >> 4;
                minValue = 0x01;
                maxValue = 0x03;
                break;
        }
    }
}

void makeBeep(void){
    uint8_t *tmp;
    if (isMinus == TRUE){
        return;
    }
    tmp = (uint8_t *) &deviceSetting.short1;
    for(uint8_t i = 0; i < deviceSetting.shortCount; i++){
        if((time.seconds == *tmp) && (lastBeepSecond != time.seconds)){
            PORTD |= (1<<BUZZER_SHORT);
            _delay_ms(150);
            PORTD &= ~(1<<BUZZER_SHORT);
            break;
        }
        tmp++;
    }
    tmp = (uint8_t *) &deviceSetting.long1;
    for(uint8_t i = 0; i < deviceSetting.longCount; i++){
        if((time.seconds == *tmp) && (lastBeepSecond != time.seconds)){
            PORTD |= (1<<BUZZER_LONG);
            _delay_ms(350);
            PORTD &= ~(1<<BUZZER_LONG);
            break;
        }
        tmp++;
    }
    lastBeepSecond = time.seconds;
}

void calculateTime(void)
{
    uint32_t realTime;
    uint8_t temp;
    realTime = getTimeStamp();
    isMinus = FALSE;
    if (deviceSetting.status == STATUS_MINUS_TIME_START && realTime >= deviceSetting.zeroTime) {
        deviceSetting.status = STATUS_ZERO_TIME_START;
        saveSettings();
    }
    if (deviceSetting.status == STATUS_MINUS_TIME_START && realTime < deviceSetting.zeroTime) {
        realTime = deviceSetting.zeroTime - realTime;
        isMinus = TRUE;
    } else if (realTime < deviceSetting.zeroTime) {
        realTime = 86400 - deviceSetting.zeroTime + realTime;
    } else {
        realTime -= deviceSetting.zeroTime;
    }
    temp = realTime % 60;
    time.seconds = (temp / 10 << 4) | (temp % 10);
    temp = realTime % 3600 / 60;
    time.minutes = (temp / 10 << 4) | (temp % 10);
    temp = realTime / 3600;
    time.hours = (temp / 10 << 4) | (temp % 10);
}

ISR(INT1_vect)
{
    cli();
    if (deviceSetting.status != STATUS_CONFIG_MENU) {
        oldStatus = deviceSetting.status;
    }
    deviceSetting.status = STATUS_CONFIG_MENU;
}

void displayHello(void)
{
    char hello[] = { "HELLO" };
    uint8_t i = 0;

    showTime = FALSE;
    while (hello[i] != 0x00) {
        spiMasterTransmit(hello[i]);
        renewDisplay();
        _delay_ms(400);
        i++;
    }
    i = 6;
    while (i != 0) {
        spiMasterTransmit(CHAR_SPACE);
        renewDisplay();
        _delay_ms(400);
        i--;
    }
}

int main(void)
{
    init();
    initUSART();
    initSpiMaster();
    initFeatures();
    // Relax after exhausting work
    asm volatile ("nop");
    asm volatile ("nop");
    clearDisplay();

    // Read settings from memory
    readSettings();

    // Set factory settings
    if (deviceSetting.saved == FALSE) {
        setFactorySetting();
    }

    if (deviceSetting.status == STATUS_REAL_TIME) {
        displayHello();
    }

    // Display battery level in control panel
    _delay_ms(100);
    displayBatteryLevelInTrafficLight();
    _delay_ms(100);
    isBatteryLow();

    // Repeat indefinitely
    for(;;)
    {
        if (readTime()) {
            switch (deviceSetting.status)
            {
                case STATUS_ZERO_TIME_START:
                case STATUS_PLUS_TIME_START:
                case STATUS_MINUS_TIME_START:
                    calculateTime();
                case STATUS_REAL_TIME_START:
                    displayTime();
                    makeBeep();
                    renewBatteryLevelInTrafficLight();
                    break;
                case STATUS_REAL_TIME:
                    displayTime();
                    renewBatteryLevelInTrafficLight();
                    break;
                case STATUS_LOW_BATTERY:
                    makeLowBatterySound();
                    break;
                case STATUS_CONFIG_MENU:
                    cli();
                    configureDevice();
                    _delay_ms(100);
                    sei();
                    break;
            }
        }
    }
}
