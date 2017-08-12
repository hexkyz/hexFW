INPUT := img/fw.img
SECTIONS := 0x08120000 0x08140000 0x05000000 0x05100000 0x04000000
BSS_SECTIONS := 0x08150000 0x05074000
INPUT_SECTIONS := $(addprefix patches/sections/, $(addsuffix .bin, $(SECTIONS)))
PATCHED_SECTIONS := $(addprefix patches/patched_sections/, $(addsuffix .bin, $(SECTIONS)))

ifeq ($(OS),Windows_NT)
	ARMIPS := armips.exe
else
	ARMIPS := ./armips
endif

.PHONY: all clean programs/hexcore/hexcore.bin

all: fw.img

extract: $(INPUT_SECTIONS)

patch: $(PATCHED_SECTIONS)

img/fw.img:
	@python scripts/verify-keys.py
	@mkdir -p img
	@python scripts/getfwimg.py

programs/hexcore/hexcore.bin:
	@cd programs/hexcore && make

patches/sections/%.bin: $(INPUT)
	@mkdir -p patches/sections
	python scripts/anpack.py -in $(INPUT) -e $*,$@

patches/patched_sections/%.bin: patches/sections/%.bin patches/%.s programs/hexcore/hexcore.bin
	@mkdir -p patches/patched_sections
	@echo patches/$*.s
	$(ARMIPS) patches/$*.s

fw.img: $(INPUT) $(INPUT_SECTIONS) $(PATCHED_SECTIONS)
	python scripts/anpack.py -in $(INPUT) -out fw.img $(foreach s,$(SECTIONS),-r $(s),patches/patched_sections/$(s).bin) $(foreach s,$(BSS_SECTIONS),-b $(s),patches/patched_sections/$(s).bin)

clean:
	@cd programs/hexcore && make clean
	@rm -rf fw.img patches/patched_sections/ patches/sections/ img/
