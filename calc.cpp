#include "common.h"
#include "data.h"

/*  Defines  */

#define MAX_DISP_LEN 10

/*  Local Functions  */

static void push(void);
static void pop(void);
static bool execute(uint8_t button);
static void drawDispDigits(uint8_t *pBuffer, uint8_t row);

/*  Local Variables  */

static float    stackX, stackY, stackZ, stackT;
static String   disp;
static bool     enterFlg, opFlg;

/*---------------------------------------------------------------------------*/
/*                               Main Functions                              */
/*---------------------------------------------------------------------------*/

void initCalc(void)
{
    stackX = stackY = stackZ = stackT = 0;
    disp = "";
    enterFlg = opFlg = false;
}

bool updateCalc(uint8_t button)
{
    return execute(button);
}

void drawCalc(int16_t y, uint8_t *pBuffer)
{
    clearScreenBuffer();
    if (y == 8 || y == 16) drawDispDigits(pBuffer, (y - 8) >> 3);
}

/*---------------------------------------------------------------------------*/
/*                             Control Functions                             */
/*---------------------------------------------------------------------------*/

static void push(void)
{
    stackT = stackZ;
    stackZ = stackY;
    stackY = stackX;
}

static void pop(void)
{
    stackX = stackY;
    stackY = stackZ;
    stackZ = stackT;
}

static bool execute(uint8_t button)
{
    bool ret = false;
    
    if (button >= BTN_PLUS && button <= BTN_DIV) {
        float acc = stackX;
        pop();
        switch (button) {
            case BTN_PLUS:  stackX += acc; break;
            case BTN_MINUS: stackX -= acc; break;
            case BTN_MULTI: stackX *= acc; break;
            case BTN_DIV:   stackX /= acc; break;
        }
        disp = String(stackX);
        opFlg = true;
        enterFlg = false;
        ret = true;
    } else if (button == BTN_ENTER) {
        push();
        disp = String(stackX);
        enterFlg = true;
        opFlg = false;
        ret = true;
    } else if (button == BTN_CLEAR) {
        initCalc();
        ret = true;
    } else {
        if (enterFlg) {
            disp = "";
            enterFlg = false;
        } else if (opFlg) {
            push();
            disp = "";
            opFlg = false;
        }
        if (button >= BTN_0 && button <= BTN_9) {
            if (disp.length() < MAX_DISP_LEN) {
                disp.concat(button - BTN_0);
                stackX = disp.toFloat();
                ret = true;
            }
        } else if (button == BTN_DOT) {
            if (disp.indexOf(".") == -1 && disp.length() < MAX_DISP_LEN - 3) {
                disp.concat(".");
                stackX = disp.toFloat();
                ret = true;
            }
        } else if (button == BTN_INVERT) {
            if (disp.charAt(0) == '-') {
                disp = disp.substring(1);
            } else {
                disp = "-" + disp;
            }
            stackX = disp.toFloat();
            ret = true;
        }
    }

    return ret;
}

static void drawDispDigits(uint8_t *pBuffer, uint8_t row)
{
    int16_t x = 0;
    uint8_t r = row * IMG_DIGIT_W;
    uint8_t len = disp.length();

    if (len > MAX_DISP_LEN) {
        // Error
        memcpy_P(&pBuffer[x], &imgDigit[12][r], IMG_DIGIT_W);
    } else {
        for (uint8_t i = 0; i < len && x < WIDTH; i++) {
            char c = disp.charAt(i);
            if (c >= '-' && c <= '9') {
                int8_t w = (c == '.') ? IMG_DOT_W : IMG_DIGIT_W;
                c -= '-' + (c >= '0');
                memcpy_P(&pBuffer[x], &imgDigit[c][r], w);
                x += w + IMG_PADDING;
            }
        }
    }
}
