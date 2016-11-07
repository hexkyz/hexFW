DEFINES	:=	

all:
	powerpc-eabi-gcc -x assembler-with-cpp -nostartfiles -nostdlib $(DEFINES) -o wiiuhaxx_loader.elf wiiuhaxx_loader.s
	powerpc-eabi-objcopy -O binary wiiuhaxx_loader.elf wiiuhaxx_loader.bin

clean:
	rm -f wiiuhaxx_loader.elf wiiuhaxx_loader.bin

