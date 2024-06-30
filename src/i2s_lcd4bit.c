#include "i2s_lcd4bit.h"
#include <stddef.h>

static uint8_t _DrawBufID;
static uint8_t _FrameBuffer[NUM_FRAMEBUFFERS][FRAME_SIZE];

// I2S peripheral has 32bit internal bus. Unfortunately, there is a byte alignment quirk where in 8bit parallel mode,
// the bytes have to be sent to the I2S peripheral in 2,3,0,1 order if you want them to appear at the bus interface in 0,1,2,3 order
static const size_t _bytePosLkp[4] = {2, 3, 0, 1};
#define ESP32_CRAP_ALIGN(address) (((address) & 0xfffffffc) | (uint32_t)_bytePosLkp[(address) & 3])

static const uint8_t _pixel_masks[4] = {7, 0xb, 0xd, 0xe};
static const uint8_t _pixel_values[4] = {8, 4, 2, 1};

static uint8_t *LCD_GetCururentFrameBuffer(void)
{
    return _FrameBuffer[_DrawBufID];
}

void LCD_PixelSet(int x, int y, bool set)
{
    if (x < 0 || x > NUM_COLS)
    {
        return;
    }

    if (y < 0 || y > NUM_ROWS)
    {
        return;
    }

    uint32_t pos = (164 * (y + 1)) + 1 + (x >> 2);
    uint8_t *pPkt = LCD_GetCururentFrameBuffer();
    uint8_t pixel = x % 4;
    uint8_t curr = pPkt[ESP32_CRAP_ALIGN(pos)];
    if (set)
    {
        curr |= _pixel_values[pixel];
    }
    else
    {
        curr &= _pixel_masks[pixel];
    }
    pPkt[ESP32_CRAP_ALIGN(pos)] = curr;
}

void LCD_ClearScreen(uint8_t pattern)
{
    uint32_t pos = 0;
    uint8_t *pPkt = LCD_GetCururentFrameBuffer();

    for (pos = 0; pos < FRAME_SIZE; pos++)
    {
        uint32_t in_frame = pos % (uint32_t)164;

        // t=0 => HS
        if (in_frame == 0)
        {
            pPkt[ESP32_CRAP_ALIGN(pos)] |= ((uint8_t)(1 << BIT_HS));
        }
        else
        {
            pPkt[ESP32_CRAP_ALIGN(pos)] = pattern & 0xf;
        }

        // slot 4-348 => VS
        if ((pos >= 2 + 164) && (pos <= 2 + 164 + 163))
        {
            pPkt[ESP32_CRAP_ALIGN(pos)] |= ((uint8_t)(1 << BIT_VS));
        }

        // t>3 && t<163
        if (pos >= 163)
        {
            // slot 4-340 => clock on
            if ((in_frame >= 3) && (in_frame <= 162))
            {
                pPkt[ESP32_CRAP_ALIGN(pos)] |= ((uint8_t)(1 << BIT_CLK_EN));
            }
        }
    }
}

void LCD_ClearLine(uint8_t pattern, int ln)
{
    uint8_t *pPkt = LCD_GetCururentFrameBuffer();
    pPkt += (NUM_ROW_BYTES * ln);
    pattern &= 0xf;
    for (int row = ln; row < ln + 8; row++)
    {
        uint32_t starting_pos = ((164 * (row + 1)) + 1);
        for (uint32_t pos = starting_pos; pos < starting_pos + (NUM_COLS / 4); pos++)
        {
            pPkt[ESP32_CRAP_ALIGN(pos)] &= 0xf0;
            pPkt[ESP32_CRAP_ALIGN(pos)] |= pattern;
        }
    }
}

void LCD_LoadBitmap(uint8_t *pImg, uint16_t x, uint16_t y, uint16_t width, uint16_t height, bool transp)
{
    uint8_t *pSrc = pImg;
    uint16_t max_row = ((y + height) > NUM_ROWS) ? NUM_ROWS : y + height;
    uint16_t max_col = ((x + width) > NUM_COLS) ? NUM_COLS : x + width;
    uint32_t pos = 0;
    uint8_t *pPkt = LCD_GetCururentFrameBuffer();

    for (int row = y; row < max_row; row++)
    {
        pos = (164 * (row + 1)) + 1 + (x / 4);

        for (int col = x / 4; col < max_col / 4; col += 4)
        {
            uint8_t d8a = *pSrc++;
            uint8_t d8b = *pSrc++;

            if (!transp)
            {
                pPkt[ESP32_CRAP_ALIGN(pos)] &= 0xf0;
            }
            pPkt[ESP32_CRAP_ALIGN(pos)] |= d8a >> 4;
            pos++;

            if (!transp)
            {
                pPkt[ESP32_CRAP_ALIGN(pos)] &= 0xf0;
            }
            pPkt[ESP32_CRAP_ALIGN(pos)] |= d8a & 0xf;
            pos++;

            if (!transp)
            {
                pPkt[ESP32_CRAP_ALIGN(pos)] &= 0xf0;
            }
            pPkt[ESP32_CRAP_ALIGN(pos)] |= d8b >> 4;
            pos++;

            if (!transp)
            {
                pPkt[ESP32_CRAP_ALIGN(pos)] &= 0xf0;
            }
            pPkt[ESP32_CRAP_ALIGN(pos)] |= d8b & 0xf;
            pos++;
        }
    }
}

void *LCD_GetFrameBuffer(uint8_t id)
{
    if (id < NUM_FRAMEBUFFERS)
    {
        return &_FrameBuffer[id];
    }

    return NULL;
}
void LCD_SelectFrameBuffer(uint8_t id)
{
    if (id < NUM_FRAMEBUFFERS)
    {
        _DrawBufID = id;
    }
}
uint8_t LCD_GetFrameBufferSelected(void)
{
    return _DrawBufID;
}
