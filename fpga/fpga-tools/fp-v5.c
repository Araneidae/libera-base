/* $Id: fp-v5.c 2192 2008-10-07 09:13:06Z matejk $ */

/** \file fp-v5.c */

/*
 fpgaLoad -
*/

/*
 * Libera GNU/Linux program for loading the prom file into the FPGA Virtex5 on the
 * ADC12-500 board through the Virtex2 on the digital board
 *
 * Copyright (C) 2004-2006 Instrumentation Technologies
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
 * or visit http://www.gnu.org
 *
 */

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sched.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <fcntl.h>
#include <ctype.h>
#include <time.h>
#include <assert.h>

#define NEUTRINO_BASE_d     0x14000000
#define NEUTRINO_END_d      0x1403BFFF
#define NEUTRINO_SIZE_d     (NEUTRINO_END_d - NEUTRINO_BASE_d)

#define FPGA_LOAD_CTRL      0xC028            // 500MHz Digitizer FPGA Load Control
#define FPGA_LOAD_DATA      0xC02C            // 500MHz Digitizer FPGA Load Data

#define INIT_B              (1 << 18)
#define DONE                (1 << 17)
#define BUSY                (1 << 16)
#define MAN_MODE            (1 << 12)
#define PROG_B              (1 << 8)
#define MAN_DATA            (1 << 4)
#define MAN_CLK             (1 << 0)

#define STREAM_DATA_SIZE    1024
#define MAX_WAIT            1000
#define WAIT_SAMPLING       10

/** Helper macro. Print diagnostic system message and exit. */
#define DIE_m(s) die(E_SYS, __FUNCTION__, __LINE__, s, strerror(errno))

//#define __DEVEL

//-----------------------------------------------------------------------------
// Program name.
const char *_argv0 = 0;

// Error codes. See error[] for descriptions.
enum {
    E_INVALID_ARG = 0,
    E_NO_ARG,
    E_SYS,
};

// Error descriptions corresponding to error codes.
const char *error[] =
{
    "invalid argument -- %s",
    "missing argument -- %s",
    "system error in function `%s': line %d: %s -- %s",
};

int mem_fd = -1;
void *map_base = MAP_FAILED;

//-----------------------------------------------------------------------------
/** Print failure message and exit.
 */
void die(int n, ...)
{
    fprintf(stderr, "%s: ", _argv0);

    va_list ap;

    va_start(ap, n);
    vfprintf(stderr, error[n], ap);
    va_end(ap);

    fputc('\n', stderr);
    exit(EXIT_FAILURE);
}

//-----------------------------------------------------------------------------
/** /dev/mem access initialization.
 *
 * On success, returns 0.
 * On failure, returns -1.
 */
int fpga_rw_init(unsigned long base_addr)
{
    mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (mem_fd == -1) {
        DIE_m("/dev/mem.");
    }

    map_base = mmap(0, NEUTRINO_SIZE_d, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, base_addr & ~(getpagesize()-1));
    if(map_base == MAP_FAILED) {
        DIE_m("mapping /dev/mem.");
    }

    return 0;
}

//-----------------------------------------------------------------------------
/** Writes a 32-bit value to fpga.
 *
 * Writes the vaule @param val to address offset @param off.
 */
void fpga_write(unsigned long off, unsigned long val)
{
    *((volatile unsigned long *) (map_base + off)) = val;

#ifdef __DEVEL
    _LOG_DEBUG("W(0x%08lx) = 0x%08lx\n", (unsigned long)(off + NEUTRINO_BASE_d), val);
#endif
}

//-----------------------------------------------------------------------------
/** Reads a 32-bit value from fpga.
 *
 * Returns the vaule, read from address offset @param off.
 */
unsigned long fpga_read(unsigned long off)
{
    unsigned long val = *((volatile unsigned long *) (map_base + off));

#ifdef __DEVEL
    _LOG_DEBUG("R(0x%08lx) = 0x%08lx\n", (unsigned long)(off + NEUTRINO_BASE_d), val);
#endif

    return val;
}

