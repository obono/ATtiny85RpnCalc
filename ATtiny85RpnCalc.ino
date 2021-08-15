#include <util/delay.h>
#include "common.h"

/*  Defines  */

#define DELAY_LOOP  100

/*---------------------------------------------------------------------------*/

void setup(void)
{
    initCore();
    initCalc();
    refreshScreen(drawCalc);
}

void loop(void)
{
    uint8_t button = getDownButton();
    if (button != BTN_NONE) {
        bool isInvalid = updateCalc(button);
        if (isInvalid) refreshScreen(drawCalc);
    }
    _delay_ms(DELAY_LOOP);
}
