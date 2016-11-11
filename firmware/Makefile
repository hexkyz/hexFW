INPUT := img/fw.img
SECTIONS := 0x08120000 0x08140000 0x05000000 0x05100000 0x04000000
BSS_SECTIONS := 0x08150000 0x05074000
INPUT_SECTIONS := $(addprefix patches/sections/, $(addsuffix .bin, $(SECTIONS)))
PATCHED_SECTIONS := $(addprefix patches/patched_sections/, $(addsuffix .bin, $(SECTIONS)))

.PHONY: all clean programs/wupserver/wupserver.bin

all: fw.img

extract: $(INPUT_SECTIONS)

patch: $(PATCHED_SECTIONS)

programs/wupserver/wupserver.bin:
	@cd programs/wupserver && make

patches/sections/%.bin: $(INPUT)
	@mkdir -p patches/sections
	@python scripts/anpack.py -in $(INPUT) -e $*,$@

patches/patched_sections/%.bin: patches/sections/%.bin patches/%.s programs/wupserver/wupserver.bin
	@mkdir -p patches/patched_sections
	@echo patches/$*.s
	@armips patches/$*.s

fw.img: $(INPUT) $(INPUT_SECTIONS) $(PATCHED_SECTIONS)
	python scripts/anpack.py -in $(INPUT) -out fw.img $(foreach s,$(SECTIONS),-r $(s),patches/patched_sections/$(s).bin) $(foreach s,$(BSS_SECTIONS),-b $(s),patches/patched_sections/$(s).bin)

clean:
	@cd programs/wupserver && make clean
	@rm -f fw.img