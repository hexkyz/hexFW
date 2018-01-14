#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "imports.h"
#include "latte.h"
#include "net_ifmgr_ncl.h"
#include "socket.h"
#include "fsa.h"
#include "svc.h"
#include "text.h"

#define FG_COLOR_REGULAR 	(0x00000000)
#define BG_COLOR_REGULAR 	(0xFF0000FF)
#define BG_COLOR_HIGHLIGHT	(0xFFFFFFFF)

bool server_done;

void init_screen()
{
	clearScreen(0xFF0000FF);
	print(0, 0, FG_COLOR_REGULAR, BG_COLOR_REGULAR, "Welcome to hexFW (v0.0.2) by hexkyz");
}

void print_credits()
{
	// Clear the screen first
	init_screen();
	
	int t = 0x2710;
	
	while (t > 0)
	{		
		print(10, 10, FG_COLOR_REGULAR, BG_COLOR_REGULAR, "Thanks to:");
		print(15, 20, FG_COLOR_REGULAR, BG_COLOR_HIGHLIGHT, "fail0verflow");
		print(15, 30, FG_COLOR_REGULAR, BG_COLOR_HIGHLIGHT, "smealum");
		print(15, 40, FG_COLOR_REGULAR, BG_COLOR_HIGHLIGHT, "yellows8");
		print(15, 50, FG_COLOR_REGULAR, BG_COLOR_HIGHLIGHT, "hykem");
		print(15, 60, FG_COLOR_REGULAR, BG_COLOR_HIGHLIGHT, "naehrwert");
		print(15, 70, FG_COLOR_REGULAR, BG_COLOR_HIGHLIGHT, "plutoo");
		print(15, 80, FG_COLOR_REGULAR, BG_COLOR_HIGHLIGHT, "derrek");
		print(15, 90, FG_COLOR_REGULAR, BG_COLOR_HIGHLIGHT, "libwiiu team");
		t--;
	}
	
	// Clear back the screen
	init_screen();
}

int server_cmd(u32* command_buffer, u32 length)
{
	if (!command_buffer || !length) return -1;

	int out_length = 4;

	switch (command_buffer[0])
	{
		case 0:
			// write
			// [cmd_id][addr]
			{
				void* dst = (void*)command_buffer[1];
				memcpy(dst, &command_buffer[2], length - 8);
			}
			break;
		case 1:
			// read
			// [cmd_id][addr][length]
			{
				void* src = (void*)command_buffer[1];
				length = command_buffer[2];

				memcpy(&command_buffer[1], src, length);
				out_length = length + 4;
			}
			break;
		case 2:
			// svc
			// [cmd_id][svc_id]
			{
				int svc_id = command_buffer[1];
				int size_arguments = length - 8;

				u32 arguments[8];
				memset(arguments, 0x00, sizeof(arguments));
				memcpy(arguments, &command_buffer[2], (size_arguments < 8 * 4) ? size_arguments : (8 * 4));

				// return error code as data
				out_length = 8;
				command_buffer[1] = ((int (*const)(u32, u32, u32, u32, u32, u32, u32, u32))(MCP_SVC_BASE + svc_id * 8))(arguments[0], arguments[1], arguments[2], arguments[3], arguments[4], arguments[5], arguments[6], arguments[7]);
			}
			break;
		case 3:
			// kill
			// [cmd_id]
			{
				server_done = true;
			}
			break;
		case 4:
			// memcpy
			// [dst][src][size]
			{
				void* dst = (void*)command_buffer[1];
				void* src = (void*)command_buffer[2];
				int size = command_buffer[3];

				memcpy(dst, src, size);
			}
			break;
		case 5:
			// repeated-write
			// [address][value][n]
			{
				u32* dst = (u32*)command_buffer[1];
				u32* cache_range = (u32*)(command_buffer[1] & ~0xFF);
				u32 value = command_buffer[2];
				u32 n = command_buffer[3];

				u32 old = *dst;
				int i;
				for (i = 0; i < n; i++)
				{
					if (*dst != old)
					{
						if (*dst == 0x0) old = *dst;
						else
						{
							*dst = value;
							svcFlushDCache(cache_range, 0x100);
							break;
						}
					}
					else
					{
						svcInvalidateDCache(cache_range, 0x100);
						usleep(50);
					}
				}
			}
			break;
		default:
			// Unknown command
			return -2;
			break;
	}

	// No error
	command_buffer[0] = 0x00000000;
	return out_length;
}

