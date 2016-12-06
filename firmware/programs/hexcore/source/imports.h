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

void usleep(u32 time);
void seeprom_read(u32 offset, u32 size, void* out_buf);
void seeprom_write(u32 offset, u32 size, void* in_buf);

#endif