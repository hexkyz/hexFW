.arm.big

.open "patches/sections/0x10700000.bin","patches/patched_sections/0x10700000.bin",0x10700000

CODE_SECTION_BASE equ 0x10700000
CODE_SECTION_SIZE equ 0x000F81C4
CODE_BASE equ (CODE_SECTION_BASE + CODE_SECTION_SIZE)

RODATA_SECTION_BASE equ 0x10800000
RODATA_SECTION_SIZE equ 0x00033B6C
RODATA_BASE equ (RODATA_SECTION_BASE + RODATA_SECTION_SIZE)

DATA_SECTION_BASE equ 0x10834000
DATA_SECTION_SIZE equ 0x000005D0
DATA_BASE equ (DATA_SECTION_BASE + DATA_SECTION_SIZE)

BSS_SECTION_BASE equ 0x10835000
BSS_SECTION_SIZE equ 0x01406554
BSS_BASE equ (BSS_SECTION_BASE + BSS_SECTION_SIZE)

FRAMEBUFFER_ADDRESS equ (0x14000000 + 0x38C0000)
FRAMEBUFFER_STRIDE equ (0xE00)
CHARACTER_MULT equ (2)
CHARACTER_SIZE equ (8 * CHARACTER_MULT)

USB_BASE_SECTORS equ (0x2720000)
SLC_BASE_SECTORS equ (0x2F20000)
SLCCMPT_BASE_SECTORS equ (0x3020000)
MLC_BASE_SECTORS equ (0x3200000)
SYSLOG_BASE_SECTORS equ (0x6D00000)
DUMPDATA_BASE_SECTORS equ (SYSLOG_BASE_SECTORS + (0x40000 / 0x200))

FS_SNPRINTF equ 0x107F5FB4
FS_MEMCPY equ 0x107F4F7C
FS_MEMSET equ 0x107F5018
FS_SYSLOG_OUTPUT equ 0x107F0C84
FS_SLEEP equ 0x1071D668
FS_GETMDDEVICEBYID equ 0x107187C4
FS_SDIO_DOREADWRITECOMMAND equ 0x10718A8C
FS_SDIO_DOCOMMAND equ 0x1071C958
FS_MMC_DEVICEINITSOMETHING equ 0x1071992C
FS_MMC_DEVICEINITSOMETHING2 equ 0x1071A4A4
FS_MMC_DEVICEINITSOMETHING3 equ 0x10719F60
FS_SVC_CREATEMUTEX equ 0x107F6BBC
FS_SVC_ACQUIREMUTEX equ 0x107F6BC4
FS_SVC_RELEASEMUTEX equ 0x107F6BCC
FS_SVC_DESTROYMUTEX equ 0x107F6BD4
FS_USB_READ equ 0x1077F1C0
FS_USB_WRITE equ 0x1077F35C
FS_SLC_READ1 equ 0x107B998C
FS_SLC_READ2 equ 0x107B98FC
FS_SLC_WRITE1 equ 0x107B9870
FS_SLC_WRITE2 equ 0x107B97E4
FS_SLC_HANDLEMESSAGE equ 0x107B9DE4
FS_MLC_READ1 equ 0x107DC760
FS_MLC_READ2 equ 0x107DCDE4
FS_MLC_WRITE1 equ 0x107DC0C0
FS_MLC_WRITE2 equ 0x107DC73C
FS_SDCARD_READ1 equ 0x107BDDD0
FS_SDCARD_WRITE1 equ 0x107BDD60
FS_ISFS_READWRITEBLOCKS equ 0x10720324
FS_CRYPTO_HMAC equ 0x107F3798
FS_RAW_READ1 equ 0x10732BC0
FS_REGISTERMDPHYSICALDEVICE equ 0x10718860

FS_MMC_SDCARD_STRUCT equ (0x1089B9F8)
FS_MMC_MLC_STRUCT equ (0x1089B948)

FS_DK_INIT equ 0x10712898
FS_ODM_INIT equ 0x10734008
FS_MAIN_INIT equ 0x10704184
FS_SAL_INIT equ 0x10732EA8
FS_RAMDISK_INIT equ 0x107F691C
FS_SVC_THREAD equ 0x10700384
FS_REGISTER_DEVICE equ 0x10700204

