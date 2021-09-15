#include "common.h"
#include <avr/sleep.h>

/*  Defines  */

#ifdef __AVR_ATtiny85__
    #if F_CPU != 4000000UL || not defined DISABLEMILLIS
        #error Board Configuration is wrong...
    #endif
    #define ATTINY85
#elif not defined __AVR_ATmega32U4__
    #error Sorry, Unsupported Board...
#endif

#ifdef ATTINY85
#define SimpleWire_SCL_PORT B
#define SimpleWire_SCL_POS  2
#define SimpleWire_SDA_PORT B
#define SimpleWire_SDA_POS  0
#else
#define SimpleWire_SCL_PORT D
#define SimpleWire_SCL_POS  0
#define SimpleWire_SDA_PORT D
#define SimpleWire_SDA_POS  1
#endif
#include "SimpleWire.h"
#define SIMPLEWIRE      SimpleWire<SimpleWire_1M>

#define DELAY_LONG_MS   200
#define DELAY_SHORT_MS  50

#define SSD1306_ADDRESS 0x3C
#define SSD1306_COMMAND 0x00
#define SSD1306_DATA    0x40

#ifdef ATTINY85
#define VPERI_PIN       3
#define RESUME_PIN      1
#define BUTTONS_PIN     A2
#else
#define VPERI_PIN       14
#define RESUME_PIN      15
#define BUTTONS_PIN     A0
#define DEBUG_PIN1      17
#define DEBUG_PIN2      30
#endif

/*  Local Functions  */

static void setupDisplay(void);
static void shutdownDisplay(void);

/*  Macro functions  */

#define buttonInfo(button, theta)   ((theta) << 5 | (button))
#define getThresholdFromInfo(info)  ((info) >> 5)
#define getButtonFromInfo(info)     ((info) & 0x1F)

/*  Local Variables  */

PROGMEM static const uint8_t ssd1306InitSequence[] = { // Initialization Sequence
    SSD1306_COMMAND,
    0xAE,           // Set Display ON/OFF - AE=OFF, AF=ON
    0xD5, 0xF0,     // Set display clock divide ratio/oscillator frequency, set divide ratio
    0xA8, 0x1F,     // Set multiplex ratio (1 to 64) ... (height - 1)
    0xD3, 0x00,     // Set display offset. 00 = no offset
    0x40 | 0x00,    // Set start line address, at 0.
    0x8D, 0x14,     // Charge Pump Setting, 14h = Enable Charge Pump
    0x20, 0x00,     // Set Memory Addressing Mode - 00=Horizontal, 01=Vertical, 10=Page, 11=Invalid
    0x22, 0x00, 0x03, // Set Page Address, start page 0, end page 3
    0xA0 | 0x01,    // Set Segment Re-map
    0xC8,           // Set COM Output Scan Direction
    0xDA, 0x02,     // Set COM Pins Hardware Configuration - 128x32:0x02, 128x64:0x12
    0x81, 0x8F,     // Set contrast control register
    0xD9, 0x22,     // Set pre-charge period (0x22 or 0xF1)
    0xDB, 0x20,     // Set Vcomh Deselect Level - 0x00: 0.65 x VCC, 0x20: 0.77 x VCC (RESET), 0x30: 0.83 x VCC
    0xA4,           // Entire Display ON (resume) - output RAM to display
    0xA6,           // Set Normal/Inverse Display mode. A6=Normal; A7=Inverse
    0x2E,           // Deactivate Scroll command
    0xAF,           // Set Display ON/OFF - AE=OFF, AF=ON
    0xB0,           // Set page start, at 0.
    0x00, 0x10,     // Set column start, at 0.
};

PROGMEM static const uint16_t buttonInfoTable[] = {
    buttonInfo(BTN_NONE,1008),  buttonInfo(BTN_ENTER,974),  buttonInfo(BTN_7,   936),
    buttonInfo(BTN_4,    888),  buttonInfo(BTN_1,    830),  buttonInfo(BTN_0,   767),
    buttonInfo(BTN_DOT,  700),  buttonInfo(BTN_2,    629),  buttonInfo(BTN_5,   553),
    buttonInfo(BTN_8,    475),  buttonInfo(BTN_9,    397),  buttonInfo(BTN_6,   321),
    buttonInfo(BTN_3,    252),  buttonInfo(BTN_INVERT,189), buttonInfo(BTN_PLUS,134),
    buttonInfo(BTN_MINUS,87 ),  buttonInfo(BTN_MULTI,49 ),  buttonInfo(BTN_DIV, 15 ),
    buttonInfo(BTN_CLEAR,0  ),
};

