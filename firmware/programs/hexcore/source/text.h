#ifndef TEXT_H
#define TEXT_H

#include "types.h"

#define FRAMEBUFFER_ADDRESS (0x14000000 + 0x38C0000)
#define FRAMEBUFFER_STRIDE (0xE00)
#define FRAMEBUFFER_STRIDE_WORDS (FRAMEBUFFER_STRIDE >> 2)

#define FRAMEBUFFER_WIDTH (896)
#define FRAMEBUFFER_HEIGHT (504)

#define CHAR_SIZE_X (8)
#define CHAR_SIZE_Y (8)

#define CHAR_MULT (2)
#define CHAR_SIZE (8 * CHAR_MULT)

void clearScreen(u32 color);
void drawString(char* str, int x, int y, int fg_color, int bg_color);
void print(int x, int y, int fg_color, int bg_color, const char *format, ...);

#endif