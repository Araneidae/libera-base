# $Id: Makefile 2192 2008-10-07 09:13:06Z matejk $
#
# Copyright (C) 2006 Instrumentation Technologies
# See the COPYING file for copying conditions.
#
#
# GNU Makefile to build the MSP tests.
# If you have GNU Make installed, type 
#
#   make
#   make install
#   make clean
#
# on the command line to build and install the MSP tests.
# To change the default parameters, edit the configuration
# section below.
#

# 
# -------- configuration section: start --------
#

# Set debugging mode (0 = inactive)
DEBUG = 0

# Kernel source tree (comment out for autodetection)
# KERNELDIR =

# Cross compilation toolchain prefix
# CROSS_COMPILE = xcep-

#
# -------- configuration section: end --------
#

# CXX=$(CROSS_COMPILE)g++
CXX=$(CROSS_COMPILE)gcc

#ifdef CROSS_COMPILE
#CXXFLAGS+= -static
#endif

CXXFLAGS+= -I. -I..

# Debugging and optimization flags
ifneq ($(DEBUG),0)
  #DEB_OPT_FLAGS = -O -g -DDEBUG # "-O" is needed to expand inlines
  DEB_OPT_FLAGS = -O2 -DDEBUG # "-O" is needed to expand inlines
else
  DEB_OPT_FLAGS = -O2
endif
CXXFLAGS += $(DEB_OPT_FLAGS)
CXXFLAGS += -Wall


TESTS =  test_msp

ifneq ($(DEBUG),0)
  TESTS += $(DEBUG_TESTS)
endif

all: $(TESTS)


#$(TESTS): .depend $(OBJS)
#	$(CC) -o $@ $(OBJS)

#%.o:    %.cpp
#	$(CXX) $(CXXFLAGS) -c $<

clean:	dummy
	$(RM) -f $(TESTS) $(DEBUG_TESTS) *~
	$(RM) -f core `find . -type f -name '*.[osd]' -o -name '*~' -o \
	-name '*.out' -o -name '#*' -o -name '.#*'`
	$(RM) -f .depend


dep .depend:
	$(CC) $(CFLAGS) -M *.cpp > .depend

ifeq (.depend,$(wildcard .depend))
  include .depend
endif

# False targets.
.PHONY: dummy
