TOP = ..
# include $(TOP)/COMMON


PROGRAMS = readenv makeenv hexdump crc32
SCRIPTS = update-env

all: $(PROGRAMS)

$(PROGRAMS): %: %.c


makeenv crc32: crc32lib.o

clean:
	rm -f $(PROGRAMS) *.o *.s


install: $(PROGRAMS) $(SCRIPTS)
	cp $^ /home/libera/nfs/testing