void server_handler(int sock)
{
	u32 command_buffer[0x180];

	while (!server_done)
	{
		int ret = recv(sock, command_buffer, sizeof(command_buffer), 0);

		if (ret <= 0) break;

		ret = server_cmd(command_buffer, ret);

		if (ret > 0)
		{
			send(sock, command_buffer, ret, 0);
		}
		else if (ret < 0)
		{
			send(sock, &ret, sizeof(int), 0);
		}
	}

	closesocket(sock);
}

void server_listen()
{
	server_done = false;

	int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	struct sockaddr_in server;

	memset(&server, 0x00, sizeof(server));

	server.sin_family = AF_INET;
	server.sin_port = 1337;
	server.sin_addr.s_addr = 0;

	int ret = bind(sock, (struct sockaddr *)&server, sizeof(server));

	while (!server_done)
	{
		ret = listen(sock, 1);
		
		if (ret >= 0)
		{
			int csock = accept(sock, NULL, NULL);
			server_handler(csock);
		} else usleep(1000);
	}
}

void mount_storage()
{
	int fsa_handle = 0xFFFFFFFF;
	int fsa_mount_res = 0xFFFFFFFF;

	// Initialize /dev/fsa
	while (fsa_handle < 0)
	{
		fsa_handle = fsaInit();
		usleep(1000);
	}

	// Mount the SD card for storage
	while (fsa_mount_res < 0)
	{
		fsa_mount_res = FSA_Mount("/dev/sdcard01", "/vol/storage_sdcard", 0x00000002, NULL, 0);
		usleep(1000);
	}
}

int file_write(char *path, void* data, int size, int count, bool append)
{
	int fsa_file_handle = 0xFFFFFFFF;
	int fsa_write_result = 0xFFFFFFFF;
	
	// Open target file
	if (append)
		FSA_OpenFile(path, "a+", &fsa_file_handle);
	else
		FSA_OpenFile(path, "w", &fsa_file_handle);
	
	// Write target file
	fsa_write_result = FSA_WriteFile(data, size, count, fsa_file_handle, 0x00000002);
	
	// Close target file
	FSA_CloseFile(fsa_file_handle);
	
	return fsa_write_result;
}

void dump_otp()
{
	// Allocate buffer
	void* otp_data = svcAlloc(0xCAFF, 0x400);
	memset(otp_data, 0x00, 0x400);
	
	// Read OTP data
	svcReadOTP(0x00, otp_data, 0x400);
	
	// Dump to SD card
	file_write("/vol/storage_sdcard/otp.bin", otp_data, 0x01, 0x400, false);
	
	// Free buffer
	svcFree(0xCAFF, otp_data);
}

void dump_seeprom()
{
	// Allocate buffer
	void* seeprom_data = svcAlloc(0xCAFF, 0x200);
	memset(seeprom_data, 0x00, 0x200);
	
	// Read SEEPROM data
	seeprom_read(0x00, 0xF8, seeprom_data);
	
	// Dump to SD card
	file_write("/vol/storage_sdcard/seeprom.bin", seeprom_data, 0x01, 0x200, false);
	
	// Free buffer
	svcFree(0xCAFF, seeprom_data);
}

