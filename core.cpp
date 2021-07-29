#include "common.h"

/*  Defines  */

#ifdef __AVR_ATtiny85__
    #if F_CPU != 16000000UL || defined DISABLEMILLIS 
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

#define SSD1306_ADDRESS 0x3C
#define SSD1306_COMMAND 0x00
#define SSD1306_DATA    0x40

#ifdef ATTINY85
#define BUTTONS_PIN     A3
#else
#define BUTTON_ROWS     4
#define BUTTON_COLS     4
#define LONG_PRESS      10
#endif

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

#ifdef ATTINY85
PROGMEM static const uint16_t buttonInfoTable[] = {
    buttonInfo(BTN_NONE,1015),  buttonInfo(BTN_PLUS, 971),  buttonInfo(BTN_ENTER, 933),
    buttonInfo(BTN_DOT,  887),  buttonInfo(BTN_0,    827),  buttonInfo(BTN_1,     762),
    buttonInfo(BTN_2,    696),  buttonInfo(BTN_3,    628),  buttonInfo(BTN_MINUS, 559),
    buttonInfo(BTN_MULTI,479),  buttonInfo(BTN_6,    393),  buttonInfo(BTN_5,     316),
    buttonInfo(BTN_4,    249),  buttonInfo(BTN_7,    189),  buttonInfo(BTN_8,     134),
    buttonInfo(BTN_9,    87 ),  buttonInfo(BTN_DIV,  49 ),  buttonInfo(BTN_CLEAR, 15 ),
    buttonInfo(BTN_INVERT,0 ),
};
#else
PROGMEM static const uint8_t buttonPinCol[BUTTON_COLS] = { 5, 4, 0, 1 };
PROGMEM static const uint8_t buttonPinRow[BUTTON_ROWS] = { 9, 8, 7, 6 };
PROGMEM static const uint8_t buttonTable[BUTTON_ROWS][BUTTON_COLS] = {
    { BTN_7,  BTN_8,   BTN_9,     BTN_DIV   },
    { BTN_4,  BTN_5,   BTN_6,     BTN_MULTI },
    { BTN_1,  BTN_2,   BTN_3,     BTN_MINUS },
    { BTN_0,  BTN_DOT, BTN_ENTER, BTN_PLUS  },
};
#endif

static uint8_t  lastButton;
static uint8_t  wireBuffer[WIDTH + 1];

/*---------------------------------------------------------------------------*/

void initCore(void)
{
    // Setup display
    SIMPLEWIRE::begin();
    memcpy_P(wireBuffer, ssd1306InitSequence, sizeof(ssd1306InitSequence));
    SIMPLEWIRE::write(SSD1306_ADDRESS, wireBuffer, sizeof(ssd1306InitSequence));

    // Setup buttons
#ifdef ATTINY85
    pinMode(BUTTONS_PIN, INPUT);
#else
    for (uint8_t row = 0; row < BUTTON_ROWS; row++) {
        uint8_t pin = pgm_read_byte(&buttonPinRow[row]);
        pinMode(pin, OUTPUT);
        digitalWrite(pin, HIGH);
    }
    for (uint8_t col = 0; col < BUTTON_COLS; col++) {
        pinMode(pgm_read_byte(&buttonPinCol[col]), INPUT_PULLUP);
    }
#endif
    lastButton = BTN_NONE;
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

uint8_t getDownButton(void)
{
    uint8_t currentButton;
#ifdef ATTINY85
    uint16_t analogValue = analogRead(BUTTONS_PIN);
    for (uint8_t i = 0; i < sizeof(buttonInfoTable) / 2; i++) {
        uint16_t buttonInfo = pgm_read_word(&buttonInfoTable[i]);
        if (analogValue >= getThresholdFromInfo(buttonInfo)) {
            currentButton = getButtonFromInfo(buttonInfo);
            break;
        }
    }
#else
    static uint8_t pressCounter = 0;
    currentButton = BTN_NONE;
    for (uint8_t row = 0; currentButton == BTN_NONE && row < BUTTON_ROWS; row++) {
        uint8_t pin = pgm_read_byte(&buttonPinRow[row]);
        digitalWrite(pin, LOW);
        for (uint8_t col = 0; currentButton == BTN_NONE && col < BUTTON_COLS; col++) {
            if (digitalRead(pgm_read_byte(&buttonPinCol[col])) == LOW) {
                currentButton = pgm_read_byte(&buttonTable[row][col]);
            }
        }
        digitalWrite(pin, HIGH);
    }
    if (currentButton == BTN_ENTER && lastButton == BTN_ENTER) {
        if (++pressCounter == LONG_PRESS) {
            lastButton = BTN_NONE;
            currentButton = BTN_CLEAR;
        }
    } else {
        pressCounter = 0;
    }
#endif

    uint8_t downButton = (lastButton == BTN_NONE) ? currentButton : BTN_NONE;
    lastButton = currentButton;
    return downButton;
}
