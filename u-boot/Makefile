# Makefile to prepare u-boot installation.

TOP = ..
include $(TOP)/CONFIG

TARGET_DIR = $(U_BOOT_TARGET_DIR)
TARGET_U_BOOT = $(TARGET_DIR)/u-boot.bin

COMPONENT = u-boot


O.native := $(foreach COMPILER, native, $(O))
PADFF = $(O.native)/padff


default: $(PADFF)
	rm -rf $@
	mkdir -p $@
	cp update-u-boot $(TARGET_DIR)
	make -C mtd-rw install TARGET_DIR=$(TARGET_DIR)
	make -C tools  install TARGET_DIR=$(TARGET_DIR)
	$(PADFF) 0xff 0x40000 <$(U_BOOT_BIN) >$(TARGET_U_BOOT)

$(PADFF):
	make -C tools

clean:
	rm -rf $(TARGET_DIR)

clean-all: clean
	make -C mtd-rw clean
	make -C tools clean

.PHONY: clean clean-all