void dump_slc()
{
	int fsa_raw_handle = 0xFFFFFFFF;
	int fsa_raw_open_result = 0xFFFFFFFF;
	int fsa_read_result = 0xFFFFFFFF;
	
	// Allocate temporary buffer
	void* slc_buf = svcAlloc(0xCAFF, 0x1000);
	memset(slc_buf, 0x00, 0x1000);
	
	// Wait for SLC to be mounted
	while (fsa_raw_open_result < 0)
	{
		// Open target device
		fsa_raw_open_result = FSA_RawOpen("/dev/slc01", &fsa_raw_handle);
		usleep(1000);
	}
	
	// Read raw sectors and dump to SD card
	int slc_offset = 0;
	while (slc_offset < 0x20000000)
	{
		// Lock PPC
		ppc_reset();
		
		// Clear buffer
		memset(slc_buf, 0x00, 0x1000);
		
		// Read from target device
		fsa_read_result = FSA_RawRead(slc_buf, 0x01, 0x1000, slc_offset, fsa_raw_handle);
		
		// We've reached the end of the device's memory
		if (fsa_read_result == 0xFFFCFFD5)
			break;
		
		// Print
		print(10, 20, FG_COLOR_REGULAR, BG_COLOR_REGULAR, "SLC");
		print(10, 30, FG_COLOR_REGULAR, BG_COLOR_REGULAR, "Result: 0x%08x Offset: 0x%08x", fsa_read_result, slc_offset);
		
		// Sleep for a while
		usleep(10);
		
		// Write to SD card
		file_write("/vol/storage_sdcard/slc.bin", slc_buf, 0x01, 0x1000, true);
		
		// Increase offset
		slc_offset += 0x1000;
	}
	
	// Close target device
	FSA_RawClose(fsa_raw_handle);
	
	// Free buffer
	svcFree(0xCAFF, slc_buf);
}

void dump_slccmpt()
{
	int fsa_raw_handle = 0xFFFFFFFF;
	int fsa_raw_open_result = 0xFFFFFFFF;
	int fsa_read_result = 0xFFFFFFFF;
	
	// Allocate temporary buffer
	void* slccmpt_buf = svcAlloc(0xCAFF, 0x1000);
	memset(slccmpt_buf, 0x00, 0x1000);
	
	// Wait for SLCCMPT to be mounted
	while (fsa_raw_open_result < 0)
	{
		// Open target device
		fsa_raw_open_result = FSA_RawOpen("/dev/slccmpt01", &fsa_raw_handle);
		usleep(1000);
	}
	
	// Read raw sectors and dump to SD card
	int slccmpt_offset = 0;
	while (slccmpt_offset < 0x20000000)
	{
		// Lock PPC
		ppc_reset();
		
		// Clear buffer
		memset(slccmpt_buf, 0x00, 0x1000);
		
		// Read from target device
		fsa_read_result = FSA_RawRead(slccmpt_buf, 0x01, 0x1000, slccmpt_offset, fsa_raw_handle);
		
		// We've reached the end of the device's memory
		if (fsa_read_result == 0xFFFCFFD5)
			break;
		
		// Print
		print(10, 20, FG_COLOR_REGULAR, BG_COLOR_REGULAR, "SLCCMPT");
		print(10, 30, FG_COLOR_REGULAR, BG_COLOR_REGULAR, "Result: 0x%08x Offset: 0x%08x", fsa_read_result, slccmpt_offset);
		
		// Sleep for a while
		usleep(10);
		
		// Write to SD card
		file_write("/vol/storage_sdcard/slccmpt.bin", slccmpt_buf, 0x01, 0x1000, true);
		
		// Increase offset
		slccmpt_offset += 0x1000;
	}
	
	// Close target device
	FSA_RawClose(fsa_raw_handle);
	
	// Free buffer
	svcFree(0xCAFF, slccmpt_buf);
}

