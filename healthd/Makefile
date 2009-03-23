TOP = $(CURDIR)/..
include $(TOP)/scripts/COMMON
COMPONENT = healthd
ifndef O
$(eval $(REFLECT_makefile))
else


CPPFLAGS += -I$(TOP)/driver/src


healthd: healthd.c

default: healthd

clean:
	rm -rf $(O)

endif
