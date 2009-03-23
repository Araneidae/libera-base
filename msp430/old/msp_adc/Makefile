# $Id: Makefile 2192 2008-10-07 09:13:06Z matejk $
#
# Copyright (C) 2004, 2005 Instrumentation Technologies
# See the COPYING file for copying conditions.
#
#
# GNU Makefile to build the MSP related health support.
# If you have GNU Make installed, type 
#
#   make
#   make install
#   make clean
#
# on the command line to build and install the MSP tools.
# To change the default parameters, edit the configuration
# section below.
#

# 
# -------- configuration section: start --------
#

#
# TARGET: Set to family member ID.
#
export TARGET = EBPP

#
# drvdir: Where to look for Libera GNU/Linux driver .h files.
#
drvdir = ../../../driver/src

#
# cspidir: Where to look for Libera GNU/Linux CSPI .h files.
#
cspidir = ../../../cspi/src

#
# DEBUG: Debug level. Set to non-zero for debug build or
# anything else (zero or commented) for non-debug build.
# Note: CPPFLAGS is appended the DEBUG and NDEBUG flag in
# debug and non-debug build, respectively.
#
DEBUG = 0

#
# CROSS_COMPILE: Cross compiler prefix to prepend to CC.
#
#CROSS_COMPILE = arm-linux-

#
# CC: The command to invoke C compiler.
#
CC = gcc 

#
# MSP related toolchain
#
MSP_TOOLS =/usr/local/msp430/bin
MSPCPU = msp430x133
MSPASFLAGS = -mmcu=${MSPCPU} -D_GNU_ASSEMBLER_
MSPCFLAGS = -mmcu=${MSPCPU} -O2 -Wall
MSPCC = $(MSP_TOOLS)/msp430-gcc
MSPAR = $(MSP_TOOLS)/msp430-ar
MSPLD = $(MSP_TOOLS)/msp430-ld
MSPOBJCP = $(MSP_TOOLS)/msp430-objcopy

#
# CFLAGS: Options to pass to the C compiler.
#
CFLAGS = -std=gnu99 -Wall

#
# CPPFLAGS: Options to pass to the preprocessor.
#
CPPFLAGS = -D$(TARGET)

#
# LDFLAGS: Options to pass to the linker.
#
LDFLAGS = -L.

#
# LIBS: Additional libs to pass to the linker.
#
LIBS =

#
# AR: The command to invoke the archiver program.
#
AR = ar

#
# ARFLAGS: Options to pass to the AR utility.
#
ARFLAGS = -rcs

#
# INSTALL: The pathname of the 'install' command for installing 
# a file into the system.
#
INSTALL = /usr/bin/install

#
# prefix: A prefix used in constructing the installation 
# pathnames below.
#
prefix = /opt

#
# includedir: The directory to install header files in.
#
includedir = $(prefix)/include

#
# libdir: The directory to install the library in.
#
libdir = $(prefix)/lib

#
# bindir: The directory to install the binaries in.
#
bindir = $(prefix)/bin

#
# -------- configuration section: end --------
#

docdir := ../doc

vpath %.h $(cspidir)

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
	CFLAGS += -O2
	CPPFLAGS += -DNDEBUG
else
	CFLAGS += -O -g		# "-O" is needed to expand inlines
	CPPFLAGS += -DDEBUG=$(DEBUG)
endif

SRCS =  msp_adc.c
OBJS = $(SRCS:%.c=%.o)


all: mspload

mspload: mspload.c
	${CC} ${CFLAGS} ${CPPFLAGS} -I$(cspidir) -o $@ $^

msp_adc.elf: msp_adc.c
	${MSPCC} ${MSPCFLAGS} -o $@ $^

msp_adc.a43: msp_adc.elf
	${MSPOBJCP} -O ihex $^ $@

msp_adc.lst: msp_adc.elf
	msp430-objdump -dSt $^ >$@

download-jtag: all
	msp430-jtag -e msp_adc.elf

download-bsl: all
	msp430-bsl -e msp_adc.elf


install-dirs:

install: install-dirs


# SRC package
S_NAME       := $(notdir $(shell cd .. ; pwd))
S_NAME_VER   := $(S_NAME)-$(RELEASE_VERSION)
S_ARCHIVE    := $(S_NAME_VER).tar.gz
src: clean doc
	@if [ ! -d ../../$(S_NAME_VER) ]; \
	then \
		echo -e "Creating SRC package ../../$(S_ARCHIVE)"; \
		$(RM) -r $(docdir)/latex; \
		cd ../..; cp -a $(S_NAME) $(S_NAME_VER); \
		$(RM) -r `find $(S_NAME_VER) -path '*CVS' -prune`; \
		tar -cvzf $(S_ARCHIVE) $(S_NAME_VER); \
		$(RM) -r $(S_NAME_VER) ; cd -; \
		sync; \
	else \
		echo -e "Cannot create SRC package: ../../$(S_NAME_VER) exists."; \
	fi


# BIN & LPM package dir
PKGDIR       := ../..


# BIN package
B_NAME       := $(S_NAME)
B_NAME_VER   := $(B_NAME)-$(RELEASE_VERSION)
B_ARCHIVE    := $(PKGDIR)/$(B_NAME_VER)-$(TARGET)-bin.tar.gz
bin: clean all
	$(MAKE) prefix=../tmp/$(patsubst /%,%,$(prefix)) install
	@echo -e "Creating BIN package in $(B_ARCHIVE)"; \
	cd ../tmp; tar cvzfp $(B_ARCHIVE) . ; \
	cd -; \
	$(RM) -r ../tmp; \
	sync;


# LPM package
L_NAME       := $(S_NAME)
L_NAME_INST  := $(L_NAME)-install
L_NAME_VER   := $(L_NAME)-$(RELEASE_VERSION)
L_ARCHIVE    := $(PKGDIR)/$(L_NAME_VER)-$(TARGET).lpm
lpm: clean all
	$(MAKE) prefix=../tmp/$(patsubst /%,%,$(prefix)) install
	@echo -e "Creating LPM package in $(L_ARCHIVE)"; \
	cd ../tmp; cp ../../../sys/src/$(L_NAME_INST) ./install ; \
	tar cvfp $(L_ARCHIVE) . ; \
	cd -; \
	$(RM) -r ../tmp; \
	sync;


clean:
	@echo Making clean
	-$(RM) *~ *.o *.a *.elf .depend mspload 
	-$(RM) -r ../tmp


dep .depend:
	@echo Making dep and .depend
	$(CC) -MM $(CPPFLAGS) *.c >.depend


ifeq (.depend, $(wildcard .depend))
include .depend
endif

# EOF