void dump_mlc()
{
	int fsa_raw_handle = 0xFFFFFFFF;
	int fsa_raw_open_result = 0xFFFFFFFF;
	int fsa_read_result = 0xFFFFFFFF;
	
	// Allocate temporary buffer
	void* mlc_buf = svcAlloc(0xCAFF, 0x10 * 0x1000);
	memset(mlc_buf, 0x00, 0x10 * 0x1000);
	
	// Wait for MLC to be mounted
	while (fsa_raw_open_result < 0)
	{
		// Open target device
		fsa_raw_open_result = FSA_RawOpen("/dev/mlc01", &fsa_raw_handle);
		usleep(1000);
	}
	
	// Read raw sectors and dump to SD card
	int mlc_offset = 0;
	while (mlc_offset < 0x20000000)
	{
		// Lock PPC
		ppc_reset();
		
		// Clear buffer
		memset(mlc_buf, 0x00, 0x10 * 0x1000);
		
		// Read from target device
		fsa_read_result = FSA_RawRead(mlc_buf, 0x10, 0x1000, mlc_offset, fsa_raw_handle);
		
		// Print
		print(10, 20, FG_COLOR_REGULAR, BG_COLOR_REGULAR, "MLC");
		print(10, 30, FG_COLOR_REGULAR, BG_COLOR_REGULAR, "Result: 0x%08x Offset: 0x%08x", fsa_read_result, mlc_offset);
		
		// Sleep for a while
		usleep(10);
		
		// Write to SD card
		file_write("/vol/storage_sdcard/mlc.bin", mlc_buf, 0x10, 0x1000, true);
		
		// Increase offset
		mlc_offset += 0x10 * 0x1000;
	}
	
	// Close target device
	FSA_RawClose(fsa_raw_handle);
	
	// Free buffer
	svcFree(0xCAFF, mlc_buf);
}

void dump_ramdisk()
{
	int fsa_raw_handle = 0xFFFFFFFF;
	int fsa_raw_open_result = 0xFFFFFFFF;
	int fsa_read_result = 0xFFFFFFFF;
	
	// Allocate temporary buffer
	void* ramdisk_buf = svcAlloc(0xCAFF, 0x10 * 0x1000);
	memset(ramdisk_buf, 0x00, 0x10 * 0x1000);
	
	// Wait for RAMDISK to be mounted
	while (fsa_raw_open_result < 0)
	{
		// Open target device
		fsa_raw_open_result = FSA_RawOpen("/dev/ramdisk01", &fsa_raw_handle);
		usleep(1000);
	}
	
	// Read raw sectors and dump to SD card
	int ramdisk_offset = 0;
	while (ramdisk_offset < 0x20000000)
	{
		// Lock PPC
		ppc_reset();
		
		// Clear buffer
		memset(ramdisk_buf, 0x00, 0x10 * 0x1000);
		
		// Read from target device
		fsa_read_result = FSA_RawRead(ramdisk_buf, 0x10, 0x1000, ramdisk_offset, fsa_raw_handle);
		
		// Print
		print(10, 20, FG_COLOR_REGULAR, BG_COLOR_REGULAR, "RAMDISK");
		print(10, 30, FG_COLOR_REGULAR, BG_COLOR_REGULAR, "Result: 0x%08x Offset: 0x%08x", fsa_read_result, ramdisk_offset);
		
		// Sleep for a while
		usleep(10);
		
		// Write to SD card
		file_write("/vol/storage_sdcard/ramdisk.bin", ramdisk_buf, 0x10, 0x1000, true);
		
		// Increase offset
		ramdisk_offset += 0x10 * 0x1000;
	}
	
	// Close target device
	FSA_RawClose(fsa_raw_handle);
	
	// Free buffer
	svcFree(0xCAFF, ramdisk_buf);
}

