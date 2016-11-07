BIN_DIR := bin
FIRMWARE_DIR := firmware
FIRMWARE_FILES := fw.img
LAUNCHER_DIR := launcher
LAUNCHER_FILES := iosu/fwboot/bin/code550.bin
BROWSERHAX_FILES := browserhax/wiiu_browserhax.php browserhax/wiiu_browserhax_common.php browserhax/wiiuhaxx_buildropversions.sh browserhax/wiiuhaxx_common_cfg.php browserhax/wiiuhaxx_loader.bin browserhax/wiiuhaxx_loader.elf browserhax/wiiuhaxx_loader.s browserhax/wiiuhaxx_locaterop.sh browserhax/wiiuhaxx_locaterop_script browserhax/wiiuhaxx_rop_sysver_532.php browserhax/wiiuhaxx_rop_sysver_550.php

all:
	@mkdir -p $(BIN_DIR)/{www,sdcard}
	@cd $(LAUNCHER_DIR)/framework && make
	@cd $(LAUNCHER_DIR)/libwiiu && make
	@cd $(LAUNCHER_DIR)/iosu/fwboot && make
	@cd $(LAUNCHER_DIR)/browserhax && make
	@cd $(FIRMWARE_DIR) && make
	@cd $(LAUNCHER_DIR) && cp $(LAUNCHER_FILES) ../$(BIN_DIR)/www
	@cd $(LAUNCHER_DIR) && cp $(BROWSERHAX_FILES) ../$(BIN_DIR)/www
	@cd $(FIRMWARE_DIR) && cp $(FIRMWARE_FILES) ../$(BIN_DIR)/sdcard
	
clean:
	@cd $(LAUNCHER_DIR)/iosu/fwboot && make clean
	@cd $(LAUNCHER_DIR)/libwiiu && make clean
	@cd $(LAUNCHER_DIR)/framework && make clean
	@cd $(LAUNCHER_DIR)/browserhax && make clean
	@cd $(FIRMWARE_DIR) && make clean
	@rm -rf $(BIN_DIR)/{www,sdcard}