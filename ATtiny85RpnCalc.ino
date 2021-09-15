#include "common.h"

/*  Defines  */

#define DELAY_LOOP_MS   100
#define IDLE_COUNT_MAX  (2 * 60 * (1000 / DELAY_LOOP_MS)) // 2 minutes

/*---------------------------------------------------------------------------*/

void setup(void)
{
    initCore();
    initCalc();
    refreshScreen(drawCalc);
}

void loop(void)
{
    static uint16_t idle_count = 0;
    uint8_t button = getDownButton();
    if (button != BTN_NONE) {
        bool isInvalid = updateCalc(button);
        if (isInvalid) refreshScreen(drawCalc);
        idle_count = 0;
    } else if (++idle_count >= IDLE_COUNT_MAX) {
        sleepCore();
        refreshScreen(drawCalc);
        idle_count = 0;
    }
    _delay_ms(DELAY_LOOP_MS);
}