void dump_boot1()
{
	u32 boot1_ancast_magic = *(u32 *)0x1000A000;
	
	// Run boot1hax
	if (boot1_ancast_magic != 0xEFA282D9)
	{
		// RAM vars
		u32 ram_start_addr = 0x10000000;
		u32 ram_test_buf_size = 0x400;
		
		// PRSH vars
		u32 prsh_hdr_offset = ram_start_addr + ram_test_buf_size + 0x5654;
		u32 prsh_hdr_size = 0x1C;
		
		// boot_info vars
		u32 boot_info_name_addr = prsh_hdr_offset + prsh_hdr_size;
		u32 boot_info_name_size = 0x100;
		u32 boot_info_ptr_addr = boot_info_name_addr + boot_info_name_size;
		
		// Calculate PRSH checksum
		u32 checksum_old = 0;
		u32 word_counter = 0;
		
		while (word_counter < 0x20D)
		{
			checksum_old ^= *(u32 *)(prsh_hdr_offset + 0x04 + word_counter * 0x04);
			word_counter++;
		}
		
		// Change boot_info to point inside boot1 memory
		*(u32 *)boot_info_ptr_addr = 0x0D40AC6D;
		
		// Re-calculate PRSH checksum
		u32 checksum = 0;
		word_counter = 0;
		
		while (word_counter < 0x20D)
		{
			checksum ^= *(u32 *)(prsh_hdr_offset + 0x04 + word_counter * 0x04);
			word_counter++;
		}
		
		// Update checksum
		*(u32 *)prsh_hdr_offset = checksum;
		
		// Copy PRSH IV from IOS-MCP
		void* prsh_iv_buf = svcAlloc(0xCAFF, 0x10);
		memset(prsh_iv_buf, 0x00, 0x10);
		memcpy(prsh_iv_buf, (void *)0x050677C0, 0x10);
		
		// Encrypt PRSH
		enc_prsh(0x10000400, 0x7C00, prsh_iv_buf, 0x10);
		
		// Free PRSH IV buffer
		svcFree(0xCAFF, prsh_iv_buf);
		
		// Flush cache
		flush_dcache(0x10000400, 0x7C00);
		
		// Setup MEM1 payload
		kern_write(0x00000000, 0xEA000010);		// B  sub_00000048
		kern_write(0x00000004, 0xDEADC0DE);
		kern_write(0x00000008, 0xDEADC0DE);
		
		// Call read_otp
		kern_write(0x00000048, 0xE59F00F8);		// LDR    R0, [PC,#0x100]
		kern_write(0x0000004C, 0xE59F10F8);		// LDR    R1, [PC,#0x100]
		kern_write(0x00000050, 0xE59F20F8);		// LDR    R2, [PC,#0x100]
		kern_write(0x00000054, 0xE59F30F8);		// LDR    R3, [PC,#0x100]
		kern_write(0x00000058, 0xE12FFF33); 	// BLX    R3
		
		// Call memcpy
		kern_write(0x0000005C, 0xE59F00F8);		// LDR    R0, [PC,#0x100]
		kern_write(0x00000060, 0xE59F10F8);		// LDR    R1, [PC,#0x100]
		kern_write(0x00000064, 0xE59F20F8);		// LDR    R2, [PC,#0x100]
		kern_write(0x00000068, 0xE59F30F8);		// LDR    R3, [PC,#0x100]
		kern_write(0x0000006C, 0xE12FFF33); 	// BLX    R3
		
		// Patch boot_info_ptr
		kern_write(0x00000070, 0xE59F10F8);		// LDR    R1, [PC,#0x100]
		kern_write(0x00000074, 0xE59F20F8);		// LDR    R2, [PC,#0x100]
		kern_write(0x00000078, 0xE5812000); 	// STR    R2, [R1]
		
		// Patch PRSH checksum
		kern_write(0x0000007C, 0xE59F10F8);		// LDR    R1, [PC,#0x100]
		kern_write(0x00000080, 0xE59F20F8);		// LDR    R2, [PC,#0x100]
		kern_write(0x00000084, 0xE5812000); 	// STR    R2, [R1]
		
		// Jump back to boot1
		kern_write(0x00000088, 0xE59FF0F8);		// LDR    PC, [PC,#0x100]
		
		// read_otp data
		kern_write(0x00000148, 0x00000000);			// OTP index
		kern_write(0x0000014C, 0x10009000);			// Buffer address
		kern_write(0x00000150, 0x00000400);		 	// OTP data size
		kern_write(0x00000154, 0x0D4002DC | 0x01);	// boot1 read_otp address
		
		// memcpy data
		kern_write(0x0000015C, 0x1000A000);			// MEM2 address
		kern_write(0x00000160, 0x0D400000);			// boot1 address
		kern_write(0x00000164, 0x0000E200);			// boot1 data size
		kern_write(0x00000168, 0x0D409BDC);			// boot1 memcpy address
		
		// boot_info_ptr data
		kern_write(0x00000170, boot_info_ptr_addr);
		kern_write(0x00000174, 0x10008000);
		
		// PRSH checksum data
		kern_write(0x0000017C, prsh_hdr_offset);
		kern_write(0x00000180, checksum_old);
		
		// Return address
		kern_write(0x00000188, 0x0D40076A | 0x01);	// Return to boot1
		
		// Reset
		svcShutdown(1);
		
		while(1);
	}
	else	// Dump boot1 and OTP from memory
	{
		// Fix up corruption from boot1hax
		*(u32 *)(0x1000A200 + 0xAA75) = 0x0800000D;
		
		// Dump to SD card
		void* otp_buf = svcAlloc(0xCAFF, 0x400);
		void* boot1_buf = svcAlloc(0xCAFF, 0xE000);
		memset(otp_buf, 0x00, 0x400);
		memset(boot1_buf, 0x00, 0xE000);
		
		memcpy(otp_buf, (void *)0x10009000, 0x400);
		memcpy(boot1_buf, (void *)0x1000A200, 0xE000);
		file_write("/vol/storage_sdcard/otp.bin", otp_buf, 0x01, 0x400, false);
		file_write("/vol/storage_sdcard/boot1.bin", boot1_buf, 0x01, 0xE000, false);
		
		svcFree(0xCAFF, otp_buf);
		svcFree(0xCAFF, boot1_buf);
		
		// Clear MEM2 region
		memset((void *)0x10009000, 0x00, 0x10000);
	}
}

