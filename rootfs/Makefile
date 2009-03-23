# Make file to build Libera root filesystem.

TOP = $(CURDIR)/..
COMPONENT = rootfs
include $(TOP)/scripts/COMMON


ROOTFS_EXPORTS = \
    BINUTILS_DIR COMPILER_PREFIX BUILD_TARGET DEVELOPMENT
MAKE_ROOTFS = \
    $(MAKE) -C $(ROOTFS) NOLOCAL=1 TARGET=. \
        $(call EXPORT, $(ROOTFS_EXPORTS)) \
        MKIMAGE=$(U_BOOT_DIR)/mkimage \
        LIBERA_BASE=$(TOP) \
        configdir=$(CURDIR)/config 

rootfs: 
	$(MAKE_ROOTFS) default

extras:
	$(MAKE_ROOTFS) build-extras

boot:
	$(MAKE_ROOTFS) deploy-rootfs

clean:
	rm -rf $(BUILD_TARGET)

default: rootfs

.PHONY: rootfs extras boot clean
