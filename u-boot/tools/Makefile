TOP = $(CURDIR)/../..
include $(TOP)/scripts/COMMON
COMPONENT = u-boot
ifndef O
$(eval $(REFLECT_makefile))
else

default: readmem mtdinfo

readmem: readmem.c

mtdinfo: mtdinfo.c

endif