FS_DEVICE_STRUCTS equ 0x108001CC
FS_DEVICE_HANDLES equ 0x1091C2EC
	
; Redirect syslog to SD card 
.org 0x107F0B68
	bl dump_syslog_hook

; Patch selective logging function
.org 0x107F5720
	b FS_SYSLOG_OUTPUT

; Hijack FS SVC thread after device registration takes place
.org FS_REGISTER_DEVICE + 0x90
	b dev_reg_hook

.org CODE_BASE
; Initialize the SD card and dump the flash memory
dev_reg_hook:
	push {r0}
	
	; Check if we just registered /dev/mmc (FS_DEVICE_STRUCTS + 0x10 * 0x0D)
	ldr r0, [r4, #0x8]
	cmp r0, #0xD
	bne dev_reg_hook_fla

	; Initialize the SD card
	bl sdcard_init

	; Prepare screen for hello message
	bl clear_screen

	mov r0, #20		; x
	mov r1, #20		; y
	adr r2, dev_reg_hook_hello
	ldr r3, =0x050BCFFC	; Our IOS-MCP thread ID
	ldr r3, [r3]
	bl _printf

	dev_reg_hook_fla:
		; Check if we just registered /dev/fla (FS_DEVICE_STRUCTS + 0x10 * 0x07)
		ldr r0, [r4, #0x8]
		cmp r0, #0x7
		bne dev_reg_hook_end

		b dump_flash

	dev_reg_hook_end:
		pop {r0,r4-r8,pc}
	
	dev_reg_hook_hello:
		.ascii "welcome to redNAND %08X"
		.byte 0x00
		.align 0x4
		
; Redirect the syslog
dump_syslog_hook:
	push {r0}
	bl dump_syslog
	pop {r0,r4-r8,r10,pc}

; Dump flash memory	
dump_flash:
	push {lr}
	
	; Initialize the MLC
	; bl mlc_init
	
	; Dump SLC
	mov r0, #0xE ; SLC device ID
	adr r1, slc_format
	ldr r2, =SLC_BASE_SECTORS
	bl dump_slc
	
	; Dump SLCCMPT
	mov r0, #0xF ; SLCCMPT device ID
	adr r1, slccmpt_format
	ldr r2, =SLCCMPT_BASE_SECTORS
	bl dump_slc
	
	; Dump MLC
	; bl dump_mlc
	
	; Crash
	mov r0, #0
	ldr r0, [r0]
	; pop {pc}
	
; Initialize the MLC
mlc_init:
	push {lr}

	; Set MLC init status flags
	ldr r0, =FS_MMC_MLC_STRUCT
	ldr r1, [r0, #0x24]
	orr r1, #0x20
	str r1, [r0, #0x24]
	ldr r1, [r0, #0x28]
	bic r1, #0x4
	str r1, [r0, #0x28]

	pop {pc}

; Initialize the SD card
sdcard_init:
	push {lr}

	; Create and store access mutex
	mov r0, #1
	mov r1, #1
	bl FS_SVC_CREATEMUTEX
	ldr r1, =sdcard_access_mutex
	str r0, [r1]

	; Set the offset for dumping data
	ldr r1, =dumpdata_offset
	mov r0, #0
	str r0, [r1]

	; Wait for /dev/mmc to finish initializing the SD card
	mov r0, #1000
	bl FS_SLEEP

	; Set SD card init status flags
	ldr r0, =FS_MMC_SDCARD_STRUCT
	ldr r1, [r0, #0x24]
	orr r1, #0x20
	str r1, [r0, #0x24]
	ldr r1, [r0, #0x28]
	bic r1, #0x4
	str r1, [r0, #0x28]

	pop {pc}

; r0 : 0 = write, not 0 = read
; r1 : data_ptr
; r2 : cnt
; r3 : block_size
; sp_arg0 : offset_blocks
; sp_arg1 : out_callback_arg2
; sp_arg2 : device_id
sdcard_readwrite:
	sdcard_readwrite_stackframe_size equ (4 * 4 + 12 * 4)
	
	push {r4,r5,r6,lr}
	sub sp, #12 * 4

	; Pointer for command params
	add r4, sp, #0xC
	
	; Pointer for read/write mutex
	add r5, sp, #0x4
	
	; Load offset_blocks
	ldr r6, [sp, #sdcard_readwrite_stackframe_size]
	
	; Save our params to stack
	push {r0,r1,r2,r3}
	
	; Acquire the SD card access mutex
	ldr r0, =sdcard_access_mutex
	ldr r0, [r0]
	mov r1, #0
	bl FS_SVC_ACQUIREMUTEX
	
	; Create a mutex to synchronize with the end of read/write
	mov r0, #1
	mov r1, #1
	bl FS_SVC_CREATEMUTEX
	str r0, [r5]	; Store our read/write mutex
	
	; Acquire the read/write mutex
	mov r1, #0
	bl FS_SVC_ACQUIREMUTEX
	
	; Restore our params
	pop {r0,r1,r2,r3}

	; We must adjust block_size to be equal to sector_size (0x200)
	sdcard_readwrite_block_size_adjust:
		cmp r3, #0x200
		movgt r3, r3, lsr 1 ; block_size >>= 1;
		movgt r2, r2, lsl 1 ; cnt <<= 1;
		movgt r6, r6, lsl 1 ; offset_blocks <<= 1;
		bgt sdcard_readwrite_block_size_adjust

	; Build the read/write command
	str r2, [r4, #0x00] ; Store cnt
	str r3, [r4, #0x04] ; Store block_size
	cmp r0, #0			; Check if it's a read or write operation
	movne r0, #0x3 		; Read is 0x03
	str r0, [r4, #0x08] ; Store command type
	str r1, [r4, #0x0C] ; Store data_ptr
	mov r0, #0
	str r0, [r4, #0x10] ; Store offset_high (0)
	str r6, [r4, #0x14] ; Store offset_low
	str r0, [r4, #0x18] ; Store callback (null)
	str r0, [r4, #0x1C] ; Store callback_arg (null)
	mvn r0, #0
	str r0, [r4, #0x20] ; Store 0xFFFFFFFF

	; Load params
	ldr r0, [sp, #sdcard_readwrite_stackframe_size + 0x8] ; device_id
	mov r1, r4 ; Pointer to the read/write command struct
	mov r2, r6 ; offset_blocks
	adr r3, sdcard_readwrite_callback ; callback (to release the mutex)
	str r5, [sp] ; callback_arg (pointer for read/write mutex)

	; Call the FS read/write function
	bl FS_SDIO_DOREADWRITECOMMAND
	
	; Check for success
	mov r4, r0
	cmp r0, #0
	bne sdcard_readwrite_skip_wait

	; The operation is done, now wait for the callback
	ldr r0, [r5]	; Read/write mutex
	mov r1, #0
	bl FS_SVC_ACQUIREMUTEX
	ldr r0, [r5, #0x4]
	ldr r1, [sp, #sdcard_readwrite_stackframe_size + 0x4]	; out_callback_arg2
	cmp r1, #0
	strne r0, [r1]
	
	sdcard_readwrite_skip_wait:
		; Release the read/write mutex
		ldr r0, [r5]
		bl FS_SVC_DESTROYMUTEX
	
		; Release the access mutex
		ldr r0, =sdcard_access_mutex
		ldr r0, [r0]
		bl FS_SVC_RELEASEMUTEX

		; Return
		mov r0, r4
		add sp, #12 * 4
		pop {r4,r5,r6,pc}

	; Callback to release the mutex
	sdcard_readwrite_callback:
		str r1, [r0, #4]
		ldr r0, [r0]
		b FS_SVC_RELEASEMUTEX
		
; r0 : data_ptr
; r1 : size
; r2 : offset_blocks
write_data_offset:
	push {r1,r2,r3,r4,lr}
	
	; Write data from offset to SD card
	mov r3, r1, lsr 9 ; size /= 0x200
	cmp r3, #0
	moveq r3, #1
	mov r1, r0 ; data_ptr
	str r2, [sp] ; offset
	mov r0, #0
	str r0, [sp, #0x4] ; out_callback_arg2
	mov r0, #0xDA
	str r0, [sp, #0x8] ; device_id
	mov r0, #0 ; Write command
	mov r2, r3 ; num_sectors
	mov r3, #0x200 ; block_size
	bl sdcard_readwrite
	
	; Return
	add sp, #0xC
	pop {r4,pc}

; Dump the syslog to the SD card
dump_syslog:
	push {r1,r2,r3,lr}
	
	; Copy MCP's syslog into our buffer
	ldr r0, =syslog_buffer
	ldr r1, =0x05095ECC ; syslog address
	ldr r1, [r1]
	mov r2, #0x40000
	bl FS_MEMCPY
	
	; Write to the SD card
	ldr r0, =SYSLOG_BASE_SECTORS ; offset_sectors
	str r0, [sp]
	mov r0, #0
	str r0, [sp, #0x4] ; out_callback_arg2
	mov r0, #0xDA
	str r0, [sp, #0x8] ; device_id
	mov r0, #0 ; write
	ldr r1, =syslog_buffer
	mov r2, #0x200 ; num_sectors (0x40000 bytes)
	mov r3, #0x200 ; block_size
	bl sdcard_readwrite
	
	; Return
	add sp, #0xC
	pop {pc}

; r0 : device_id
getPhysicalDeviceHandle:
	ldr r1, =FS_DEVICE_HANDLES
	mov r2, #0x204
	mla r1, r2, r0, r1
	ldrh r1, [r1, #6]
	orr r0, r1, r0, lsl 16
	bx lr

; r0 : dst
; r1 : offset
; r2 : sectors
; r3 : device_id
read_slc:
	push {lr}
	sub sp, #0xC
	
	str r0, [sp] ; outptr
	mov r0, #0
	str r0, [sp, #4] ; callback (null)
	str r0, [sp, #8] ; callback_arg (null)
	
	; Save our args in the stack
	push {r1-r3}
	
	; Grab the SLC device handle
	mov r0, r3	; device_id
	bl getPhysicalDeviceHandle
	
	; Restore our args
	pop {r1-r3}
	
	// Call FS raw read function
	mov r3, r2 ; cnt
	mov r2, r1 ; offset_low
	mov r1, #0 ; offset_high
	BL FS_RAW_READ1
	
	; Return
	add sp, #0xC
	pop {pc}
	
; Dump the SLC
dump_slc:
	push {r4-r7,lr}
	
	; Grab args
	mov r4, #0
	mov r5, r0
	mov r6, r1
	mov r7, r2

	; Sleep a while
	mov r0, #1000
	bl FS_SLEEP

	dump_slc_loop:
		; Print args
		mov r3, r4
		mov r0, #20
		mov r1, #0
		mov r2, r6
		bl _printf

		; Read the memory into our buffer
		ldr r0, =sdcard_read_buffer
		mov r1, r4
		mov r2, #0x80
		add r4, r2
		mov r3, r5
		bl read_slc
	
		; Sleep again
		mov r0, #10
		bl FS_SLEEP

		; Write the data to the SD card
		ldr r0, =sdcard_read_buffer
		mov r1, #0x40000
		mov r2, r7
		add r7, r7, r1, lsr 9
		bl write_data_offset

		; Loop until we dumped everything
		cmp r4, #0x40000
		blt dump_slc_loop

	pop {r4-r7,pc}

	slc_format:
	.ascii "slc     = %08X"
	.byte 0x00
	.align 4

	slccmpt_format:
	.ascii "slccmpt = %08X"
	.byte 0x00
	.align 4

; r0 : data_ptr
; r1 : num_sectors
; r2 : offset_sectors
read_mlc:
	push {r1,r2,r3,r4,lr}
	
	; Call sdcard_readwrite with read command
	str r2, [sp]
	mov r2, r1 ; num_sectors
	mov r1, r0 ; data_ptr
	ldr r0, =mlc_out_callback_arg2
	str r0, [sp, #0x4] ; out_callback_arg2
	mov r0, #0xAB	   ; MLC ID
	str r0, [sp, #0x8] ; device_id
	mov r0, #1 ; Read command
	mov r3, #0x200 ; block_size
	bl sdcard_readwrite
	
	; Return
	add sp, #0xC
	pop {r4,pc}	

; Dump the SLC
mlc_dump:
	push {r4,r7,lr}
	sub sp, #8
	
	mov r4, #0
	ldr r7, =MLC_BASE_SECTORS

	dump_mlc_loop:

		mov r5, #0

		dump_mlc_loop2:
			; Print args
			mov r3, r4
			mov r0, #20
			mov r1, #0
			adr r2, mlc_format
			bl _printf

			; Read from MLC
			ldr r0, =sdcard_read_buffer
			mov r1, #0x800
			mov r2, r4
			add r4, r1
			bl read_mlc
			str r0, [sp]

			; Write to SD card
			ldr r0, =sdcard_read_buffer
			mov r1, #0x100000
			mov r2, r7
			add r7, r7, r1, lsr 9
			bl write_data_offset
			str r0, [sp, #4]

			add r5, #1
			cmp r5, #0x40
			blt dump_mlc_loop2

		; Loop until we've dumped everything
		ldr r0, =0x3A20000
		cmp r4, r0
		blt dump_mlc_loop

	add sp, #8
	pop {r4,r7,pc}

	mlc_format:
	.ascii "mlc     = %08X %08X %08X"
	.byte 0x00
	.align 4

.pool

; Clear screen black
clear_screen:
	push {lr}
	ldr r0, =FRAMEBUFFER_ADDRESS
	ldr r1, =0x00
	ldr r2, =FRAMEBUFFER_STRIDE * 504
	bl FS_MEMSET
	pop {pc}

; r0 : x, r1 : y, r2 : format, ...
_printf:
	ldr r12, =_printf_xylr
	str r0, [r12]
	str r1, [r12, #4]
	str lr, [r12, #8]
	ldr r0, =_printf_string
	mov r1, #_printf_string_end - _printf_string
	bl FS_SNPRINTF
	ldr r12, =_printf_xylr
	ldr r1, [r12]
	ldr r2, [r12, #4]
	ldr lr, [r12, #8]
	push {lr}
	ldr r0, =_printf_string
	bl drawString
	pop {pc}

; r0 : str, r1 : x, r2 : y
drawString:
	push {r4-r6,lr}
	mov r4, r0
	mov r5, r1
	mov r6, r2
	drawString_loop:
		ldrb r0, [r4], #1
		cmp r0, #0x00
		beq drawString_end
		mov r1, r5
		mov r2, r6
		bl drawCharacter
		add r5, #CHARACTER_SIZE
		b drawString_loop
	drawString_end:
	pop {r4-r6,pc}

; r0 : char, r1 : x, r2 : y
drawCharacter:
	subs r0, #32
	; bxlt lr
	push {r4-r7,lr}
	ldr r4, =FRAMEBUFFER_ADDRESS ; r4 : framebuffer address
	add r4, r1, lsl 2 ; add x * 4
	mov r3, #FRAMEBUFFER_STRIDE
	mla r4, r2, r3, r4
	adr r5, font_data ; r5 : character data
	add r5, r0, lsl 3 ; font is 1bpp, 8x8 => 8 bytes represents one character
	mov r1, #0xFFFFFFFF ; color
	mov r2, #0x0 ; empty color
	mov r6, #8 ; i
	drawCharacter_loop1:
		mov r3, #CHARACTER_MULT
		drawCharacter_loop3:
			mov r7, #8 ; j
			ldrb r0, [r5]
			drawCharacter_loop2:
				tst r0, #1
				; as many STRs as CHARACTER_MULT
				streq r1, [r4], #4
				streq r1, [r4], #4
				strne r2, [r4], #4
				strne r2, [r4], #4
				mov r0, r0, lsr #1
				subs r7, #1
				bne drawCharacter_loop2
			add r4, #FRAMEBUFFER_STRIDE - CHARACTER_SIZE * 4
			subs r3, #1
			bne drawCharacter_loop3
		add r5, #1
		subs r6, #1
		bne drawCharacter_loop1
	pop {r4-r7,pc}

.pool

font_data:
	.incbin "patches/font.bin"

.Close

.create "patches/patched_sections/0x10835000.bin",0x10835000

.org BSS_BASE
	sdcard_access_mutex:
		.word 0x00000000
	dumpdata_offset:
		.word 0x00000000
	mlc_out_callback_arg2:
		.word 0x00000000
	_printf_xylr:
		.word 0x00000000
		.word 0x00000000
		.word 0x00000000
	_printf_string:
		.fill ((_printf_string + 0x100) - .), 0x00
	_printf_string_end:
	.align 0x40
	syslog_buffer:
		.fill ((syslog_buffer + 0x40000) - .), 0x00
	.align 0x40
	sdcard_read_buffer:
		.fill ((sdcard_read_buffer + 0x100000) - .), 0x00

.Close
