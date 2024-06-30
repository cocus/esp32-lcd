#ifndef _I2S_LCD_4BIT_H_
#define _I2S_LCD_4BIT_H_

#include <stdint.h>
#include <stdbool.h>

// graphics for controller-less monochrome LCD display with 4bit data bus,
// hsync, vsync and fr signals.  The fr signal is generated externally
// in hw from the vsync signal. So we need to encode the 4bit data + hsync + vsync
// on a 6bit wide parallel bus to the LCD. For this we use the ESP32 i2s bus
// in parallel LCD 8bit mode.

// NOTE: D0-D3 need to be on bits 0-3; HS, VS and CLK_EN can be changed
#define BIT_D0 ((uint8_t)(1 << 0))
#define BIT_D1 ((uint8_t)(1 << 1))
#define BIT_D2 ((uint8_t)(1 << 2))
#define BIT_D3 ((uint8_t)(1 << 3))

#define BIT_HS 4
#define BIT_VS 5
#define BIT_CLK_EN 6

#define NUM_COLS 640
#define NUM_ROWS 200

#define NUM_ROW_BYTES ((NUM_COLS / 4) + 4)

#define NUM_FRAMEBUFFERS 2

#define FRAME_SIZE (164 * (NUM_ROWS + 1))

void LCD_ClearScreen(uint8_t pattern);
void LCD_PixelSet(int x, int y, bool set);
void LCD_ClearLine(uint8_t pattern, int ln);
void LCD_LoadBitmap(uint8_t *pImg, uint16_t x, uint16_t y, uint16_t width, uint16_t height, bool transp);

void *LCD_GetFrameBuffer(uint8_t id);
void LCD_SelectFrameBuffer(uint8_t id);
uint8_t LCD_GetFrameBufferSelected(void);

#endif
