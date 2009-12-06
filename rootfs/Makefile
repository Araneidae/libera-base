# Make file to build Libera root filesystem.

export TOP = $(CURDIR)/..
include $(TOP)/CONFIG
include $(TOP)/scripts/COMMON


# For maintenance targets, can be overridden on the command line.
_BUILD ?= libera

# Default build is complete build of libera and installer packages.
default: installer


KBUILD_OUTPUT := $(shell \
    $(MAKE) -s -C $(KERNEL_TOP) TARGET=libera-2.6 print-build-dir)
KERNEL_IMAGE = $(KBUILD_OUTPUT)/arch/arm/boot/uImage
INSTALL_IMAGE = $(BUILD_ROOT)/rootfs/libera/image/imagefile.cpio


# Builds the selected rootfs, called as
#
#   $(call ROOTFS,<target-name>,<action>[,<package>])
#
define ROOTFS
$(ROOTFS_TOP)/rootfs $2 \
    TARGET=$(CURDIR)/$1 TARGET_ROOT=$(BUILD_ROOT)/rootfs/$1
endef

package:
	$(call ROOTFS,$(_BUILD),package $(PACKAGE)) $(ACTION)

clean-all:
	rm -rf $(BUILD_ROOT)/rootfs

libera:
	$(call ROOTFS,$@,make)

installer:
	$(call ROOTFS,$@,make) $(call EXPORT,KERNEL_IMAGE INSTALL_IMAGE)

installer: libera

.PHONY: package clean-all libera installer
