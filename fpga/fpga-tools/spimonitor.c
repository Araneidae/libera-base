/* $Id: spimonitor.c 2192 2008-10-07 09:13:06Z matejk $ */
/** \file spimonitor.c */
/** SPI interface utility for BBFP. */

/*
Libera GNU/Linux DDR2 Test Writing
Copyright (C) 2004-2008 Instrumentation Technologies

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
or visit http://www.gnu.org
*/

#include <sys/types.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h> 
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <termios.h>
#include <assert.h>

/** FPGA SPI interface addresses is mapped on pagesize (0x1000). */
#define NEUTRINO_SPI_BASE_d 0x1400C018
#define MAP_SIZE_d 4096UL

/** SPI devices. */
#define SPIDAC1_d   "dac1"
#define SPIDAC2_d   "dac2"
#define SPICLKM_d   "clkm"
#define SPIFPGA_d   "fpga"

/** SPI devices Control Register values. */
#define SPIDAC1_CTRL_d 0x10100804
#define SPIDAC2_CTRL_d 0x10100808
#define SPICLKM_CTRL_d 0x08081002
#define SPIFPGA_CTRL_d 0x20201001

/** Expected command line arguments are: SPI device, r/w, CtrlData, WriteData */
#define MINARGS_d 3

/** Helper macro. Print diagnostic system message and exit. */
#define DIE_m(s) die(E_SYS, __FUNCTION__, __LINE__, s, strerror(errno))

#define MAX(a,b) ((a > b) ? a:b)
#define MIN(a,b) ((a < b) ? a:b)
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

// Memory mapping interface.
int mmap_fd = -1;
void *map_base = NULL;

// Adresses of SPI registers.
unsigned long g_target_SPICtrlReg = NEUTRINO_SPI_BASE_d;


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
/** Memory map FPGA interface.
 *  @param fd_out File descriptor of memory mapped region.
 *  @param mm_offset Starting address of FPGA interface.
 *  @param mm_size Length of FPGA interface adresses to map.
 *  @return Base address in process space of mapped region.
 */
void *cep_mapfpga(int *fd_out, off_t mm_offset, off_t mm_size) 
{
    void *map_base;
    int fd;

    if ((fd = open("/dev/mem", O_RDWR | O_SYNC)) == -1) 
    {
        DIE_m("/dev/mem.");
    }

    map_base = mmap(0, mm_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, mm_offset & ~(getpagesize()-1));
    if (map_base == ((void *) -1)) 
    {
        DIE_m("mapping /dev/mem.");
    }

    *fd_out = fd;
    return map_base;
} 
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Libera register read & write function.
inline unsigned long read_w(void *base, off_t offset) 
{
    return *((volatile unsigned long *) (base+offset));
}

inline void write_w(void *base, off_t offset, unsigned long value) 
{
    *((volatile unsigned long *) (base+offset)) = value;
    return;
}
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/** Print usage message.
 */
void usage()
{
    const char *format =
            "Usage: %s [OPTION] dac1|dac2|clkm|fpga <ctrldata> [r]|[w <wrdata>]\n"
            "\n"
            "-h         Print this message and exit.\n"
            "-o         SPI Control Register address. Default (0x%lx).\n"
            "dac1       DAC1 SPI device. SPI CtrlReg(0x%lX).\n"
            "dac2       DAC2 SPI device. SPI CtrlReg(0x%lX).\n"
            "clkm       Clock manager device. SPI CtrlReg(0x%lX).\n"
            "fpga       FPGA device. SPI CtrlReg(0x%lX).\n"
            "ctrldata   SPI Control Data register value.\n"
            "r|w        read or write to/from SPI device. Default is read.\n"
            "wrdata     Data to write to SPI Data Write register.\n"
            "\nExamples:\n"
            "%s fpga 0x1000 # Read from FPGA 0x1000 data control register.\n"
            "%s clkm 0x1 r # Read from Clock Manager 0x1 data control register.\n"
            "%s fpga 0x1001 w 0x12345678 # Write 0x12345678 to FPGA 0x1001 data control register.\n";

    fprintf(stderr, format, _argv0,
            g_target_SPICtrlReg, SPIDAC1_CTRL_d, SPIDAC2_CTRL_d, SPICLKM_CTRL_d, SPIFPGA_CTRL_d,
            _argv0, _argv0, _argv0);
}
//-----------------------------------------------------------------------------

//------------------------------------------------------------------------------
/** Cleanup function.
 *  Remove the process allocated resources.
 */
