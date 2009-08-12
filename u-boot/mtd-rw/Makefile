TOP = ../..

include ../CONFIG

KERNELDIR = $(SOURCE_DIR)

AS      =$(CROSS_COMPILE)as
LD      =$(CROSS_COMPILE)ld
CC      =$(CROSS_COMPILE)gcc
CPP     =$(CC) -E
AR      =$(CROSS_COMPILE)ar
NM      =$(CROSS_COMPILE)nm
STRIP   =$(CROSS_COMPILE)strip
OBJCOPY =$(CROSS_COMPILE)objcopy
OBJDUMP =$(CROSS_COMPILE)objdump


CPPFLAGS += -Wall -Wstrict-prototypes -D__KERNEL__ -DMODULE -DLINUX \
 -I$(KERNELDIR)/include -std=gnu99 -O2


#include Rules.make



# Target & sources
DRIVER = nothing.ko hello.ko mtd-rw.ko memory.ko



SRCS = nothing.c
OBJS = $(SRCS:%.c=%.o)



all: $(DRIVER) 

$(DRIVER): %.ko: %.o
	$(LD) -r -o $@ $^

clean:
	rm -f $(DRIVER) *.o
