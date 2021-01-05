#ifndef IMPORTS_H
#define IMPORTS_H

#include <stdlib.h>
#include <stdarg.h>
#include "types.h"

#define MCP_SVC_BASE ((void*)0x050567EC)
#define MCP_USLEEP ((void*)0x050564E4)
#define MCP_MEMCPY ((void*)0x05054E54)
#define MCP_VSNPRINTF ((void*)0x05055C40)
#define MCP_SEEPROM_READ ((void*)0x050590C8)
#define MCP_SEEPROM_WRITE ((void*)0x050591C8)
#define MCP_BSP_QUERY ((void*)0x050596B8)
#define MCP_BSP_READ ((void*)0x05059568)
#define MCP_BSP_WRITE ((void*)0x05059570)
#define MCP_FLUSH_DCACHE ((void*)0x05059178)
#define MCP_ENC_PRSH ((void*)0x0500A611)
#define MCP_LOAD_BOOT1_PARAMS ((void*)0x05006A29)

void usleep(u32 time);
void seeprom_read(u32 offset, u32 size, void* out_buf);
void seeprom_write(u32 offset, u32 size, void* in_buf);
int bsp_query(char *entity, u32 offset, char* param, u32 query_size, void* query_buf);
int bsp_read(char *entity, u32 offset, char* param, u32 out_size, void* out_buf);
int bsp_write(char *entity, u32 offset, char* param, u32 in_size, void* in_buf);
void flush_dcache(u32 addr, u32 size);
int enc_prsh(u32 addr, u32 size, void* iv_buf, u32 iv_size);
void load_boot1_params();

#endif