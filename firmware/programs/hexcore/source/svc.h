#ifndef SVC_H
#define SVC_H

#include "types.h"

#define SHUTDOWN_POWER_OFF     0
#define SHUTDOWN_REBOOT 	   1

typedef struct
{
	void* ptr;
	u32 len;
	u32 unk;
} iovec_s;

int svcReadOTP(u32 word_offset, void* out_buf, u32 size);
void* svcAlloc(u32 heapid, u32 size);
void* svcAllocAlign(u32 heapid, u32 size, u32 align);
void svcFree(u32 heapid, void* ptr);
int svcOpen(char* name, int mode);
int svcClose(int fd);
int svcIoctl(int fd, u32 request, void* input_buffer, u32 input_buffer_len, void* output_buffer, u32 output_buffer_len);
int svcIoctlv(int fd, u32 request, u32 vector_count_in, u32 vector_count_out, iovec_s* vector);
int svcInvalidateDCache(void* address, u32 size);
int svcFlushDCache(void* address, u32 size);
void svcShutdown(int action);
void svcReset();
int svcRW(int mode, u32 addr, u32 data);

#endif