void dump(int device)
{
	// Clear the screen first
	init_screen();
	
	// Print message and dump
	if (device == 0)
	{
		print(10, 10, FG_COLOR_REGULAR, BG_COLOR_REGULAR, "Dumping OTP to SD card...");
		dump_otp();
	}
	else if (device == 1)
	{
		print(10, 10, FG_COLOR_REGULAR, BG_COLOR_REGULAR, "Dumping SEEPROM to SD card...");
		dump_seeprom();
	}
	else if (device == 2)
	{
		print(10, 10, FG_COLOR_REGULAR, BG_COLOR_REGULAR, "Dumping SLC to SD card...");
		dump_slc();
	}
	else if (device == 3)
	{
		print(10, 10, FG_COLOR_REGULAR, BG_COLOR_REGULAR, "Dumping SLCCMPT to SD card...");
		dump_slccmpt();
	}
	else if (device == 4)
	{
		print(10, 10, FG_COLOR_REGULAR, BG_COLOR_REGULAR, "Dumping MLC to SD card...");
		dump_mlc();
	}
	else if (device == 5)
	{
		print(10, 10, FG_COLOR_REGULAR, BG_COLOR_REGULAR, "Dumping BOOT1+OTP to SD card...");
		dump_boot1();
	}
	
	// Clear back the screen
	init_screen();
}

void launch_server()
{
	int ncl_handle = 0xFFFFFFFF;
	int socket_handle = 0xFFFFFFFF;
	u16 if_status0 = 0;
	u16 if_status1 = 0;
	
	// Clear the screen
	init_screen();

	print(10, 10, FG_COLOR_REGULAR, BG_COLOR_REGULAR, "Initializing /dev/net/ifmgr/ncl...");
	
	// Initialize /dev/net/ifmgr/ncl
	while (ncl_handle < 0)
	{
		ncl_handle = ifmgrnclInit();
		usleep(1000);
	}

	print(10, 20, FG_COLOR_REGULAR, BG_COLOR_REGULAR, "Polling interfaces...");
	
	// Keep polling the interfaces until we get a reply
	while((if_status0 != 0x01) && (if_status1 != 0x01))
	{
		IFMGRNCL_GetInterfaceStatus(0, &if_status0);
		IFMGRNCL_GetInterfaceStatus(1, &if_status1);
		usleep(1000);
	}

	print(10, 30, FG_COLOR_REGULAR, BG_COLOR_REGULAR, "Initializing /dev/socket...");
	
	// Initialize /dev/socket
	while (socket_handle < 0)
	{
		socket_handle = socketInit();
		usleep(1000);
	}

	print(10, 40, FG_COLOR_REGULAR, BG_COLOR_REGULAR, "Launching wupserver...");
	
	// Wait a while
	usleep(5 * 1000 * 1000);
	
	// Start listening
	server_listen();
}

