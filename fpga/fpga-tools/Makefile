# $Id: Makefile 2352 2008-12-05 09:50:04Z matejk $
#
# Copyright (C) 2004 Instrumentation Technologies
# See the COPYING file for copying conditions.

#
# -------- configuration section: start --------
#

#
# TARGET: Set to family member ID.
#
export TARGET = EBPP

# DEBUG: Debugging mode. Set to non-zero for debug build or
# anything else (zero or commented) for non-debug build.
# Note: CPPFLAGS is appended the DEBUG or NDEBUG flag in
# debug or non-debug build, respectively.
#
ifndef DEBUG
DEBUG = 0
endif

#
# CROSS_COMPILE: Cross compiler prefix to prepend to CC.
#

#
# CC: The command to invoke C compiler.
#
CC = gcc

#
# CFLAGS: Options to pass to the C compiler.
#
CFLAGS = -std=gnu99 -Wall

#
# CPPFLAGS: Options to pass to the preprocessor.
#
CPPFLAGS =

#
# LDFLAGS: Options to pass to the linker.
#
LDFLAGS =

# Installation prefix
prefix=$(DESTDIR)/
bindir=opt/bin/

#
# -------- configuration section: end --------
#

CC := $(CROSS_COMPILE)$(CC)
AR := $(CROSS_COMPILE)$(AR)


# Release version
RELEASE_VERSION := $(shell /bin/date +%Y%m%d)
CPPFLAGS += -DRELEASE_VERSION=$(RELEASE_VERSION)

# Debugging and optimization flags
ifndef DEBUG
        DEBUG = 0
endif
ifeq ($(DEBUG),0)
        CFLAGS += -O2 -Wall
        CPPFLAGS += -DNDEBUG
else
        # "-O" is needed to expand inlines
        CFLAGS += -O -g -Wall
        CPPFLAGS += -DDEBUG=$(DEBUG)
endif
CPPFLAGS += -D$(TARGET)

# Target & sources
ifeq ($(TARGET),BBFP)
	FPGA = fp Monitor fp-v5 spimonitor
	SRCS = fp.c monitor.c fp-v5.c spimonitor.c
else
	FPGA = fp Monitor
	SRCS = fp.c monitor.c
endif
OBJS = $(SRCS:%.c=%.o)

all: .depend $(FPGA)

fp:  fp.o
	$(CC) -o $@ $<

fp-v5:  fp-v5.o
	$(CC) -o $@ $<

spimonitor:  spimonitor.o
	$(CC) -o $@ $<

Monitor:  monitor.o
	$(CC) -o $@ $<

install: $(FPGA)
	install -d -o root -g root -m 755 $(prefix)$(bindir)
	install -c -o root -g root -m 755 $(FPGA) $(prefix)$(bindir)

#
#
#
# create debian package
DEB_MINOR := 1
DEB_TIME := $(shell /bin/date -R)
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
	-@$(RM) -r ../libera-fpga-tools-$(RELEASE_VERSION_TARGET)
	@cd ..;  pwd;\
	cp -a src libera-fpga-tools-$(RELEASE_VERSION_TARGET);\
	cd libera-fpga-tools-$(RELEASE_VERSION_TARGET);\
	pwd;\
	echo "libera-fpga-tools ($(RELEASE_VERSION_TARGET)-$(DEB_MINOR)) unstable; urgency=low" >debian/changelog ;\
	echo >>debian/changelog;\
	echo "  * Initial release" >>debian/changelog ;\
	echo >>debian/changelog ;\
	echo " -- Instrumentation Technologies <support@i-tech.si>  $(DEB_TIME)" >>debian/changelog;
	cd ../libera-fpga-tools-$(RELEASE_VERSION_TARGET); dpkg-buildpackage -rfakeroot -uc -us
	pwd
	$(RM) -r ../libera-fpga-tools-$(RELEASE_VERSION_TARGET)
	@echo "Packet was build for major release number $(RELEASE_VERSION) and  minor release number $(DEB_MINOR)"
	@echo "You may change this by setting RELEASE_VERSION and DEB_MINOR correspondingly"


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

clean:	dummy
	$(RM) -f $(FPGA) *~
	$(RM) -f core `find . -type f -name '*.[osd]' -o -name '*~' -o \
	-name '*.out' -o -name '#*' -o -name '.#*'`
	$(RM) -f .depend
	$(RM) -rf ../tmp
	-$(RM)  ../libera-fpga-tools_*

dep .depend:
	$(CC) $(CPPFLAGS) -M $(SRCS) > .depend

ifeq (.depend,$(wildcard .depend))
  include .depend
endif

# False targets.
.PHONY: dummy
