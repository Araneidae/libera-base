# Make file to build Libera root filesystem.

export TOP = $(CURDIR)/..
include $(TOP)/CONFIG
include $(TOP)/scripts/COMMON

COMPONENT = rootfs
TARGET_DIR = $(INSTALL_ROOT)/$(COMPONENT)
O = $(BUILD_ROOT)/$(COMPONENT)
TFTP_DIR = $(TARGET_DIR)/tftp


# For maintenance targets, can be overridden on the command line.
BUILD ?= libera
unexport BUILD


default: 


# KERNEL_IMAGE locates the kernel image that will be installed.
K_MAKE = $(MAKE) --no-print-directory -s -C $(KERNEL_TOP) TARGET=libera-2.6
KBUILD_OUTPUT  := $(shell $(K_MAKE) print-build-dir)
KERNEL_RELEASE := $(shell $(K_MAKE) kernelrelease)
KERNEL_IMAGE = $(KBUILD_OUTPUT)/arch/arm/boot/uImage
# This is where the libera image is placed: this will be installed on the
# target system.
INSTALL_IMAGE = $(O)/libera/image/imagefile.cpio


# Builds the selected rootfs, called as
#
#   $(call ROOTFS,<target-name>,<action>[,<package>])
#
define ROOTFS
$(ROOTFS_TOP)/rootfs $2 \
    TARGET=$(CURDIR)/$1 TARGET_ROOT=$(O)/$1
endef

package:
	$(call ROOTFS,$(BUILD),package $(PACKAGE)) $(ACTION)

menuconfig:
	$(call ROOTFS,libera,package busybox) menuconfig
	$(call ROOTFS,libera,package busybox) saveconfig

clean-all:
	rm -rf $(O)
	rm -rf $(TARGET_DIR)

libera:
	$(call ROOTFS,$@,make) $(call EXPORT,KERNEL_RELEASE)

installer: libera
	$(call ROOTFS,$@,make) \
            $(call EXPORT,KERNEL_IMAGE INSTALL_IMAGE KERNEL_RELEASE)

default: installer libera upgrade-libera
	rm -rf $(TARGET_DIR)
	mkdir -p $(TARGET_DIR) $(TFTP_DIR)
	sed <upgrade-libera >$(TARGET_DIR)/upgrade-libera \
            's/@@KERNEL_RELEASE@@/$(KERNEL_RELEASE)/'
	chmod +x $(TARGET_DIR)/upgrade-libera
	cp $(O)/installer/image/boot-script-installer.image $(TFTP_DIR)
	cp $(KERNEL_IMAGE) $(TFTP_DIR)/uImage-libera-$(KERNEL_RELEASE)
	cp $(O)/installer/image/imagefile.cpio.gz \
            $(TFTP_DIR)/imagefile-installer.cpio.gz


.PHONY: default package clean-all libera installer
