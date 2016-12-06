#include "imports.h"

void usleep(u32 time)
{
	((void (*const)(u32))MCP_USLEEP)(time);
}

void seeprom_read(u32 offset, u32 size, void* out_buf)
{
	((void (*const)(u32, u32, void*))MCP_SEEPROM_READ)(offset, size, out_buf);
}

void seeprom_write(u32 offset, u32 size, void* in_buf)
{
	((void (*const)(u32, u32, void*))MCP_SEEPROM_WRITE)(offset, size, in_buf);
}

void* memset(void* dst, int val, size_t size)
{
	char* _dst = dst;
	
	int i;
	for(i = 0; i < size; i++) _dst[i] = val;

	return dst;
}

void* (*const _memcpy)(void* dst, void* src, int size) = (void*)MCP_MEMCPY;

void* memcpy(void* dst, const void* src, size_t size)
{
	return _memcpy(dst, (void*)src, size);
}

char* strncpy(char* dst, const char* src, size_t size)
{
	int i;
	for(i = 0; i < size; i++)
	{
		dst[i] = src[i];
		if(src[i] == '\0') return dst;
	}

	return dst;
}

int vsnprintf(char * s, size_t n, const char * format, va_list arg)
{
    return ((int (*const)(char*, size_t, const char *, va_list))MCP_VSNPRINTF)(s, n, format, arg);
}