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

int bsp_query(char *entity, u32 offset, char* param, u32 query_size, void* query_buf)
{
	return ((int (*const)(char*, u32, char*, u32, void*))MCP_BSP_QUERY)(entity, offset, param, query_size, query_buf);
}

int bsp_read(char *entity, u32 offset, char* param, u32 out_size, void* out_buf)
{
	return ((int (*const)(char*, u32, char*, u32, void*))MCP_BSP_READ)(entity, offset, param, out_size, out_buf);
}

int bsp_write(char *entity, u32 offset, char* param, u32 in_size, void* in_buf)
{
	return ((int (*const)(char*, u32, char*, u32, void*))MCP_BSP_WRITE)(entity, offset, param, in_size, in_buf);
}

void flush_dcache(u32 addr, u32 size)
{
	((void (*const)(u32, u32))MCP_FLUSH_DCACHE)(addr, size);
}

int enc_prsh(u32 addr, u32 size, void* iv_buf, u32 iv_size)
{
	return ((int (*const)(u32, u32, void*, u32))MCP_ENC_PRSH)(addr, size, iv_buf, iv_size);
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