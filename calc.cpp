#include "common.h"
#include "data.h"

/*  Defines  */

#define RADIX       10
#define LENGTH_MAX  8
#define STACK_SIZE  16
#define DECODE_MAX  12
#define BIG_NUMBER  999999999UL
#define EXP_MAX     99

/*  Typedefs  */

typedef struct {
    int32_t m;
    int8_t  exp;
} NUM_T;


/*  Local Functions  */

static void     prepareNumber(void);
static bool     modifyNumber(uint8_t button);
static bool     enterNumber(void);
static bool     clearNumber(void);
static bool     operate(void (*opFunc)(NUM_T *a, NUM_T *b));

static void     add(NUM_T *a, NUM_T *b);
static void     sub(NUM_T *a, NUM_T *b);
static void     multi(NUM_T *a, NUM_T *b);
static void     div(NUM_T *a, NUM_T *b);

static void     setZero(NUM_T *n);
static int8_t   getLength(NUM_T *n);
static void     align(NUM_T *a, NUM_T *b);
static void     normalize(NUM_T *n);

static int8_t   decodeNumber(NUM_T *n);
static void     drawStack(uint8_t *pBuffer);
static void     drawNumber(uint8_t *pBuffer, int16_t x, int8_t row);
static void     drawEntering(uint8_t *pBuffer);

/*  Local Variables  */

PROGMEM static void (*const opFuncTable[])(NUM_T *a, NUM_T *b) = {
    add, sub, multi, div
};

static NUM_T    stack[STACK_SIZE], *pStack;
static uint8_t  decodeBuffer[DECODE_MAX];
static bool     isEntering, isDotted, isError;

/*---------------------------------------------------------------------------*/
/*                               Main Functions                              */
/*---------------------------------------------------------------------------*/

void initCalc(void)
{
    pStack = &stack[0];
    prepareNumber();
    isError = false;
}

bool updateCalc(uint8_t button)
{
    if (button == BTN_ALLCLEAR) {
        initCalc();
        return true;
    } else if (button == BTN_CLEAR) {
        return clearNumber();
    } else if (!isError) {
        if (button == BTN_ENTER) {
            return enterNumber();
        } else if (button >= BTN_PLUS && button <= BTN_DIV) {
            void *opFunc = pgm_read_ptr(&opFuncTable[button - BTN_PLUS]);
            return operate((void (*)(NUM_T *a, NUM_T *b))opFunc);
        } else {
            return modifyNumber(button);
        }
    }
    return false;
}

void drawCalc(int16_t y, uint8_t *pBuffer)
{
    clearScreenBuffer();
    if (y == 0) {
        drawStack(pBuffer);
        decodeNumber(pStack);
    } else {
        drawNumber(pBuffer, WIDTH + IMG_PADDING, (y - 8) >> 3);
        if (y == 8 && isEntering) drawEntering(pBuffer);
    }
}

/*---------------------------------------------------------------------------*/
/*                             Control Functions                             */
/*---------------------------------------------------------------------------*/

static void prepareNumber(void)
{
    setZero(pStack);
    isEntering = true;
    isDotted = false;
}

static bool modifyNumber(uint8_t button)
{
    bool ret = false;
    if (!isEntering && pStack < &stack[STACK_SIZE - 1]) {
        pStack++;
        prepareNumber();
        ret = true;
    }
    if (isEntering) {
        if (button >= BTN_0 && button <= BTN_9) {
            if (getLength(pStack) < LENGTH_MAX && 1 - pStack->exp < LENGTH_MAX) {
                int8_t num = button - BTN_0;
                if (pStack->m < 0) num = -num;
                pStack->m = pStack->m * RADIX + num;
                if (isDotted) pStack->exp--;
                ret = true;
            }
        } else if (button == BTN_DOT) {
            if (!isDotted) {
                isDotted = true;
                ret = true;
            }
        } else if (button == BTN_INVERT) {
            if (pStack->m != 0) {
                pStack->m = -pStack->m;
                ret = true;
            }
        }
    }
    return ret;
}

static bool enterNumber(void)
{
    if (isEntering) {
        normalize(pStack);
        isEntering = false;
    } else if (pStack < &stack[STACK_SIZE - 1]) {
        pStack++;
        *pStack = *(pStack - 1);
    }
    return true;
}

static bool clearNumber(void)
{
    if (pStack > &stack[0]) {
        pStack--;
        isEntering = false;
    } else {
        prepareNumber();
    }
    isError = false;
    return true;
}

static bool operate(void (*opFunc)(NUM_T *a, NUM_T *b))
{
    bool ret = false;
    if (pStack > &stack[0]) {
        if (isEntering) normalize(pStack);
        pStack--;
        opFunc(pStack, pStack + 1);
        normalize(pStack);
        if (pStack->exp > 0) isError = true; // too large
        isEntering = false;
        ret = true;
    }
    return ret;
}

static void add(NUM_T *a, NUM_T *b)
{
    align(a, b);
    a->m += b->m;
}

static void sub(NUM_T *a, NUM_T *b)
{
    align(a, b);
    a->m -= b->m;
}

