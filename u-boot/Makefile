# Makefile to prepare u-boot installation.

TOP = $(CURDIR)/..
include $(TOP)/CONFIG
include $(TOP)/scripts/COMMON

TARGET_DIR = $(INSTALL_ROOT)/u-boot-upgrade
TARGET_U_BOOT = $(TARGET_DIR)/u-boot.bin

COMPONENT = u-boot


O.native := $(foreach COMPILER,native,$(O))
PADFF = $(O.native)/padff

SCRIPTS = update-u-boot readenv


default: $(PADFF)
	rm -rf $(TARGET_DIR)
	mkdir -p $(TARGET_DIR)
	cp $(SCRIPTS) $(TARGET_DIR)
	$(MAKE) -C mtd-rw install TARGET_DIR=$(TARGET_DIR)
	$(MAKE) -C tools  install TARGET_DIR=$(TARGET_DIR)
	$(PADFF) 0xff 0x40000 <$(U_BOOT_TOP)/u-boot.bin >$(TARGET_U_BOOT)

$(PADFF):
	$(MAKE) -C tools COMPILERS=native padff

clean:
	rm -rf $(TARGET_DIR)
	$(MAKE) -C mtd-rw clean
	$(MAKE) -C tools clean

.PHONY: clean clean-all
