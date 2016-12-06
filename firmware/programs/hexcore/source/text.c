#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "imports.h"
#include "font_bin.h"
#include "text.h"

u32* const framebuffer = (u32*)FRAMEBUFFER_ADDRESS;

void clearScreen(u32 color)
{
	int i;
	for(i = 0; i < FRAMEBUFFER_WIDTH * FRAMEBUFFER_HEIGHT; i++)
	{
		framebuffer[i] = color;
	}
}

void drawCharacter(char c, int x, int y, int fg_color, int bg_color)
{
	if (c < 32) return;
	c -= 32;
	
	u8* charData = (u8* )&font_bin[c * 8];
	u8* fb = (u8 *)&framebuffer[x * 4 + y * FRAMEBUFFER_STRIDE];
	
	int i, j, k;
	
	for (i = 0; i < CHAR_SIZE_X; i++)
	{
		for (j = 0; j < CHAR_MULT; j++)
		{
			u8 v = *(charData);
			for (k = 0; k < CHAR_SIZE_Y; k++)
			{
				if (v & 1)
				{
					*(u32 *)fb = fg_color;
					*(u32 *)(fb + 0x04) = fg_color;
				}
				else
				{
					*(u32 *)fb = bg_color;
					*(u32 *)(fb + 0x04) = bg_color;
				}
				v >>= 1;
				fb += 0x08;
			}
			fb += FRAMEBUFFER_STRIDE - CHAR_SIZE * 4;
		}
		charData++;
	}
}

void drawString(char* str, int x, int y, int fg_color, int bg_color)
{
	if(!str) return;
	int k;
	int dx = 0, dy = 0;
	for(k = 0; str[k]; k++)
	{
		if(str[k] >= 32 && str[k] < 128) drawCharacter(str[k], x + dx, y + dy, fg_color, bg_color);
		
		dx += CHAR_SIZE_X / CHAR_MULT;
		
		if(str[k] == '\n')
		{
			dx = 0;
			dy -= CHAR_SIZE_Y / CHAR_MULT;
		}
	}
}

void print(int x, int y, int fg_color, int bg_color, const char *format, ...)
{
    va_list args;
    va_start(args, format);

    static char buffer[0x100];

    vsnprintf(buffer, 0xFF, format, args);
    drawString(buffer, x, y, fg_color, bg_color);

    va_end(args);
}