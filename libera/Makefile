TOP = $(CURDIR)/..
COMPONENT = kernel
include $(TOP)/scripts/COMMON

# This isn't quite right yet.  Currently doesn't build out of tree, but we
# need to point to the kernel build tree which this is.
KBUILD_OUTPUT = $(BUILD_ROOT)/kernel


export CROSS_COMPILE
#export KBUILD_OUTPUT = $(O)
export ARCH = arm


default:
#	make -C $(O) M=$(CURDIR) ARCH=arm O=$(O) KBUILD_OUTPUT=$(O)
	make -C $(KBUILD_OUTPUT) M=$(CURDIR)

clean:
	rm -f *.o *.ko .*.cmd *.mod.c *.symvers
	rm -rf .tmp_versions


install:
	make -C $(KBUILD_OUTPUT) M=$(CURDIR) \
            INSTALL_MOD_PATH=$(CURDIR)/testing modules_install V=1
