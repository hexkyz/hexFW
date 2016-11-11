.arm.big

.open "patches/sections/0x05000000.bin","patches/patched_sections/0x05000000.bin",0x05000000

CODE_SECTION_BASE equ 0x05000000
CODE_SECTION_SIZE equ 0x000598F0
CODE_BASE equ (CODE_SECTION_BASE + CODE_SECTION_SIZE)

RODATA_SECTION_BASE equ 0x05060000
RODATA_SECTION_SIZE equ 0x0000FFC4
RODATA_BASE equ (RODATA_SECTION_BASE + RODATA_SECTION_SIZE)

DATA_SECTION_BASE equ 0x05070000
DATA_SECTION_SIZE equ 0x00003420
DATA_BASE equ (DATA_SECTION_BASE + DATA_SECTION_SIZE)

BSS_SECTION_BASE equ 0x05074000
BSS_SECTION_SIZE equ 0x00048574
BSS_BASE equ (BSS_SECTION_BASE + BSS_SECTION_SIZE)

MCP_FSA_OPEN_THUMB equ 0x05059160
MCP_FSA_MOUNT_THUMB equ 0x05059530
MCP_SYSLOG_OUTPUT_THUMB equ 0x05059140

MCP_SVC_CREATETHREAD equ 0x050567EC
MCP_SVC_STARTTHREAD equ 0x05056824

NEW_TIMEOUT equ (0xFFFFFFFF)

; Fix MCP timeout
.org 0x05022474
	.word NEW_TIMEOUT

; Hook main thread
.org 0x05056718
	.arm
	bl mcpMainThread_hook

; Patch IOSC_VerifyPubkeySign (also patches OS launch sig check)
.org 0x05052C44
	.arm
	mov r0, #0
	bx lr

; Replace call to syslog_output with os_mount_hook
.org 0x050282AE
	.thumb
	bl os_mount_hook

; Patch pointer to fw.img loader path
.org 0x050284D8
	.word fw_img_path

.org CODE_BASE
	.arm
	mcpMainThread_hook:
		mov r11, r0
		push {r0-r11,lr}
		sub sp, #8
		
		; Create our own thread
		mov r0, #0x78
		str r0, [sp] ; Thread's priority
		mov r0, #1
		str r0, [sp, #4] ; Thread's flags
		ldr r0, =thread_entry ; Thread's entry
		mov r1, #0 ; Thread's args (NULL)
		ldr r2, =thread_stacktop ; Thread's stack top
		mov r3, #thread_stacktop - thread_stack ; Thread's stack size
		bl MCP_SVC_CREATETHREAD

		; Start our thread
		cmp r0, #0
		blge MCP_SVC_STARTTHREAD

		; Save our thread's ID
		ldr r1, =0x050BCFFC
		str r0, [r1]

		add sp, #8
		pop {r0-r11,pc}

	.thumb
	os_mount_hook:
		bx pc
		.align 0x4
		.arm
		push {r0-r3,lr}
		sub sp, #8

		; Emulate replaced syslog_output call
		bl MCP_SYSLOG_OUTPUT_THUMB
		
		; Open /dev/fsa
		mov r0, #0
		bl MCP_FSA_OPEN_THUMB

		; Mount OS drive
		ldr r1, =os_device_path
		ldr r2, =os_mount_path
		mov r3, #0
		str r3, [sp]
		str r3, [sp, #4]
		bl MCP_FSA_MOUNT_THUMB

		add sp, #8
		pop {r0-r3,pc}
		
		os_device_path:
			.ascii "/dev/sdcard01"
			.byte 0x00
		
		os_mount_path:
			.ascii "/vol/sdcard"
			.byte 0x00
			.align 0x4

	fw_img_path:
		.ascii "/vol/sdcard"
		.byte 0x00
		.align 0x4

	.pool

.Close

.open "patches/sections/0x05100000.bin","patches/patched_sections/0x05100000.bin",0x05100000

; Append our thread's code
.org 0x05116000
	thread_entry:
		.incbin "programs/wupserver/wupserver.bin"
	.align 0x100

.Close

.create "patches/patched_sections/0x05074000.bin",0x05074000

.org BSS_BASE

.org 0x050BD000
	thread_stack:
		.fill ((thread_stack + 0x1000) - .), 0x00
	thread_stacktop:
	thread_bss:
		.fill ((thread_bss + 0x2000) - .), 0x00

.Close