//-----------------------------------------------------------------------------
/** /dev/mem access cleanup.
 *
 * On success, returns 0.
 * On failure, returns -1.
 */
int fpga_rw_cleanup()
{
    munmap(map_base, NEUTRINO_SIZE_d);
    close(mem_fd);

    return 0;
}

//------------------------------------------------------------------------------
/** Cleanup function.
 *  Remove the process allocated resources.
 */
void cleanup()
{
    fpga_rw_cleanup();
}
//------------------------------------------------------------------------------

/*
  PROG_B  -----\______/-----------------------------------------------
  INIT_B  -------\______/---------------------------------------------
  DATA    ~~~~~~~~~~~~~~~~~~======x=========x=========x=========~~~~~~
  DONE_________________________________________________________/------
*/

int main(int argc, char **argv)
{
    unsigned int StreamData[STREAM_DATA_SIZE];
    unsigned int len, i;
    int blocks = 0;
    int bytes_read = 0;
    unsigned int data_out;
    unsigned int curr_int;

    // Install cleanup handler.
    if (atexit(cleanup) != 0) {
        DIE_m("atexit");
    }

    // If init fails the program will exit there.
    fpga_rw_init(NEUTRINO_BASE_d);

    //------------------- initialization sequence -----------------------------

    // set PROG_B high and wait to stabilize
    fpga_write(FPGA_LOAD_CTRL, MAN_MODE | PROG_B);
    usleep(500);

    //..........................................................................
    // set PROG_B low and wait a little
    fpga_write(FPGA_LOAD_CTRL, MAN_MODE);
    usleep(100);

    // check if INIT_B is low too
    i = 0;
    while ((i < MAX_WAIT) && (fpga_read(FPGA_LOAD_CTRL) & INIT_B)) {
        usleep(WAIT_SAMPLING);
        i++;
    }

    // timeout expired
    if (i >= MAX_WAIT) {
        fprintf(stderr,"FAILURE: missing INIT_B response. \n");
        exit(EXIT_FAILURE);
    }

    //..........................................................................
    // set PROG_B high
    fpga_write(FPGA_LOAD_CTRL, MAN_MODE | PROG_B);

    // check if INIT_B is high too
    i = 0;
    while ((i < MAX_WAIT) && ((fpga_read(FPGA_LOAD_CTRL) & INIT_B) == 0)) {
        usleep(WAIT_SAMPLING);
        i++;
    }

    // timeout expired
    if (i >= MAX_WAIT) {
        fprintf(stderr,"FAILURE: INIT_B does not follow up PROG_B \n");
        exit(EXIT_FAILURE);
    }

    //..........................................................................
    // wait a bit before data transfer
    usleep(1000);

    // transfer in auto mode
    fpga_write(FPGA_LOAD_CTRL, PROG_B);


    //------------------- data transfer ---------------------------------------
    while ((len = fread(StreamData, 4, STREAM_DATA_SIZE, stdin)) > 0) {
        bytes_read += len * 4;
        for (i=0; i < len; i++) {

            // wait till serial register not empty
            while (fpga_read(FPGA_LOAD_CTRL) & BUSY);

            // byte swap
            curr_int = StreamData[i];
            data_out = (curr_int & 0x000000FF) << 24;
            data_out |= (curr_int & 0x0000FF00) << 8;
            data_out |= (curr_int & 0x00FF0000) >> 8;
            data_out |= (curr_int & 0xFF000000) >> 24;

            // write data into serial register
            fpga_write(FPGA_LOAD_DATA, data_out);
        }
        blocks ++;
    }

    //------------------- wait for DONE signal --------------------------------
    i = 0;
    while ((i < MAX_WAIT) && ((fpga_read(FPGA_LOAD_CTRL) & DONE) == 0)) {
        usleep (WAIT_SAMPLING);
        i++;
    }

    if (i >= MAX_WAIT) {
        fprintf(stderr,"FAILURE: FPGA image load failed. \n");
        exit(EXIT_FAILURE);
    }

    return EXIT_SUCCESS;
    // cleanup is automatically called at program exit.
}




