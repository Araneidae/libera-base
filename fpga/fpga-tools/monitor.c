/* $Id: monitor.c 2192 2008-10-07 09:13:06Z matejk $ */
/*
 * Monitor.c: Simple program to read/write from/to any location in memory.
 */
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <ctype.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/mman.h>
  
#define FATAL do { fprintf(stderr, "Error at line %d, file %s (%d) [%s]\n", \
  __LINE__, __FILE__, errno, strerror(errno)); exit(1); } while(0)
 
#define MAP_SIZE 4096UL
#define MAP_MASK (MAP_SIZE - 1)

#define DEBUG_MONITOR 0

int parse_from_argv(int a_argc, char **a_argv, unsigned long* a_addr, int* a_type, unsigned long** a_values, ssize_t* a_len);
int parse_from_stdin(unsigned long* a_addr, int* a_type, unsigned long** a_values, ssize_t* a_len);
void read_value(unsigned long a_addr, int a_type);
void write_values(unsigned long a_addr, int a_type, unsigned long* a_values, ssize_t a_len);

void* map_base = (void*)(-1);

int main(int argc, char **argv) {
	int fd = -1;
	int retval = EXIT_SUCCESS;

	if(argc < 2) {
		fprintf(stderr,
			"\nUsage:\n"
			"\tvalues from command line   : %s address [ type [ data... ] ]\n"
			"\tvalues from standard input : %s -\n"
			"\t- address: memory address to act upon\n"
			"\t- type   : access operation type : [b]yte, [h]alfword, [w]ord, [r]awmemory\n"
			"\t- data   : a list of values to be written to the specified address\n\n",
			argv[0], argv[0]);

		return EXIT_FAILURE;
	}

	if((fd = open("/dev/mem", O_RDWR | O_SYNC)) == -1) FATAL;

	/* Read from standard input */
	if (strncmp(argv[1], "-", 1) == 0) {
		unsigned long addr;
		unsigned long *val = NULL;
		int access_type = 'w';
		ssize_t val_count = 0;
		while ( parse_from_stdin(&addr, &access_type, &val, &val_count) != -1) {
			if (addr == 0) {
				continue;
			}
			/* Map one page */
			map_base = mmap(0, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, addr & ~MAP_MASK);
			if(map_base == (void *) -1) FATAL;

			if (val_count == 0) {
				read_value(addr, access_type);
			}
			else {
				write_values(addr, access_type, val, val_count);
			}
			if (map_base != (void*)(-1)) {
				if(munmap(map_base, MAP_SIZE) == -1) FATAL;
				map_base = (void*)(-1);
			}
#if DEBUG_MONITOR
			printf("addr/type: %lu/%c\n", addr, access_type);

			printf("val (%ld):", val_count);
			for (ssize_t i = 0; i < val_count; ++i) {
				printf("%lu ", val[i]);
			}
			if (val != NULL) {
				free(val);
				val = NULL;
			}
			printf("\n");
#endif
		}
		goto exit;
	}
	/* Read from command line */
	else {
		unsigned long addr;
		unsigned long *val = NULL;
		int access_type = 'w';
		ssize_t val_count = 0;
		parse_from_argv(argc, argv, &addr, &access_type, &val, &val_count);

		/* Map one page */
		map_base = mmap(0, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, addr & ~MAP_MASK);
		if(map_base == (void *) -1) FATAL;
			
		if (addr != 0) {
			if (val_count == 0) {
				read_value(addr, access_type);
			}
			else {
				write_values(addr, access_type, val, val_count);
			}
		}
		if (map_base != (void*)(-1)) {
			if(munmap(map_base, MAP_SIZE) == -1) FATAL;
			map_base = (void*)(-1);
		}
#if DEBUG_MONITOR
		printf("addr/type: %lu/%c\n", addr, access_type);

		printf("val (%ld):", val_count);
		for (ssize_t i = 0; i < val_count; ++i) {
			printf("%lu ", val[i]);
		}
		
		if (val != NULL) {
			free(val);
			val = NULL;
		}
		printf("\n");
#endif
	}

exit:

	if (map_base != (void*)(-1)) {
		if(munmap(map_base, MAP_SIZE) == -1) FATAL;
	}
	if (fd != -1) {
		close(fd);
	}
	
	return retval;
}

