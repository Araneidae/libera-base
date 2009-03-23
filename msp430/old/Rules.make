# TOPDIR is declared by the Makefile including this file.
ifndef TOPDIR
  TOPDIR  := $(shell /bin/pwd)
endif

# Which kernel source to use?
ifndef KERNELDIR
  # Kernel Search Path
  # All the places we look for kernel source (change order to change priority)
  KSP :=  /usr/src/linux \
          /lib/modules/$(shell uname -r)/build \
          /usr/src/linux-$(shell uname -r) \
          /usr/src/linux-$(shell uname -r | sed 's/-.*//') \
          /usr/src/kernel-headers-$(shell uname -r) \
          /usr/src/kernel-source-$(shell uname -r) \
          /usr/src/linux-$(shell uname -r | sed 's/\([0-9]*\.[0-9]*\)\..*/\1/')

  # prune the list down to only values that exist
  # and have an include/linux sub-directory
  test_dir = $(shell [ -e $(dir)/include/linux ] && echo $(dir))
  KSP := $(foreach dir, $(KSP), $(test_dir))

  # we will use this first valid entry in the search path
  KERNELDIR := $(firstword $(KSP))

endif

# The headers are taken from the kernel
INCLUDEDIR = $(KERNELDIR)/include


# We need the configuration file, for CONFIG_SMP and possibly other stuff
# (especiall for RISC platforms, where CFLAGS depends on the exact
# processor being used).
DOT_CONFIG_FILE := $(KERNELDIR)/.config
ifeq ($(DOT_CONFIG_FILE),$(wildcard $(DOT_CONFIG_FILE)))
  include $(DOT_CONFIG_FILE)
else
  MESSAGE := $(shell echo "WARNING: No .config file in kernel source tree ($(KERNELDIR))" 1>&2; \
                     echo "Please read the README file." 1>&2)
endif

# ARCH can be specified on the comdline or env. too, and defaults to this arch
# Unfortunately, we can't easily extract if from kernel configuration
# (well, we could look athe asm- symlink... don't know if worth the effort)
ifndef ARCH
  ARCH := $(shell uname -m | sed -e s/i.86/i386/ -e s/sun4u/sparc64/ \
                -e s/arm.*/arm/ -e s/sa110/arm/)
endif

# Cross-compiling?
AS      =$(CROSS_COMPILE)as
LD      =$(CROSS_COMPILE)ld
CC      =$(CROSS_COMPILE)gcc
CPP     =$(CC) -E
AR      =$(CROSS_COMPILE)ar
NM      =$(CROSS_COMPILE)nm
STRIP   =$(CROSS_COMPILE)strip
OBJCOPY =$(CROSS_COMPILE)objcopy
OBJDUMP =$(CROSS_COMPILE)objdump

# The platform-specific Makefiles include portability nightmares.
# Some platforms, though, don't have one, so check for existence first
ARCHMAKEFILE = $(TOPDIR)/Makefile.$(ARCH)
ifeq ($(ARCHMAKEFILE),$(wildcard $(ARCHMAKEFILE)))
  include $(ARCHMAKEFILE)
endif

# CPPFLAGS: all assignments to CPPFLAGS are inclremental, so you can specify
# the initial flags on the command line or environment, if needed.
CPPFLAGS +=  -Wall -Wstrict-prototypes -D__KERNEL__ -DMODULE -DLINUX -I$(INCLUDEDIR)
# CFALGS += -fomit-frame-pointer 

ifdef CONFIG_SMP
  CPPFLAGS += -D__SMP__ -DSMP
endif

# Prepend modversions.h if we're running with versioning.
ifdef CONFIG_MODVERSIONS
  CPPFLAGS += -DMODVERSIONS -include $(KERNELDIR)/include/linux/modversions.h
endif


# Figure out the linux version we are compiling against and set Install dir
VERSION_FILE := $(KERNELDIR)/include/linux/version.h
#CONFIG_FILE  := $(KERNELDIR)/include/linux/config.h
ifeq (,$(wildcard $(VERSION_FILE)))
  $(error Linux kernel source not configured - missing $(VERSION_FILE))
endif

#ifeq (,$(wildcard $(CONFIG_FILE)))
#  $(error Linux kernel source not configured - missing $(CONFIG_FILE))
#endif

KERNEL_VERSION  = $(shell awk -F\" '/REL/ {print $$2}' $(VERSION_FILE))
INSTALLDIR = /lib/modules/$(KERNEL_VERSION)/misc