static uint8_t  lastButton;
static uint8_t  wireBuffer[WIDTH + 1];

/*---------------------------------------------------------------------------*/

void initCore(void)
{

    pinMode(VPERI_PIN, OUTPUT);
    pinMode(RESUME_PIN, INPUT_PULLUP);
    pinMode(BUTTONS_PIN, INPUT);
#ifndef ATTINY85
    pinMode(DEBUG_PIN1, OUTPUT);
    pinMode(DEBUG_PIN2, OUTPUT);
    digitalWrite(DEBUG_PIN1, HIGH);
    digitalWrite(DEBUG_PIN2, HIGH);
#endif

    setupDisplay();
    lastButton = BTN_NONE;
}

void sleepCore(void)
{
    shutdownDisplay();
#ifdef ATTINY85
    GIMSK = 0x20;   // enable PCINT, do not allow INT0
    PCMSK = 0x02;   // pin change mask, set interrupt pin to PCINT1
    ADCSRA = 0x00;  // stop ADC (save 330uA)
#else
    digitalWrite(DEBUG_PIN2, LOW);
    PCICR = 0x01;   // enable PCINT
    PCMSK0 = 0x02;  // pin change mask, set interrupt pin to PCINT1
#endif
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    sleep_mode();   // sleep_(enable + cpu + disable)
#ifdef ATTINY85
    GIMSK = 0x00;   // disable PCINT
    ADCSRA = 0x80;  // restart ADC
#else
    PCICR = 0x00;   // disable PCING
    digitalWrite(DEBUG_PIN2, HIGH);
#endif
    setupDisplay();
    analogRead(BUTTONS_PIN);  // clear input charge of ADC
}

ISR(PCINT0_vect)
{ 
    // do nothing
}

/*---------------------------------------------------------------------------*/

static void setupDisplay(void)
{
    digitalWrite(VPERI_PIN, HIGH);
    _delay_ms(DELAY_LONG_MS);
    SIMPLEWIRE::begin();
    memcpy_P(wireBuffer, ssd1306InitSequence, sizeof(ssd1306InitSequence));
    SIMPLEWIRE::write(SSD1306_ADDRESS, wireBuffer, sizeof(ssd1306InitSequence));
}

static void shutdownDisplay(void)
{
    wireBuffer[0] = SSD1306_COMMAND;
    wireBuffer[1] = 0xAE;
    SIMPLEWIRE::write(SSD1306_ADDRESS, wireBuffer, 2);
    _delay_ms(DELAY_LONG_MS);
    digitalWrite(VPERI_PIN, LOW);
}

void refreshScreen(void (*func)(int16_t, uint8_t *))
{
    wireBuffer[0] = SSD1306_DATA;
    for (int16_t y = 0; y < HEIGHT; y += 8) {
        (func) ? func(y, &wireBuffer[1]) : clearScreenBuffer();
        SIMPLEWIRE::write(SSD1306_ADDRESS, wireBuffer, WIDTH + 1);
    }
}

void clearScreenBuffer(void)
{
    memset(&wireBuffer[1], 0, WIDTH);
}

/*---------------------------------------------------------------------------*/

uint8_t getDownButton(void)
{
    uint8_t currentButton;
    uint16_t analogValue = analogRead(BUTTONS_PIN);
    for (uint8_t i = 0; i < sizeof(buttonInfoTable) / 2; i++) {
        uint16_t buttonInfo = pgm_read_word(&buttonInfoTable[i]);
        if (analogValue >= getThresholdFromInfo(buttonInfo)) {
            currentButton = getButtonFromInfo(buttonInfo);
            break;
        }
    }

    uint8_t downButton = (lastButton == BTN_NONE) ? currentButton : BTN_NONE;
    lastButton = currentButton;
#ifndef ATTINY85
    digitalWrite(DEBUG_PIN1, (downButton == BTN_NONE) ? HIGH : LOW);
#endif
    return downButton;
}
