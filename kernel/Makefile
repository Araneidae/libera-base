TOP = $(CURDIR)/..
COMPONENT = kernel
include $(TOP)/scripts/COMMON

O = $(BUILD_TARGET)

include $(KERNEL_CONFIG)/CONFIG

ifeq ($(DEVELOPMENT),y)
include $(TOP)/rootfs/config/DEVELOPMENT
endif


MD5_SUM_2.6.20.14 = 63deab5998064e93196c0b75e77060f0
MD5_SUM_2.6.26    = 5169d01c405bc3f866c59338e217968c
MD5_SUM_2.6.26.8  = 05dd0d4f8f110b4219ae6ec7a36c046d
MD5_SUM_2.6.29.2  = a6839571a9e70baf821d2fb752f9c4c6

default: build



ifndef KERNEL_SOURCE_DIR
# If no external build directory is specified, extract and patch the sources
# as specified
KERNEL_SOURCE_DIR = $(SOURCE_ROOT)/linux-$(LINUX_VERSION)

# Source extraction is somewhat involved.  We have to search through what may
# be a list of tar directories to find the tar file, which we then checksum
# against the given MD5_SUM before untarring.  Then we apply all the patches,
# again taking care to catch any failures.  Finally the directory is touched
# to try and avoid redundant rebuilds.
$(KERNEL_SOURCE_DIR): 
	mkdir -p $(SOURCE_ROOT)
	for dir in $(TAR_DIRS); do \
            tar_file="$$dir/linux-$(LINUX_VERSION).tar.bz2"; \
            [ -e "$$tar_file" ]  && \
            echo "$(MD5_SUM_$(LINUX_VERSION))  $$tar_file" | md5sum -c  && \
            tar xjf "$$tar_file" -C $(SOURCE_ROOT)  && \
            exit 0; \
        done; exit 1
	for file in $(PATCHES); do \
            patch -p1 -d$(KERNEL_SOURCE_DIR) \
                < $(KERNEL_CONFIG)/$$file  || \
            exit 1; \
        done
	touch $(KERNEL_SOURCE_DIR)
endif


# Export the symbols needed by the kernel build.
export CROSS_COMPILE
#export KBUILD_OUTPUT = $(O)
export ARCH = arm
export PATH := $(PATH):$(U_BOOT_DIR)

$(O)/.config: $(KERNEL_SOURCE_DIR) $(KERNEL_CONFIG)/$(CONFIG)
	mkdir -p $(O)
	cp $(KERNEL_CONFIG)/$(CONFIG) $@
	make -C $(KERNEL_SOURCE_DIR) O=$(O) oldconfig

menuconfig: $(O)/.config
	make -C $(KERNEL_SOURCE_DIR) O=$(O) menuconfig

oldconfig:
	make -C $(KERNEL_SOURCE_DIR) O=$(O) oldconfig

build: $(O)/.config
	make -C $(KERNEL_SOURCE_DIR) O=$(O) uImage
ifeq ($(DEVELOPMENT),y)
	scp $(O)/arch/arm/boot/uImage $(TFTP_SERVER)/$(KERNEL_NAME)
endif

clean:
	rm -rf $(O)


untar: $(KERNEL_SOURCE_DIR)
config: $(O)/.config

save-config:
	cp $(O)/.config $(KERNEL_CONFIG)/$(CONFIG)
