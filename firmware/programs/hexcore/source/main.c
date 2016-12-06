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
	print(0, 0, FG_COLOR_REGULAR, BG_COLOR_REGULAR, "Welcome to hexFW (v0.0.1) by hexkyz");
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
	fsa_write_result = FSA_WriteFile(data, size, count, fsa_file_handle, 0);
	
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
	int fsa_read_result = 0xFFFFFFFF;
	
	// Allocate temporary buffer
	void* slc_buf = svcAlloc(0xCAFF, 0x1000);
	memset(slc_buf, 0x00, 0x1000);
	
	// Open target device
	FSA_RawOpen("/dev/slc01", &fsa_raw_handle);
	
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
	int fsa_read_result = 0xFFFFFFFF;
	
	// Allocate temporary buffer
	void* slccmpt_buf = svcAlloc(0xCAFF, 0x1000);
	memset(slccmpt_buf, 0x00, 0x1000);
	
	// Open target device
	FSA_RawOpen("/dev/slccmpt01", &fsa_raw_handle);
	
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
		print(10, 10, FG_COLOR_REGULAR, BG_COLOR_REGULAR, "Dumping SLC/SLCCMPT to SD card...");
		dump_slc();
		dump_slccmpt();
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
		u32 resets = kern_read(0x0D800194);
		kern_write(0x0D800194, (resets & ~(0x210)));
	
		int btn = get_btn();
		
		// Check if the user pressed POWER
		if (btn == 1)
			do_exec = true;
		else if (btn == 2)	// Check if the user pressed EJECT
			sel_cnt++;
		
		// Check options' list boundaries
		// NOTE: Using multiples of 2 compensates
		// the button state's transition speed
		if ((sel_cnt > 10) && (sel_cnt <= 12))
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
			print(10, 30, FG_COLOR_REGULAR, BG_COLOR_HIGHLIGHT, "- Dump SLC/SLCCMPT");
		else
			print(10, 30, FG_COLOR_REGULAR, BG_COLOR_REGULAR, "- Dump SLC/SLCCMPT");
		
		if ((sel_cnt > 4) && (sel_cnt <= 6))
			print(10, 40, FG_COLOR_REGULAR, BG_COLOR_HIGHLIGHT, "- Launch wupserver");
		else
			print(10, 40, FG_COLOR_REGULAR, BG_COLOR_REGULAR, "- Launch wupserver");

		if ((sel_cnt > 6) && (sel_cnt <= 8))
			print(10, 50, FG_COLOR_REGULAR, BG_COLOR_HIGHLIGHT, "- Shutdown");
		else
			print(10, 50, FG_COLOR_REGULAR, BG_COLOR_REGULAR, "- Shutdown");
		
		if ((sel_cnt > 8) && (sel_cnt <= 10))
			print(10, 60, FG_COLOR_REGULAR, BG_COLOR_HIGHLIGHT, "- Credits");
		else
			print(10, 60, FG_COLOR_REGULAR, BG_COLOR_REGULAR, "- Credits");
		
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
					launch_server();
					break;
				case 7:
				case 8:
					svcShutdown(0);
					break;
				case 9:
				case 10:
					print_credits();
					break;
			}
			do_exec = false;
		}
	}
}