void cleanup()
{
    if (mmap_fd != -1)
        close(mmap_fd);
    
    //printf("Cleanup and exit.\n");
}
//------------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/** Program main function.
 */
int main(int argc, char *argv[])
{
    _argv0 = argv[0];

    if (argc < MINARGS_d) 
    {
        usage();
        exit(EXIT_FAILURE);
    }

    const char *optstr = "ho:";
    int ch = -1;
    while ((ch = getopt(argc, argv, optstr)) != -1)
    {
        switch (ch) 
        {
            case 'h':
                usage();
                exit(EXIT_SUCCESS);

            case 'o':
                g_target_SPICtrlReg = strtoul(optarg, 0, 0);
                break;

            default:
                exit(EXIT_FAILURE);
        }
    }

    // Install cleanup handler.
    if (atexit(cleanup) != 0)
    {
        DIE_m("atexit");
    }

    // SPI device.
    if (optind == argc) 
        die(E_NO_ARG, "dac1|dac2|clkm|fpga");

    const char *spi_dev = argv[optind++];
    unsigned long spictrl = 0;
    if (strcmp(spi_dev, SPIDAC1_d) == 0)
    {
        // Data len = 0x10 bits, control len = 0x08 bits.
        spictrl = SPIDAC1_CTRL_d;
    }
    else if (strcmp(spi_dev, SPIDAC2_d) == 0)
    {
        // Data len = 0x10 bits, control len = 0x08 bits.
        spictrl = SPIDAC2_CTRL_d;
    }
    else if (strcmp(spi_dev, SPICLKM_d) == 0)
    {
        // Data len = 0x08 bits, control len = 0x10 bits.
        spictrl = SPICLKM_CTRL_d;
    }
    else if (strcmp(spi_dev, SPIFPGA_d) == 0)
    {
        // Data len = 0x20 bits, control len = 0x10 bits.
        spictrl = SPIFPGA_CTRL_d;
    }
    else
    {
        die(E_INVALID_ARG, "dac1|dac2|clkm|fpga");
    }

    //printf("Memory mapping FPGA (0x%lX) interface address.\n",
    //       g_target_SPICtrlReg & ~(getpagesize()-1));
    map_base = cep_mapfpga(&mmap_fd, g_target_SPICtrlReg, MAP_SIZE_d);

    // All other SPI registers are based od SPI Control Register.
    g_target_SPICtrlReg &= (getpagesize()-1);
    unsigned long target_SPICtrlDataWr = g_target_SPICtrlReg + 0x4;
    unsigned long target_SPIDataWr = g_target_SPICtrlReg + 0x8;
    unsigned long target_SPICtrlDataRd = g_target_SPICtrlReg + 0xC;

    // Get control data.
    if (optind == argc)
        die(E_NO_ARG, "ctrldata");

    unsigned long ctrldata = strtoul(argv[optind++], 0, 0);

    // Check if reading from SPI device.
    if ((optind == argc) || (strcmp(argv[optind], "r") == 0))
    {
        //Read form SPI device.
        // NOTE reading from dac1 and dac2 will return garbage.

        // Set bit 15 in SPI Control Data Write to 1 for reading.
        write_w(map_base, target_SPICtrlDataWr, ctrldata | 0x00008000);
        write_w(map_base, g_target_SPICtrlReg, spictrl | 0x00000020);
        unsigned long status;
        unsigned long max_cnts = 1000;
        do
        {
            // Check SPI BUSY bit.
            status = read_w(map_base, g_target_SPICtrlReg);
            //printf("SPI status 0x%lX.\n", status);
        }
        while ((status & 0x00000080) && (--max_cnts));
        if (max_cnts == 0)
            die(E_SYS, "SPI_timeout");

        unsigned long read_result = read_w(map_base, target_SPICtrlDataRd);
        printf("Value for %s at (0x%lX): 0x%lX\n",
               spi_dev, ctrldata, read_result);

        // We are done.
        return(0);
    }

    if (strcmp(argv[optind++], "w") != 0)
        die(E_INVALID_ARG, "r|w");

    if (optind == argc) 
        die(E_NO_ARG, "wrdata");

    unsigned long wrdata = strtoul(argv[optind++], 0, 0);
    write_w(map_base, target_SPIDataWr, wrdata);
    write_w(map_base, target_SPICtrlDataWr, ctrldata);
    write_w(map_base, g_target_SPICtrlReg, spictrl);

    printf("Written 0x%lX\n", wrdata);

    return(0);
}
//-----------------------------------------------------------------------------