int get_btn()
{
	// Send "SystemEventFlag" (0x41) to SMC (0x50 slave)
	u8 reg = 0x41;
	i2c_write(0x50, &reg, 0x01);
		
	u8 result = 0xFF;
	i2c_read(0x50, &result, 0x01);
	
	if (result == 0x40)			// POWER was pressed
		return 1;
	else if (result == 0x20)	// EJECT was pressed
		return 2;
	else
		return 0;
}

u32 read_rtc(u32 offset)
{
	// Register device 0x01 (RTC) to EXI0
	exi_reg(0x00, 0x01, 0x00);
		
	// Write RTC command
	u32 rtc_cmd = offset;
	exi_rw(0x00, &rtc_cmd, 0x04, 0x01);
		
	// Wait for the transfer to complete
	exi_poll(0x00);
		
	// Read reply from command
	u32 reply = 0xFFFFFFFF;
	exi_rw(0x00, &reply, 0x04, 0x00);
	
	// Wait for the transfer to complete
	exi_poll(0x00);
	
	// Reset EXI0
	exi_reset(0x00);
	
	return reply;
}

void write_rtc(u32 offset, u32 data)
{
	// Register device 0x01 (RTC) to EXI0
	exi_reg(0x00, 0x01, 0x00);
		
	// Write RTC command (ORed with write flag)
	u32 rtc_cmd = offset | 0x80000000;
	exi_rw(0x00, &rtc_cmd, 0x04, 0x01);
		
	// Wait for the transfer to complete
	exi_poll(0x00);
		
	// Write data
	u32 rtc_data = data;
	exi_rw(0x00, &rtc_data, 0x04, 0x01);
	
	// Wait for the transfer to complete
	exi_poll(0x00);
	
	// Reset EXI0
	exi_reset(0x00);
}

void test_exi()
{
	// Init EXI
	exi_init();
	
	// Send command 0x21000400 (RTC_UNK) in read mode
	u32 unk_reply = read_rtc(0x21000400);
	
	while (1)
	{
		// Send command 0x21000C00 (RTC_CONTROL0) in read mode
		u32 ctrl0_reply = read_rtc(0x21000C00);
	
		//Send command 0x21000D00 (RTC_CONTROL1) in write mode
		u32 ctrl1_reply = read_rtc(0x21000D00);
		
		print(10, 10, FG_COLOR_REGULAR, BG_COLOR_HIGHLIGHT, "CMD: 0x%08x Reply: 0x%08x", 0x21000400, unk_reply);
		print(10, 20, FG_COLOR_REGULAR, BG_COLOR_HIGHLIGHT, "CMD: 0x%08x Reply: 0x%08x", 0x21000C00, ctrl0_reply);
		print(10, 30, FG_COLOR_REGULAR, BG_COLOR_HIGHLIGHT, "CMD: 0x%08x Reply: 0x%08x", 0x21000D00, ctrl1_reply);
	}
}

void test_bsp()
{
	void* test_buf = svcAlloc(0xCAFF, 0x100);
	memset(test_buf, 0x00, 0x100);
	
	bsp_query("SDIO", 0, "SlotProperties", 0x30, test_buf + 0x00);
	bsp_query("SDIO", 1, "SlotProperties", 0x30, test_buf + 0x30);
	bsp_query("SDIO", 2, "SlotProperties", 0x30, test_buf + 0x60);
	bsp_query("SDIO", 3, "SlotProperties", 0x30, test_buf + 0x90);
	bsp_query("SDIO", 4, "SlotProperties", 0x30, test_buf + 0xC0);
	
	file_write("/vol/storage_sdcard/test.bin", test_buf, 0x01, 0x100, false);	
	print(10, 10, FG_COLOR_REGULAR, BG_COLOR_HIGHLIGHT, "DONE");
	
	svcFree(0xCAFF, test_buf);
	
	while (1);
}

