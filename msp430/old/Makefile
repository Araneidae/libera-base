# $Id: Makefile 2352 2008-12-05 09:50:04Z matejk $	
#
# Copyright (C) 2004-2006 Instrumentation Technologies
# See the COPYING file for copying conditions.
#
#
# GNU Makefile to build the MSP linux driver for Libera.
# If you have GNU Make installed, type 
#
#   make
#   make install
#   make devices
#   make clean
#
# on the command line to build and install the MSP linux driver.
# To change the default parameters, edit the configuration
# section below.
#

ifneq ($(KERNELRELEASE),)

# TARGET: Set to family member ID.
#
export TARGET = EBPP

ifndef KERNELDIR
KERNELDIR = /usr/src/linux-2.6.20.14
endif


PWD := $(shell pwd)
ARCH := arm
# Set debugging level (0, 1, 2, 3, 4)
ifndef DEBUG
DEBUG = 0
endif
DRIVER := msp.ko
SRCS = msp_main.c 
OBJS = $(SRCS:%.c=%.o)
prefix=$(DESTDIR)/
ifndef INSTALLDIR
INSTALLDIR=/lib/modules/2.6.20.14.xcep-itech/misc
endif

RELEASE_VERSION := $(shell /bin/date +%Y%m%d)
EXTRA_CFLAGS += -DRELEASE_VERSION=$(RELEASE_VERSION)

# Debugging and optimization flags
ifeq ($(DEBUG),0)
 DEB_OPT_FLAGS = -O2 -DDEBUG=$(DEBUG)
else
 DEB_OPT_FLAGS = -O -g -DDEBUG=$(DEBUG) # "-O" is needed to expand inlines
endif
EXTRA_CFLAGS += $(DEB_OPT_FLAGS)
EXTRA_CFLAGS += -D$(TARGET)
obj-m := msp.o
msp-objs := msp_main.o

else


# TARGET: Set to family member ID.
#
export TARGET = EBPP

ifndef KERNELDIR
KERNELDIR = /usr/src/linux-2.6.20.14
endif

PWD := $(shell pwd)
ARCH := arm
# Set debugging level (0, 1, 2, 3, 4)
DEBUG = 0
DRIVER := msp.ko
SRCS = msp_main.c 
OBJS = $(SRCS:%.c=%.o)
prefix=$(DESTDIR)/
ifndef INSTALLDIR
INSTALLDIR=/lib/modules/2.6.20.14.xcep-itech/misc
endif

RELEASE_VERSION := $(shell /bin/date +%Y%m%d)
EXTRA_CFLAGS += -DRELEASE_VERSION=$(RELEASE_VERSION)

# Debugging and optimization flags
ifeq ($(DEBUG),0)
 DEB_OPT_FLAGS = -O2 -DDEBUG=$(DEBUG)
else
 DEB_OPT_FLAGS = -O -g -DDEBUG=$(DEBUG) # "-O" is needed to expand inlines
endif
EXTRA_CFLAGS += $(DEB_OPT_FLAGS)
EXTRA_CFLAGS += -D$(TARGET)



all: $(DRIVER) tests msp_adc

$(DRIVER):
	make  ARCH=$(ARCH)  DEBUG=$(DEBUG) RELEASE_VERSION=$(RELEASE_VERSION) -C $(KERNELDIR) M=$(PWD) modules


tests:	dummy
	@${MAKE} -C tests

msp_adc: dummy
	@${MAKE} -C msp_adc




# create debian package
DEB_TIME := $(shell /bin/date -R)
DEB_MINOR := 1
DEB_TARGET := unknown_target 
ifeq ($(TARGET),EBPP)
DEB_TARGET := ebpp
endif
ifeq ($(TARGET),BBFP)
DEB_TARGET := bbfp
endif
ifeq ($(TARGET),HBPP)
DEB_TARGET := hbpp
endif
ifeq ($(TARGET),DPP)
DEB_TARGET := dpp
endif

RELEASE_VERSION_TARGET=$(RELEASE_VERSION)-$(DEB_TARGET)

deb:
	@echo Creating debian package
	pwd
	-@$(RM) -r ../libera-msp-$(RELEASE_VERSION_TARGET)
	@cd ..;  pwd;\
	cp -a src libera-msp-$(RELEASE_VERSION_TARGET);\
	cd libera-msp-$(RELEASE_VERSION_TARGET);\
	pwd;\
	echo "libera-msp ($(RELEASE_VERSION_TARGET)-$(DEB_MINOR)) unstable; urgency=low" >debian/changelog ;\
	echo >>debian/changelog;\
	echo "  * Initial release" >>debian/changelog ;\
	echo >>debian/changelog ;\
	echo " -- Instrumentation Technologies <support@i-tech.si>  $(DEB_TIME)" >>debian/changelog;
	cd ../libera-msp-$(RELEASE_VERSION_TARGET); dpkg-buildpackage -rfakeroot -uc -us
	pwd
	$(RM) -r ../libera-msp-$(RELEASE_VERSION_TARGET)
	@echo "Packet was build for major release number $(RELEASE_VERSION) and  minor release number $(DEB_MINOR)"
	@echo "You may change this by setting RELEASE_VERSION and DEB_MINOR correspondingly"


clean:	
	$(RM) -f $(DRIVER) $(OBJS) *~
	$(RM) -f core `find . -type f -name '*.[osd]' -o -name '*~' -o \
	-name '*.out' -o -name '#*' -o -name '.#*'`
	$(RM) -f .depend
	$(RM) -rf ../tmp
	-$(RM)  ../libera-msp_* 
	@${MAKE} -C tests clean
	@${MAKE} -C msp_adc clean

install: $(DRIVER)
	install -d -o root -g root -m 755 $(prefix)$(INSTALLDIR)
	install -d -o root -g root -m 755 $(prefix)opt/bin
	install -d -o root -g root -m 755 $(prefix)opt/lib
	install -o root -g root -m 644 $(DRIVER) $(prefix)$(INSTALLDIR)
	install -o root -g root -m 744 msp_adc/mspload $(prefix)opt/bin
	install -o root -g root -m 644 msp_adc/msp_adc.a43 $(prefix)opt/lib/

# SRC package
S_NAME       := $(notdir $(shell cd .. ; pwd))
S_NAME_VER   := $(S_NAME)-$(RELEASE_VERSION)
S_ARCHIVE    := $(S_NAME_VER).tar.gz
src: clean
	@echo "Creating source package $(S_ARCHIVE)."
	-@$(RM) -r ../../$(S_NAME_VER)
	cd ../..; cp -a $(S_NAME) $(S_NAME_VER)
	-cd ../..; $(RM) -r `find $(S_NAME_VER) -path '*.svn'`
	tar -cvzf ../../$(S_ARCHIVE) -C ../../$(S_NAME_VER) .
	$(RM) -r ../../$(S_NAME_VER)

# False targets.
.PHONY: dummy
endif
