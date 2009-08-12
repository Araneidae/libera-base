obj-m := hello.o
KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)


KDIR := /scratch/git/linux-2.6

default:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules

clean:
	rm -rf .tmp_versions
	rm -f .*.cmd *.o *.ko *.mod.c
