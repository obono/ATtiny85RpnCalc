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

#if DISPLAY == DISPLAY_LCD

#define LCD_ADDRESS     0x3E
#define LCD_CMD_CONT    0b10000000 // Co = 1, RS = 0
#define LCD_CMD_LAST    0b00000000 // Co = 0, RS = 0
#define LCD_DATA_CONT   0b11000000 // Co = 1, RS = 1
#define LCD_DATA_LAST   0b01000000 // Co = 0, RS = 1

#define LCD_CONTRAST    10  // 0-63, tune for Vperi and so on (ex. LiPo:10, LR44x3-Di:15)

PROGMEM static const uint8_t lcdInitSequence1[] = {
    LCD_CMD_CONT, 0b00111001, // Function set (Extension mode)
    LCD_CMD_CONT, 0b00000100, // Entry mode set
    LCD_CMD_CONT, 0b00010100, // Internal OSC
    LCD_CMD_CONT, 0b01110000 | (LCD_CONTRAST & 0xF),        // Contrast Low
    LCD_CMD_CONT, 0b01011100 | ((LCD_CONTRAST >> 4) & 0x3), // Contast High / Icon / Power
    LCD_CMD_LAST, 0b01101100, // Follower control
};

PROGMEM static const uint8_t lcdInitSequence2[] = {
    LCD_CMD_CONT, 0b00111000, // Function set (Normal mode)
    LCD_CMD_CONT, 0b00001100, // Display On
    LCD_CMD_LAST, 0b00000001, // Clear Display
};

PROGMEM static const uint8_t lcdInitSequence3[] = {
    LCD_CMD_CONT, 0b01000000, // Set CGRAM address 0
    LCD_DATA_CONT, 0x00,
    LCD_DATA_CONT, 0x00,
    LCD_DATA_CONT, 0x00,
    LCD_DATA_CONT, 0x00,
    LCD_DATA_CONT, 0x00,
    LCD_DATA_CONT, 0x15,
    LCD_DATA_CONT, 0x00,
    LCD_DATA_LAST, 0x00,
};

static uint8_t wireBuffer[WIDTH * 2 + 2];
static uint8_t vram[WIDTH];

static void setupDisplay(void)
{
    digitalWrite(VPERI_PIN, HIGH);

    // Setup
    _delay_ms(DELAY_SHORT_MS);
    memcpy_P(wireBuffer, lcdInitSequence1, sizeof(lcdInitSequence1));
    SIMPLEWIRE::write(LCD_ADDRESS, wireBuffer, sizeof(lcdInitSequence1));
    _delay_ms(DELAY_LONG_MS);
    memcpy_P(wireBuffer, lcdInitSequence2, sizeof(lcdInitSequence2));
    SIMPLEWIRE::write(LCD_ADDRESS, wireBuffer, sizeof(lcdInitSequence2));
    _delay_ms(DELAY_SHORT_MS);

    // CGRAM
    memcpy_P(wireBuffer, lcdInitSequence3, sizeof(lcdInitSequence3));
    SIMPLEWIRE::write(LCD_ADDRESS, wireBuffer, sizeof(lcdInitSequence3));
    for (uint8_t i = 1; i <= LCD_GLYPH_W; i++) {
        wireBuffer[1] = 0b01000000 | (i << 3); // Set CGRAM address
        uint8_t ptn = (1 << i) - 1;
        for (uint8_t j = 5; j <= 9; j += 2) {
            wireBuffer[j] = ptn;
        } 
        SIMPLEWIRE::write(LCD_ADDRESS, wireBuffer, sizeof(lcdInitSequence3));
    }

    // Preset wireBuffer
    if (WIDTH > LCD_GLYPH_H) memset(&wireBuffer[LCD_GLYPH_H * 2], LCD_DATA_CONT, (WIDTH - LCD_GLYPH_H) * 2);
    if (WIDTH != LCD_GLYPH_H) wireBuffer[WIDTH * 2] = LCD_DATA_LAST;
    // After this, wireBuffer[] must be kept 1 command and (WIDTH) data until shotdown.
}

static void shutdownDisplay(void)
{
    wireBuffer[0] = LCD_CMD_LAST;
    wireBuffer[1] = 0b00000001; // Clear Display
    SIMPLEWIRE::write(LCD_ADDRESS, wireBuffer, 2);
    _delay_ms(DELAY_LONG_MS);
    digitalWrite(VPERI_PIN, LOW);
}

void refreshScreen(void (*func)(int16_t, uint8_t *))
{
    for (int16_t y = 0; y < HEIGHT; y++) {
        (func) ? func(y, vram) : clearScreenBuffer();
        wireBuffer[1] = 0x80 | (y * 0x40); // Set cursor position
        for (uint8_t *pSrc = vram, *pDst = &wireBuffer[3]; pSrc < &vram[WIDTH]; pSrc++, pDst += 2) {
            *pDst = *pSrc;
        }
        SIMPLEWIRE::write(LCD_ADDRESS, wireBuffer, sizeof(wireBuffer));
    }
}

void clearScreenBuffer(void)
{
    memset(vram, ' ', WIDTH);
}

#endif

/*---------------------------------------------------------------------------*/

#if DISPLAY == DISPLAY_OLED

#define SSD1306_ADDRESS 0x3C
#define SSD1306_COMMAND 0x00
#define SSD1306_DATA    0x40

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

static uint8_t wireBuffer[WIDTH + 1];

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
    wireBuffer[1] = 0xAE; // Display OFF
    SIMPLEWIRE::write(SSD1306_ADDRESS, wireBuffer, 2);
    _delay_ms(DELAY_LONG_MS);
    digitalWrite(VPERI_PIN, LOW);
}

void refreshScreen(void (*func)(int16_t, uint8_t *))
{
    wireBuffer[0] = SSD1306_DATA;
    for (int16_t y = 0; y < HEIGHT; y += PAGE_HEIGHT) {
        (func) ? func(y, &wireBuffer[1]) : clearScreenBuffer();
        SIMPLEWIRE::write(SSD1306_ADDRESS, wireBuffer, WIDTH + 1);
    }
}

void clearScreenBuffer(void)
{
    memset(&wireBuffer[1], 0, WIDTH);
}

#endif

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
