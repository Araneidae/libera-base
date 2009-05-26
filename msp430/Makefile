TOP = $(CURDIR)/..
COMPONENT = kernel
include $(TOP)/scripts/COMMON

# This isn't quite right yet.  Currently doesn't build out of tree, but we
# need to point to the kernel build tree which this is.
KBUILD_OUTPUT = $(BUILD_ROOT)/kernel

#KERNEL_SOURCE_DIR = $(SOURCE_ROOT)/linux-2.6.26.8
O = $(BUILD_ROOT)/msp

export CROSS_COMPILE
#export KBUILD_OUTPUT = $(O)
export ARCH = arm

#KBUILD_CFLAGS += -std=c99



test: test.c

$(O):
	mkdir -p $(O)
	ln -fs $(wildcard $(patsubst %,$(CURDIR)/%, *.[c,h] Kbuild)) $(O)

default: $(O)
	make -C $(KBUILD_OUTPUT) M=$(O) 
	cp $(O)/msp.ko /home/libera/nfs/testing

clean:
	rm -rf $(O)

# install:
# 	make -C $(KBUILD_OUTPUT) M=$(O) \
#             INSTALL_MOD_PATH=$(CURDIR)/testing modules_install V=1
