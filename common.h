#pragma once

#include <Arduino.h>

/*  Defines  */

#define WIDTH       128
#define HEIGHT      32
#define PAGE_HEIGHT 8

enum : uint8_t {
    BTN_NONE = 0,
    BTN_0,
    BTN_1,
    BTN_2,
    BTN_3,
    BTN_4,
    BTN_5,
    BTN_6,
    BTN_7,
    BTN_8,
    BTN_9,
    BTN_ENTER,
    BTN_DOT,
    BTN_INVERT,
    BTN_PLUS,
    BTN_MINUS,
    BTN_MULTI,
    BTN_DIV,
    BTN_CLEAR,
    BTN_ALLCLEAR,
};

/*  Global Functions  */

void    initCore(void);
void    refreshScreen(void (*func)(int16_t, uint8_t *));
void    clearScreenBuffer(void);
uint8_t getDownButton(void);

void    initCalc(void);
bool    updateCalc(uint8_t button);
void    drawCalc(int16_t y, uint8_t *pBuffer);
