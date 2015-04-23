# Make file to build Libera root filesystem.

export TOP = $(CURDIR)/..
include $(TOP)/CONFIG
include $(TOP)/scripts/COMMON

COMPONENT = rootfs
TARGET_DIR = $(INSTALL_ROOT)/$(COMPONENT)
O = $(BUILD_ROOT)/$(COMPONENT)


# For maintenance targets, can be overridden on the command line.
BUILD ?= libera
PACKAGE ?= busybox
unexport BUILD PACKAGE


default: 


# KERNEL_IMAGE locates the kernel image that will be installed.
KERNEL_IMAGE = $(KBUILD_OUTPUT)/arch/arm/boot/uImage
# This is where the libera image is placed: this will be installed on the
# target system.
INSTALL_IMAGE = $(O)/libera/image/imagefile.cpio

# sed script to replace symbols.
DO_SUBST = $(foreach sym,$1,s/@@$(sym)@@/$($(sym))/g;)


# Builds the selected rootfs, called as
#
#   $(call ROOTFS,<target-name>,<action>[,<package>])
#
ROOTFS = $(ROOTFS_TOP)/rootfs $2 \
    TARGET=$(CURDIR)/$1 TARGET_ROOT=$(O)/$1 ROOTFS_ROOT=$(O)


package:
	$(call ROOTFS,$(BUILD),package $(PACKAGE)) $(ACTION)

menuconfig:
	$(call ROOTFS,$(BUILD),package busybox) menuconfig
	$(call ROOTFS,$(BUILD),package busybox) saveconfig

clean:
	chmod -R u+w $(O)
	rm -rf $(O)

clean-all: clean
	rm -rf $(TARGET_DIR)

libera:
	$(call ROOTFS,$@,make) $(call EXPORT,KERNEL_RELEASE)

installer: libera
	$(call ROOTFS,$@,make) \
            $(call EXPORT,KERNEL_IMAGE INSTALL_IMAGE KERNEL_RELEASE)

# INSTALL_TARGETS = \
#     $(TARGET_DIR)/upgrade-libera $(TARGET_DIR)/upgrade-defaults \
#     $(TFTP_DIR)/boot-script-installer-$(LIBERA_BASE_VERSION).image \
#     $(TFTP_DIR)/uImage-libera-$(KERNEL_RELEASE) \
#     $(TFTP_DIR)/imagefile-installer-$(LIBERA_BASE_VERSION).cpio.gz 

BOOT_SCRIPT_IMAGE = boot-script-installer-$(LIBERA_BASE_VERSION).image
INSTALL_DRIVERS = ../drivers/install-drivers
default: installer libera upgrade-libera upgrade-defaults
	rm -rf $(TARGET_DIR)
	mkdir -p $(TARGET_DIR) $(TFTP_DIR)
	sed <upgrade-libera >$(TARGET_DIR)/upgrade-libera \
            '$(call DO_SUBST,KERNEL_RELEASE LIBERA_BASE_VERSION)'
	sed <upgrade-defaults >$(TARGET_DIR)/upgrade-defaults \
            '$(call DO_SUBST,KERNEL_RELEASE LIBERA_BASE_VERSION)'
	chmod +x $(TARGET_DIR)/upgrade-libera
	cp $(O)/installer/image/$(BOOT_SCRIPT_IMAGE) $(TFTP_DIR)
	cp $(KERNEL_IMAGE) $(TFTP_DIR)/uImage-libera-$(KERNEL_RELEASE)
	cp $(O)/installer/image/imagefile.cpio.gz \
            $(TFTP_DIR)/imagefile-installer-$(LIBERA_BASE_VERSION).cpio.gz
	cp $(INSTALL_DRIVERS) $(INSTALL_ROOT)/drivers

install: default

.PHONY: default install package clean clean-all libera installer
