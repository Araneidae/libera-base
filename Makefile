# Top level make file for LiberaBase.

default:


DRIVERS = drivers/msp430 drivers/libera
INSTALLER = u-boot rootfs


ALL = $(DRIVERS) $(INSTALLER)

# Support for DO_MAKE command to invoke make on each of a list of targets.
define _MAKE_ONE
$(MAKE) -C $1 $2

endef
DO_MAKE = $(foreach DIR,$1,$(call _MAKE_ONE,$(DIR),$2))


clean:
	$(call DO_MAKE,$(ALL),clean)

clean-all:
	$(call DO_MAKE,$(ALL),clean-all)

drivers:
	$(call DO_MAKE,$(DRIVERS))

installer:
	$(call DO_MAKE,$(INSTALLER))

docs:
	$(call DO_MAKE,docs)

$(ALL):
	$(call DO_MAKE,$@,install)

rootfs: u-boot

default: $(ALL)

.PHONY: default clean drivers installer docs
.PHONY: $(ALL)