static void multi(NUM_T *a, NUM_T *b)
{
    int32_t mA = abs(a->m), mB = abs(b->m);
    if (mB != 0) {
        while (BIG_NUMBER / mB < mA) {
            a->exp++;
            if (mA % RADIX == 0) {
                mA /= RADIX;
            } else if (mB % RADIX == 0 || mB > mA) {
                mB /= RADIX;
            } else {
                mA /= RADIX;
            }
        }
    }
    a->m = ((a->m < 0) == (b->m < 0)) ? mA * mB : -mA * mB;
    a->exp += b->exp;
}

static void div(NUM_T *a, NUM_T *b)
{
    if (b->m == 0) {
        a->m = BIG_NUMBER;
        a->exp = EXP_MAX;
        isError = true;
        return;
    }
    int32_t odd = a->m % b->m;
    a->m /= b->m;
    a->exp -= b->exp;
    int8_t len = getLength(a);
    while (odd != 0 && len < LENGTH_MAX) {
        odd *= RADIX;
        int32_t d = odd / b->m;
        a->m = a->m * RADIX + d;
        len += (a->m != 0);
        a->exp--;
        odd -= d * b->m;
    }
}

static void setZero(NUM_T *n)
{
    n->m = 0;
    n->exp = 0;
}

static int8_t getLength(NUM_T *n)
{
    int8_t len = 0;
    int32_t m = abs(n->m);
    int32_t z = 1;
    while (m >= z) {
        len++;
        z *= RADIX;
    }
    return len;
}

static void align(NUM_T *a, NUM_T *b)
{
    if (a->exp < b->exp) {
        align(b, a);
        return;
    }
    int8_t len = getLength(a);
    while (a->exp > b->exp) {
        if (len < LENGTH_MAX) {
            len++;
            a->exp--;
            a->m *= RADIX;
        } else {
            b->exp++;
            b->m /= RADIX;
        }
    }
}

static void normalize(NUM_T *n)
{
    if (n->m == 0) {
        setZero(n);
        return;
    }
    int8_t len = getLength(n);
    while (len > LENGTH_MAX || n->exp < 0 && n->m % RADIX == 0) {
        len--;
        n->exp++;
        n->m /= RADIX;
    }
    while (n->exp > 0 && len < LENGTH_MAX) {
        len++;
        n->exp--;
        n->m *= RADIX;
    }
    if (len + n->exp <= 1 - LENGTH_MAX) setZero(n); // too small
}

/*---------------------------------------------------------------------------*/
/*                              Draw Functions                               */
/*---------------------------------------------------------------------------*/

static int8_t decodeNumber(NUM_T *n)
{
    uint8_t *pBuf = decodeBuffer;
    int32_t m = abs(n->m);
    int8_t len = getLength(n) + (m == 0);
    int8_t exp = n->exp;
    if (len < 1 - exp) len = 1 - exp;

    if (exp > 0) {
        *pBuf++ = IMG_ID_BAR;
        *pBuf++ = IMG_ID_E;
        *pBuf++ = IMG_ID_BAR;
    } else {
        while (len > 0) {
            if (len <= LENGTH_MAX) {
                if (exp == 0) *pBuf++ = IMG_ID_DOT;
                *pBuf++ = m % RADIX;
            }
            len--;
            exp++;
            m /= RADIX;
        }
        if (n->m < 0) *pBuf++ = IMG_ID_BAR;
    }

    *pBuf = IMG_ID_MAX;
    return pBuf - decodeBuffer;
}

static void drawStack(uint8_t *pBuffer)
{
    int16_t x = WIDTH + IMG_SUB_PADDING;
    for (NUM_T *n = pStack - 1; n >= &stack[0] && x >= STACK_SIZE * 2; n--) {
        int16_t len = decodeNumber(n);
        drawNumber(pBuffer, x, -1);
        x -= (len + 1) * (IMG_SUB_DIGIT_W + IMG_SUB_PADDING) - (IMG_SUB_DIGIT_W - IMG_SUB_DOT_W); 
    }

    int8_t stackPos = &stack[STACK_SIZE - 1] - pStack;
    uint8_t *p = pBuffer;
    *p++ = 0x7F;
    for (int8_t i = 0; i < STACK_SIZE - 1; i++) {
        if (i >= stackPos) {
            *p++ = 0x55;
            *p++ = 0x6B;
        } else {
            *p++ = 0x41;
            *p++ = 0x41;
        }
    }
    *p++ = 0x7F;
}

static void drawNumber(uint8_t *pBuffer, int16_t x, int8_t row)
{
    for (uint8_t *pBuf = decodeBuffer; *pBuf < IMG_ID_MAX; pBuf++) {
        const uint8_t *pImg;
        int8_t w; 
        if (row >= 0) {
            pImg = &imgDigit[*pBuf][row * IMG_DIGIT_W];
            w = (*pBuf == IMG_ID_DOT) ? IMG_DOT_W : IMG_DIGIT_W;
            x -= IMG_PADDING;
        } else {
            pImg = imgSubDigit[*pBuf];
            w = (*pBuf == IMG_ID_DOT) ? IMG_SUB_DOT_W : IMG_SUB_DIGIT_W;
            x -= IMG_SUB_PADDING;
        }
        x -= w;
        if (x < 0) break;
        memcpy_P(&pBuffer[x], pImg, w);
    }
}

static void drawEntering(uint8_t *pBuffer)
{
    memcpy_P(&pBuffer[0], imgEntering, IMG_ENTERING_W);
}
