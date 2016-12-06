.arm.big

.open "patches/sections/0x08120000.bin","patches/patched_sections/0x08120000.bin",0x08120000

CODE_SECTION_BASE equ 0x08120000
CODE_SECTION_SIZE equ 0x00015000
CODE_BASE equ (CODE_SECTION_BASE + CODE_SECTION_SIZE)

RODATA_SECTION_BASE equ 0x08140000
RODATA_SECTION_SIZE equ 0x00002478
RODATA_BASE equ (RODATA_SECTION_BASE + RODATA_SECTION_SIZE)

DATA_SECTION_BASE equ 0x08143000
DATA_SECTION_SIZE equ 0x0000D000
DATA_BASE equ (DATA_SECTION_BASE + DATA_SECTION_SIZE)

BSS_SECTION_BASE equ 0x08150000
BSS_SECTION_SIZE equ 0x00061230
BSS_BASE equ (BSS_SECTION_BASE + BSS_SECTION_SIZE)

FRAMEBUFFER_ADDRESS equ (0x14000000 + 0x38C0000)
FRAMEBUFFER_STRIDE equ (0xE00)
CHARACTER_MULT equ (2)
CHARACTER_SIZE equ (8 * CHARACTER_MULT)

KERNEL_MEMSET equ 0x08131DA0
KERNEL_SNPRINTF equ 0x08132988
KERNEL_MCP_IOMAPPINGS_STRUCT equ 0x08140DE0

; Use syscall 0x81 for arbitrary read/write
.org 0x0812CD2C
    b rw_syscall

; Remove PID check for syscall 0x22 (read_otp)
.org 0x0812037C
	cmp r12, r12
	
; Set all MMU domains to 0x55555555 (client access)
.org 0x081253C4
	str r3, [r7,#0x10]
	str r3, [r7,#0x0C]
	str r3, [r7,#0x04]
	str r3, [r7,#0x14]
	str r3, [r7,#0x08]
	str r3, [r7,#0x34]

; Custom ARM exceptions' handler
.org 0x0812A134 ; Instruction fetch abort
	mov r3, #0
	b ex_handler
.org 0x0812A1AC ; Data abort
	mov r3, #1
	b ex_handler
.org 0x08129E50 ; Illegal instruction
	mov r3, #2
	b ex_handler
.org 0x08129DF8 ; Bad stack
	mov r3, #3
	b ex_handler
.org 0x08129D80 ; sysProt IOI fault
	mov r3, #4
	b ex_handler

.org CODE_BASE

rw_syscall:
	cmp r0, #0
	ldreq r0, [r1]
	strne r2, [r1]
	bx lr

ex_handler:
	sub sp, #4
	mov r4, r0	; thread_struct address
	mov r5, r3	; Exception error number

	; Clear framebuffer
	ldr r0, =FRAMEBUFFER_ADDRESS
	ldr r1, =0xFF
	ldr r2, =896*504*4
	bl KERNEL_MEMSET
	
	; Print exception type
	mov r0, #0
	mov r1, #0
	cmp r5, #0
	adreq r2, ex_handler_format_fetch
	cmp r5, #1
	adreq r2, ex_handler_format_data
	cmp r5, #2
	adreq r2, ex_handler_format_instr
	cmp r5, #3
	adreq r2, ex_handler_format_stack
	cmp r5, #4
	adreq r2, ex_handler_format_sysprot
	bl _printf

	mov r6, #0
	
	; Print thread's registers
	print_regs_loop:
		mov r0, #20
		mul r1, r6, r0
		add r1, #40
		cmp r6, #10
		adrlt r2, reg_format_0
		adrge r2, reg_format_00
		add r3, r4, #4
		ldr r3, [r3, r6, lsl 2]
		str r3, [sp]
		mov r3, r6
		bl _printf

		add r6, #1
		cmp r6, #0x10
		blt print_regs_loop

	deadlock:
		b deadlock
	
	ex_handler_format_fetch:
		.ascii "ARM exception (instruction fetch abort)"
		.byte 0x00
		.align 0x4
	
	ex_handler_format_data:
		.ascii "ARM exception (data abort)"
		.byte 0x00
		.align 0x4
		
	ex_handler_format_instr:
		.ascii "ARM exception (illegal instruction)"
		.byte 0x00
		.align 0x4
		
	ex_handler_format_stack:
		.ascii "ARM exception (bad stack)"
		.byte 0x00
		.align 0x4
		
	ex_handler_format_sysprot:
		.ascii "ARM exception (sysProt IOI fault)"
		.byte 0x00
		.align 0x4
		
	reg_format_0:
		.ascii "r%d  = %08X"
		.byte 0x00
		.align 0x4

	reg_format_00:
		.ascii "r%d = %08X"
		.byte 0x00
		.align 0x4

; r0 : x, r1 : y, r2 : format, ...
_printf:
	ldr r12, =_printf_xylr
	str r0, [r12]
	str r1, [r12, #4]
	str lr, [r12, #8]
	ldr r0, =_printf_string
	mov r1, #_printf_string_end - _printf_string
	bl KERNEL_SNPRINTF
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


.open "patches/sections/0x08140000.bin","patches/patched_sections/0x08140000.bin",0x08140000

.org KERNEL_MCP_IOMAPPINGS_STRUCT
	.word mcpIoMappings_patch ; Pointer to IO mapping structs
	.word 0x00000003 ; Number of IO mapping structs
	.word 0x00000001 ; PID (MCP)

.org RODATA_BASE
	mcpIoMappings_patch:
		; Latte registers (shared with Espresso) mapping
			.word 0x0D000000 ; paddr
			.word 0x0D000000 ; vaddr
			.word 0x001C0000 ; size
			.word 0x00000000 ; domain
			.word 0x00000003 ; permissions (read/write)
			.word 0x00000000 ; attributes (uncached)
		; Latte registers mapping
			.word 0x0D800000 ; paddr
			.word 0x0D800000 ; vaddr
			.word 0x001C0000 ; size
			.word 0x00000000 ; domain
			.word 0x00000003 ; permissions (read/write)
			.word 0x00000000 ; attributes (uncached)
		; Espresso registers mapping
			.word 0x0C200000 ; paddr
			.word 0x0C200000 ; vaddr
			.word 0x00100000 ; size
			.word 0x00000000 ; domain
			.word 0x00000003 ; permissions (read/write)
			.word 0x00000000 ; attributes (uncached)

.Close


.create "patches/patched_sections/0x08150000.bin",0x08150000

.org BSS_BASE
	_printf_xylr:
		.word 0x00000000
		.word 0x00000000
		.word 0x00000000
	_printf_string:
		.fill ((_printf_string + 0x100) - .), 0x00
	_printf_string_end:
	.align 0x40

.Close