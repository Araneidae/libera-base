# Make file to build Libera root filesystem.

TOP = $(CURDIR)/..
COMPONENT = rootfs
COMPILER = linux-arm_el
include $(TOP)/scripts/COMMON

export TOP

TARGET = $(CURDIR)/$1
#CONFIG = $(CURDIR)/CONFIG.rootfs
TARGET_ROOT = $(BUILD_ROOT)/rootfs/$1

libera_TARGET    = $(CURDIR)/libera
installer_TARGET = $(CURDIR)/installer
libera_ROOT      = $(BUILD_ROOT)/rootfs/libera
installer_ROOT   = $(BUILD_ROOT)/rootfs/installer

define rootfs
$(ROOTFS)/rootfs $2 TARGET=$($1_TARGET) TARGET_ROOT=$($1_ROOT)
endef

rootfs: 
	$(call rootfs,libera,make) BOOT=initramfs

packages:
	$(call rootfs,libera,all)

default: rootfs

installer:
#	$(call rootfs,installer,all)
	$(call rootfs,installer,make)


.PHONY: rootfs packages installer