void read_value(unsigned long a_addr, int a_type) {
	void* virt_addr = map_base + (a_addr & MAP_MASK);
	unsigned long read_result = 0;

	switch(a_type) {
		case 'b':
			read_result = *((unsigned char *) virt_addr);
			printf("Value at address 0x%lX (%p): 0x%lX\n",
					a_addr, virt_addr, read_result);
			break;
		case 'h':
			read_result = *((unsigned short *) virt_addr);
			printf("Value at address 0x%lX (%p): 0x%lX\n",
					a_addr, virt_addr, read_result);
			break;
		case 'w':
			// When changing remember that BbB GUI is parsing this output.
			read_result = *((unsigned long *) virt_addr);
			printf("Value at address 0x%lX (%p): 0x%lX\n",
					a_addr, virt_addr, read_result);
			break;
		case 'r':
			read_result = *((unsigned long *) virt_addr);
			printf("0x%08lx\n", read_result);
			break;
		default:
			fprintf(stderr, "Illegal data type '%c'.\n", a_type);
			return;
	}
	fflush(stdout);
}

void write_values(unsigned long a_addr, int a_type, unsigned long* a_values, ssize_t a_len) {
	void* virt_addr = map_base + (a_addr & MAP_MASK);

	for (ssize_t i = 0; i < a_len; ++i) {
		switch(a_type) {
			case 'b':
				*((unsigned char *) virt_addr) = a_values[i];
				break;
			case 'h':
				*((unsigned short *) virt_addr) = a_values[i];
				break;
			case 'w':
				*((unsigned long *) virt_addr) = a_values[i];
				break;
		}
	}
	if (a_len == 1) {
		printf("Written 0x%lX\n", a_values[0]);
	}
	else {
		printf("Written %d values\n", a_len);
	}
	fflush(stdout);
}

int parse_from_stdin(unsigned long* a_addr, int* a_type, unsigned long** a_values, ssize_t* a_len) {
	char* line = NULL;
	size_t len = 0;
	ssize_t ret = 0;
	ssize_t val_count = 0;

	*a_addr = 0;
	*a_values = calloc(4*1024, sizeof(unsigned long));
	
	while ((ret = getline(&line, &len, stdin)) != -1) {
		if (line[0] == '\n') {
			break;
		}
		{
			char* token;
			token = strtok(line, " \t");
			if (token == NULL) {
				break;
			}
			if (*a_addr == 0) {
				*a_addr = strtoul(token, 0, 0);
				token = strtok(NULL, " \t");
				if (token == NULL) {
					break;
				}
				*a_type = tolower(token[0]);
			}
			else {
				(*a_values)[val_count] = strtoul(token, 0, 0);
				++val_count;
			}

			for (; ; ++val_count) {
				token = strtok(NULL, " \t");
				if (token == NULL)
					break;
				(*a_values)[val_count] = strtoul(token, 0, 0);
			}
		}
	}

	if (line) {
		free(line);
	}

	*a_len = val_count;
	
	if (ret == -1) {
		if (errno != 0) {
			FATAL;
		}
		else if (*a_addr != 0) {
			return val_count;
		}
		return -1;
	}
	else {
		return val_count;
	}
}

int parse_from_argv(int a_argc, char **a_argv, unsigned long* a_addr, int* a_type, unsigned long** a_values, ssize_t* a_len) {

	int val_count = 0;
	
	*a_addr = strtoul(a_argv[1], 0, 0);
	*a_values = calloc(4*1024, sizeof(unsigned long));

	if (a_argc > 2) {
		*a_type = tolower(a_argv[2][0]);
	}

	for (int i = 3; i < a_argc; ++i, ++val_count) {
		(*a_values)[val_count] = strtoul(a_argv[i], 0, 0);
	}

	*a_len = val_count;
	return 0;
}