void _main()
{
	// Clear the screen and set welcome message
	init_screen();
	
	// Mount the SD card for storage
	mount_storage();
	
	int sel_cnt = 0;
	bool do_run = true;
	bool do_exec = false;
	
	while (do_run)
	{
		// Lock PPC
		ppc_reset();
	
		// Get button presses
		int btn = get_btn();
		
		// Check if the user pressed POWER
		if (btn == 1)
			do_exec = true;
		else if (btn == 2)	// Check if the user pressed EJECT
			sel_cnt++;
		
		// Check options' list boundaries
		// NOTE: Using multiples of 2 compensates
		// the button state's transition speed
		if ((sel_cnt > 16) && (sel_cnt <= 18))
			sel_cnt = 0;
		
		if (sel_cnt <= 0)
			print(10, 10, FG_COLOR_REGULAR, BG_COLOR_HIGHLIGHT, "- Dump OTP");
		else
			print(10, 10, FG_COLOR_REGULAR, BG_COLOR_REGULAR, "- Dump OTP");
		
		if ((sel_cnt > 0) && (sel_cnt <= 2))
			print(10, 20, FG_COLOR_REGULAR, BG_COLOR_HIGHLIGHT, "- Dump SEEPROM");
		else
			print(10, 20, FG_COLOR_REGULAR, BG_COLOR_REGULAR, "- Dump SEEPROM");
		
		if ((sel_cnt > 2) && (sel_cnt <= 4))
			print(10, 30, FG_COLOR_REGULAR, BG_COLOR_HIGHLIGHT, "- Dump SLC");
		else
			print(10, 30, FG_COLOR_REGULAR, BG_COLOR_REGULAR, "- Dump SLC");

		if ((sel_cnt > 4) && (sel_cnt <= 6))
			print(10, 40, FG_COLOR_REGULAR, BG_COLOR_HIGHLIGHT, "- Dump SLCCMPT");
		else
			print(10, 40, FG_COLOR_REGULAR, BG_COLOR_REGULAR, "- Dump SLCCMPT");
		
		if ((sel_cnt > 6) && (sel_cnt <= 8))
			print(10, 50, FG_COLOR_REGULAR, BG_COLOR_HIGHLIGHT, "- Dump MLC");
		else
			print(10, 50, FG_COLOR_REGULAR, BG_COLOR_REGULAR, "- Dump MLC");
		
		if ((sel_cnt > 8) && (sel_cnt <= 10))
			print(10, 60, FG_COLOR_REGULAR, BG_COLOR_HIGHLIGHT, "- Dump BOOT1+OTP");
		else
			print(10, 60, FG_COLOR_REGULAR, BG_COLOR_REGULAR, "- Dump BOOT1+OTP");
		
		if ((sel_cnt > 10) && (sel_cnt <= 12))
			print(10, 70, FG_COLOR_REGULAR, BG_COLOR_HIGHLIGHT, "- Launch wupserver");
		else
			print(10, 70, FG_COLOR_REGULAR, BG_COLOR_REGULAR, "- Launch wupserver");
		
		if ((sel_cnt > 12) && (sel_cnt <= 14))
			print(10, 80, FG_COLOR_REGULAR, BG_COLOR_HIGHLIGHT, "- Shutdown");
		else
			print(10, 80, FG_COLOR_REGULAR, BG_COLOR_REGULAR, "- Shutdown");
		
		if ((sel_cnt > 14) && (sel_cnt <= 16))
			print(10, 90, FG_COLOR_REGULAR, BG_COLOR_HIGHLIGHT, "- Credits");
		else
			print(10, 90, FG_COLOR_REGULAR, BG_COLOR_REGULAR, "- Credits");
		
		// Execute the selected action
		if (do_exec)
		{
			switch (sel_cnt)
			{
				case 0:
					dump(0);
					break;
				case 1:
				case 2:
					dump(1);
					break;
				case 3:
				case 4:
					dump(2);
					break;
				case 5:
				case 6:
					dump(3);
					break;
				case 7:
				case 8:
					dump(4);
					break;
				case 9:
				case 10:
					dump(5);
					break;
				case 11:
				case 12:
					launch_server();
					break;
				case 13:
				case 14:
					svcShutdown(0);
					break;
				case 15:
				case 16:
					print_credits();
					break;
			}
			do_exec = false;
		}
	}
}