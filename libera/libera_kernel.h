/* $Id: libera_kernel.h 2361 2008-12-12 12:26:13Z tomazb $ */

/** \file libera_kernel.h */
/** Driver kernel interface for GNU/Linux Libera driver. */

/*
LIBERA - Libera GNU/Linux device driver
Copyright (C) 2004-2006 Instrumentation Technologies

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

#ifndef _LIBERA_KERNEL_H_
#define _LIBERA_KERNEL_H_

/* This should only be included in kernel space */
#ifdef __KERNEL__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/ioport.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/version.h>
#include <asm/io.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26)
#include <linux/math64.h>
#else
#include <linux/calc64.h>
#define div_u64(A, B) do_div_llr(A, B, &dummy_return_value)
#define div_u64_rem(A, B, C) do_div_llr(A, B, (unsigned long *) C)
extern unsigned long dummy_return_value;
#endif
#include "libera.h"


/** Libera GNU/Linux driver version */
#ifndef RELEASE_VERSION
#define RELEASE_VERSION unknown
#endif

/* Default module parameters */
#define LIBERA_IOBASE           0x14000000
#define LIBERA_IORANGE          0x00040000 /* 16 x 16k = 256k */
#define LIBERA_IRQ_GPIO_NR      1
#define LIBERA_FLMC_INITIAL     1080000000UL  /* dHz = 10^-1 Hz */
#define LIBERA_D_INITIAL        128    /* Decimation */
#define LIBERA_DFA_INITIAL      0      /* FA Decimation */
#define LIBERA_KADC_INITIAL     1000   /* ADC interlock scaling factor x1000 */
#define LIBERA_DUR_INITIAL      1000   /* ILK and PM overflow duration limit */
#define LIBERA_PMSIZE_INITIAL   16384  /* Post Mortem buffer size (DD atoms) */
#define LIBERA_PMSIZE_MAX       262144 /* Post Mortem buffer limit (DD atoms) */

/* Libera SA device structures */
#define LIBERA_SA_MAX_READERS  16

/**********************/
/* Register addresses */
/**********************/
/* General SBC interface offset from LIBERA_IOBASE */
#define SBC_OFFSET   0x00000000
/* Timing (CLK) interface offset from LIBERA_IOBASE */
#define CLK_OFFSET   0x00004000
/* ADC-rate buffer offset from LIBERA_IOBASE */
#define ADC_OFFSET   0x00008000
/* Digital signal conditioning (DSC) offset from LIBERA_IOBASE */
#define DSC_OFFSET   0x0000c000
/* Digital Down Converter (DDC) offset from LIBERA_IOBASE */
#define DDC_OFFSET   0x00014000
/* Storage and Circular Buffer offset from LIBERA_IOBASE */
#define HB_OFFSET    0x00018000
/* Fast Acquisition offset from LIBERA_IOBASE */
#define FA_OFFSET    0x0001c000
/* Slow Acquisition offset from LIBERA_IOBASE */
#define SA_OFFSET    0x00020000
/* Interlock offset from LIBERA_IOBASE */
#define ILK_OFFSET   0x00024000
/* Fast Application Interface offset from LIBERA_IOBASE */
#define FAI_OFFSET   0x00028000

/* Signal conditioning offset from LIBERA_IOBASE */
#define SIGNAL_OFFSET 0x0001c000
/* Housekeeping components (msp430,fan, diagnostics &.) offset from LIBERA_IOBASE */
#define HOUSEKEEP_OFFSET 0x00008000
/* SDRAM controller offset from LIBERA_IOBASE */
#define SDRAM_OFFSET 0x00004000


/* Transform operator prototypes */
typedef int (*libera_operator_t)( const void *in, void *out );

/* Mirroring of Write-Only registers */
struct wom_timing_t
{
    libera_U32_t swTrigger;
    libera_U32_t triggerMask;
};
struct wom_fa_t
{
    libera_U32_t BlockSizeW;
    libera_U32_t BlockSizeR;
    libera_U32_t StartAddressW;
    libera_U32_t StartAddressR;
    libera_U32_t DataToFPGA[511];
};
struct wom_sbc_t
{
    libera_U32_t irqMask;
};
struct wo_mirror_t
{
    struct wom_timing_t timing;
    struct wom_fa_t fa;
    struct wom_sbc_t sbc;
};


/* DD pages defines */
#define DD_HB_PAGE     32  /* HB page size = 1024 bytes = 32 atoms (atom = 8*4 bytes) */
#define DD_MAX_NOATOMS 0x400000 /* Max. No. of requested Atoms from HB */

/** Libera Quad timestamp MT, ST, LMT & LST */
typedef struct 
{
    libera_timestamp_t  U;
    libera_Ltimestamp_t L;
    int request;
    loff_t relative;
    libera_Ltimestamp_t lasttrig;
    loff_t trigoffset;
} libera_Qtimestamp_t;


/* All addresses must be defined as offsets from LIBERA_IOBASE */
/* DDC register map */
#define DDC_NCO           0x4 + DDC_OFFSET
/* DDC Moving Average Filter parameters */
#define DDC_MAF_LENGTH    0xc  + DDC_OFFSET
#define DDC_MAF_DELAY     0x10 + DDC_OFFSET

/* SC timing module register map */
#define T_SC_STATUSL      0x0  + CLK_OFFSET
#define T_SC_STATUSH      0x4  + CLK_OFFSET
#define T_SC_EVENTGENL    0x0  + CLK_OFFSET
#define T_SC_EVENTGENH    0x4  + CLK_OFFSET
#define T_SC_EVENTGEN     0x8  + CLK_OFFSET
#define T_SC_FIFOADV      0x8  + CLK_OFFSET
#define T_SC_TRIGGER_MASK 0x18 + CLK_OFFSET

/* MC timing module register map */
#define T_MC_STATUSL      0xc  + CLK_OFFSET
#define T_MC_STATUSH      0x10 + CLK_OFFSET
#define T_MC_EVENTGENL    0xc  + CLK_OFFSET
#define T_MC_EVENTGENH    0x10 + CLK_OFFSET

#define T_MC_FIFOADV      0x14 + CLK_OFFSET
#define T_MC_EVENTGEN     0x14 + CLK_OFFSET
#define T_MC_TRIGGER_MASK 0x1c + CLK_OFFSET

#define T_TRIG_DELAY      0x38 + CLK_OFFSET

/* Trigger numbers */
#define T_TRIG_TIME_64_EXT   9
#define T_TRIG_TRIGGER       8
#define T_TRIG_POST_MORTEM   7
#define T_TRIG_MC_PRESCALER  6
#define T_TRIG_SC_PRESCALER  5
#define T_TRIG_CTIME         4
#define T_TRIG_FA3           3
#define T_TRIG_FA2           2
#define T_TRIG_FA1           1
#define T_TRIG_FA0           0

/* Event generation bits */
#define T_EG_DDC              31
#define T_EG_HB               30
#define T_EG_FA               29
#define T_EG_SA               28
#define T_EG_FAI              25

/* SPI DAC */
#define EVENT_SC_DAC      0x20 + CLK_OFFSET
#define EVENT_MC_DAC      0x24 + CLK_OFFSET

/* FA defines */
#define FA_POLYPHASE_FIR 0x14   + FA_OFFSET
#define FA_NOTCH1        0x18   + FA_OFFSET
#define FA_NOTCH2        0x1c   + FA_OFFSET
#define FA_FIFO_RESET    0x20   + FA_OFFSET
#define FA_NSUMS         0x24   + FA_OFFSET
#define FA_SUMSUMS_LSW   0x28   + FA_OFFSET
#define FA_SUMSUMS_MSW   0x2C   + FA_OFFSET
#define FA_ENABLE        0x30   + FA_OFFSET
#define FA_AVE_STOP      0x34   + FA_OFFSET
#define FA_AVE_WIN       0x38   + FA_OFFSET
#define FA_START         0x3C   + FA_OFFSET
#define FA_WINDOW        0x40   + FA_OFFSET
#define FA_BLOK_RAM      0x44   + FA_OFFSET

/* SA defines */
#define SA_CONTROL       0      + SA_OFFSET
#define SA_ENABLE        0x0
#define SA_STOP          0x1
#define SA_STATUS        0x4    + SA_OFFSET
#define SA_FIFO_BASE     0x1000 + SA_OFFSET

#define SA_INOVR_LSB    31
#define SA_SIZE_LSB      0
#define SA_INOVR_WIDTH   1
#define SA_INOVR(status) ( (status >> SA_INOVR_LSB) & ((1 << SA_INOVR_WIDTH)-1) )
#define SA_SIZE(status)  ( (status >> SA_SIZE_LSB)  & ((1 << SA_SIZE_WIDTH)-1)  )


/* Interlock defines */
#define ILK_CONTROL          0x0  + ILK_OFFSET
#define ILK_XLOW             0x8  + ILK_OFFSET
#define ILK_XHIGH            0xc  + ILK_OFFSET
#define ILK_YLOW             0x10 + ILK_OFFSET
#define ILK_YHIGH            0x14 + ILK_OFFSET
#define ILK_STATUS           0x18 + ILK_OFFSET
#define PM_XLOW              0x20 + ILK_OFFSET
#define PM_XHIGH             0x24 + ILK_OFFSET
#define PM_YLOW              0x28 + ILK_OFFSET
#define PM_YHIGH             0x2C + ILK_OFFSET


/* FAI defines */
#define FAI_BLOCK            0x0  + FAI_OFFSET
#define FAI_BLOCKSIZE        0x14000
#define FAI_CORR_BASE        0x14000    + FAI_OFFSET
#define FAI_SW_STATUS        0x14024    + FAI_OFFSET
#define FAI_COUNTER_RESET    0x14028    + FAI_OFFSET
#define FAI_PLL_STATUS_MASK  0x00000003UL


/* DSC defines */
#define DSC_BLOCK            0x0  + ADC_OFFSET
#define DSC_BLOCKSIZE        0xc000


/* DD defines */
#if defined BBFP
#define DD_CB_FIFO_POSTFILT   0x0  + HB_OFFSET
#define DD_CB_FIFO_STARTADDR  0x4  + HB_OFFSET
#define DD_CB_FIFO_ADDRSTEP   0x8  + HB_OFFSET
#define DD_CB_FIFO_SNGLSIZE   0xc  + HB_OFFSET
#define DD_CB_FIFO_NOTRANS    0x10 + HB_OFFSET
#elif defined HBPP
#define DD_CB_FIFO_POSTFILT   0x0  + HB_OFFSET
#define DD_CB_FIFO_STARTADDR  0x4  + HB_OFFSET
#define DD_CB_FIFO_ADDRSTEP   0x8  + HB_OFFSET
#define DD_CB_FIFO_SNGLSIZE   0xc  + HB_OFFSET
#define DD_CB_FIFO_NOTRANS    0x10 + HB_OFFSET
#else
#define DD_CB_FIFO_L        0x0  + HB_OFFSET
#define DD_CB_FIFO_H        0x4  + HB_OFFSET
#endif

#if defined BBFP
#define DD_CB_ADDR_OVERFLOW_CNT		(0x20 + HB_OFFSET)
#else
#define DD_CB_FIFO_RESET		(0x20 + HB_OFFSET)
#endif

#define DD_CBH_FDEC_LSB      30
#define DD_CBH_SADDR_LSB      0
#define DD_CBH_FDEC_WIDTH     2
#define DD_CBH_SADDR_WIDTH   21

#define DD_CBL_NOAT_LSB     0
#define DD_CBL_EADDR_LSB    0
#define DD_CBL_NOAT_WIDTH  22
#define DD_CBL_EADDR_WIDTH  8

#define DD_CBH_DEC_64       1
#define DD_CBH_DEC_1        0

#if defined BBFP
#define DD_OB_STATUS        0x14   + HB_OFFSET
#define DD_OB_FIFOBASE      0x1000 + HB_OFFSET
#elif defined HBPP
#define DD_OB_STATUS        0x14   + HB_OFFSET
#define DD_OB_FIFOBASE      0x1000 + HB_OFFSET
//#define DD_OB_STATUS        0x27800
//#define DD_OB_FIFOBASE      0x24000
#else
#define DD_OB_STATUS        0x8    + HB_OFFSET
#define DD_OB_FIFOBASE      0x1000 + HB_OFFSET
#endif

#define DD_OB_BUSY_LSB     31
#define DD_OB_OVERRUN_LSB  30
#define DD_OB_SIZE_LSB      0
#define DD_OB_BUSY_WIDTH    1
#define DD_OB_OVERRUN_WIDTH 1
#if defined BBFP
#define DD_OB_SIZE_WIDTH    11
#elif defined HBPP
#define DD_OB_SIZE_WIDTH    11
//#define DD_OB_SIZE_WIDTH    9
#else
#define DD_OB_SIZE_WIDTH    8
#endif

// TODO: Consider BUSY & OVERRUN without shifting
#define DD_OB_BUSY(status)   ( (status >> DD_OB_BUSY_LSB)   & ((1 << DD_OB_BUSY_WIDTH)-1)  )
#define DD_OB_OVERRUN(status)   ( (status >> DD_OB_OVERRUN_LSB)   & ((1 << DD_OB_OVERRUN_WIDTH)-1)  )
#define DD_OB_SIZE(status)   ( (status >> DD_OB_SIZE_LSB)   & ((1 << DD_OB_SIZE_WIDTH)-1)  )


/* IRQ defines */
#define SBC_IRQ_MASK     0 + SBC_OFFSET //write
#define SBC_IRQ          0 + SBC_OFFSET
#define SBC_IRQ2         0x28 + CLK_OFFSET

/* FPGA info defines */ 
#define FPGA_INFO_COMPILE_TIME  0x4  + SBC_OFFSET
#define FPGA_INFO_BUILDNO       0x8  + SBC_OFFSET

#define FPGA_INFO_ID            0xc  + SBC_OFFSET
#define FPGA_ID_BRILLIANCE      0x10
#define FPGA_ID_ELECTRON        0x1

#define FPGA_INFO_DEC_DDC       0x10 + SBC_OFFSET
#define FPGA_INFO_DEC_CIC_FIR   0x14 + SBC_OFFSET
#define FPGA_FEATURE_CUSTOMER   0x18 + SBC_OFFSET
#define FPGA_FEATURE_ITECH      0x1C + SBC_OFFSET

/* Attenuation */
#define ATTN_VAL_SIZE    5
#define ATTN_VAL_MASK    ((1 << ATTN_VAL_SIZE) - 1)

/* M3 DSC */
#define DSC_MODE                   0x0  + DSC_OFFSET
#define DSC_ANALOG_SW              0x4  + DSC_OFFSET
#define DSC_DIGITAL_SW             0x10 + DSC_OFFSET
#define DSC_ATTN_L                 0x8  + DSC_OFFSET
#define DSC_ATTN_H                 0xc  + DSC_OFFSET
#define DSC_ILK_GAIN_LIMIT         0x18 + DSC_OFFSET
#define DSC_ILK_OVERFLOW_LIMIT     0x1c + DSC_OFFSET
#define DSC_ILK_OVERFLOW_DUR       0x20 + DSC_OFFSET
#define DSC_SW_SOURCE              0x38 + DSC_OFFSET
#define DSC_SW_DELAY               0x3c + DSC_OFFSET
#define DSC_PM_OVERFLOW_LIMIT      0x40 + DSC_OFFSET
#define DSC_PM_OVERFLOW_DUR        0x44 + DSC_OFFSET
#define DSC_SW_EXTERNAL            (1 << 31)

#define ATTN7_LSB        24
#define ATTN6_LSB        16
#define ATTN5_LSB         8
#define ATTN4_LSB         0
#define ATTN3_LSB        24
#define ATTN2_LSB        16
#define ATTN1_LSB         8
#define ATTN0_LSB         0
#define DSC_MODE_DIGITAL  (1<<1)
#define DSC_MODE_ATTN     (1<<2)
#define DSC_MODE_ANALOG   (1<<3)
#define DSC_MODE_MANUAL   0
#define DSC_MODE_AUTO     1

/* ADC-rate buffer */
#define ADC_IRQ_STATUS    0x0    + ADC_OFFSET
#define ADC_MAX           0x4    + ADC_OFFSET
#define ADC_CHAN_AB       0x2000 + ADC_OFFSET
#define ADC_CHAN_CD       0x3000 + ADC_OFFSET
#define ADC_OVERFLOW      0x4000 + ADC_OFFSET
#define ADC_HW_LENGTH     1024

/* Switches */
#define SWITCH_VAL_SIZE    4
#define SWITCH_VAL_LSB    19
#define SWITCH_VAL_MASK    ((1 << SWITCH_VAL_SIZE) - 1)
#define ATTN_SWITCH_MASK   (SWITCH_VAL_MASK << SWITCH_VAL_LSB)

#define ATTN_NUM(i)  ((ATTN_CHANNELS - i - 1) << ATTN_NUM_LSB)

/* The module name. Used for logging. */
#define LIBERA_NAME "libera"


/* Major and minor numbers for device files */
/* NOTE: Major numbers in ranges 60-63, 120-127 and 240-254 
   are reserved for experimental use.
   For the inclusion in the official linux kernel one has to
   apply for the assignment of a unique major number. 
*/
#define LIBERA_MAJOR      240
#define LIBERA_MINOR_CFG    0
#define LIBERA_MINOR_DD     1
#define LIBERA_MINOR_FA     2
#define LIBERA_MINOR_PM     3
#define LIBERA_MINOR_SA     4
#define LIBERA_MINOR_EVENT  5
#define LIBERA_MINOR_ADC    6
#define LIBERA_MINOR_DSC    7

#define LIBERA_MAX_MINOR    7


/* Libera CFG device parameters */
#define LIBERA_CFG_PARAMS_MAX     512


/* Libera interrupt cause */
#define LIBERA_INTERRUPT_ADC_MASK     (1 << 0)
#define LIBERA_INTERRUPT_SC_MASK      (1 << 1)
#define LIBERA_INTERRUPT_HELPSC_MASK  (1 << 2)
#define LIBERA_INTERRUPT_MC_MASK      (1 << 3)
#define LIBERA_INTERRUPT_HELPMC_MASK  (1 << 4)
#define LIBERA_INTERRUPT_DD_MASK      (1 << 5)
#define LIBERA_INTERRUPT_SA_MASK      (1 << 6)
#define LIBERA_INTERRUPT_FA_MASK      (1 << 7)
#define LIBERA_INTERRUPT_ILK_MASK     (1 << 8)

/* Libera DD time definition mode */
enum dd_time_t
{
    LIBERA_DD_TIME_IMPLICIT, /* MC & SC defined by external trigger */
    LIBERA_DD_TIME_EXPLICIT_ST, /* Request given in ST */
    LIBERA_DD_TIME_EXPLICIT_MT, /* Request given in MT */
};
#define NS_IN_SECOND        1000000000UL /* 1e9 nano seconds in 1 second */
#define LSC_FREQ            125000000UL  /* Libera Machine clock frequency   */
#define FLMC_DECI_HZ        10
#define DELTA_LST_OVERFLOW  15707377440ULL
#define DELTA_LMT_OVERFLOW  14757395240ULL


/* Libera DMA */
typedef struct
{
    int chan;
    libera_atom_dd_t *buf;
    long put, get;
    long csize, remaining;
    unsigned long obFIFOstatus;
    unsigned long Overrun;
    unsigned long DMAC_transfer;
    unsigned long aborting;
    size_t written;
} libera_dma_t;


/* Libera global parameters */
struct libera_global
{
    libera_U32_t event_mask;
    int irq;
    libera_dma_t dma;
    struct wo_mirror_t wom;
    unsigned long feature;
    unsigned long d;
    unsigned long dfa;
    unsigned long dcic;
    unsigned long num_dfir;
};


/*
 * Device structures for each libera device.
 * Part of device structure is the same for all devices
 * (LIBERA_COMMON_DEV) and the rest is specific for each device.
 */

/* Common items in libera device structures */
#define LIBERA_COMMON_DEV             \
    int minor;                        \
    int open_count;                   \
    unsigned int writers;             \
    unsigned int readers;             \
    struct file *master_filp;         \
    spinlock_t spinlock;              \
    struct mutex sem;             \
    struct libera_global *global;


/* Libera CFG device structure */
struct libera_cfg_device
{
    LIBERA_COMMON_DEV
    unsigned int param[LIBERA_CFG_PARAMS_MAX];
    int pm_freezed;
};

#define TRIG_ALL_MASK 0x1FF << 22  /* 9 bit TRIGGER vector */
#define TRIG_EVENTS_MAX 11
#define TRIG_LOG 4096              /* this must be a power of 2 */
#define TRIG_LOG_MASK (TRIG_LOG - 1)
#define SA_LOG_MASK (SA_LOG - 1)


struct libera_fifo 
{
    unsigned long long stamp[TRIG_LOG];
    long put, get;
};


struct libera_event_fifo 
{
    libera_event_t data[TRIG_LOG];
    long put, get;
};

struct libera_sa_fifo 
{
    libera_atom_sa_t data[SA_LOG];
    long put, get;
};

struct libera_circbuf
{
    libera_Ltimestamp_t stamp[TRIG_LOG];
    unsigned int put;
};


#define GLITCH_HISTORY 8
#define GLITCH_LOG_MASK (GLITCH_HISTORY - 1)
#define GT_SC_JIFF_LOW   8
#define GT_SC_JIFF_HIGH 20
#define GT_MC_JIFF_LOW   8
#define GT_MC_JIFF_HIGH 20
#define GT_SC_TRIG10_LOW  10500000
#define GT_SC_TRIG10_HIGH 12600000
#define GT_MC_TRIG10_LOW  10500000
#define GT_MC_TRIG10_HIGH 12500000

typedef struct {
    unsigned long long stamp;
    unsigned long th;
    unsigned long tl;
    unsigned long jiff_time;
} glitch_times_t;

typedef struct {
    glitch_times_t gt[GLITCH_HISTORY];
    long put;
} glitch_CB_t;


/* Libera DD device structure */
#define LIBERA_DD_MAX_INTERVAL   DD_MAX_NOATOMS /* Atoms, not bytes. */
#define LIBERA_DD_CIRCBUF_BYTES  (LIBERA_CIRCBUF_ATOMS*sizeof(libera_atom_dd_t))

/* How much time in advance can we request data, when explictly defining 
 * the start interval. In atoms = MC cycles. */ 
#define LIBERA_DD_MAX_INADVANCE  (5*LIBERA_DD_CIRCBUF_ATOMS)
#define DD_WAIT_STEP             1  /* jiffies */
#define DD_WAIT_TIMEOUT          2  /* No. of DD_WAIT_STEP cycles */
#define LIBERA_DD_CIRCBUF_SAFE   ((LIBERA_DD_CIRCBUF_ATOMS * lgbl.d)/10)  /* CB 10% safety margin */
#define LIBERA_DD_READSYNC_MARGIN 64 /* PUT - GET pointer margin in atoms */
#define LIBERA_YIELD_INTERVAL     10 /* Yield interval - jiffies */

typedef struct
{
    unsigned long dec;
    libera_Qtimestamp_t Qts;
    libera_timestamp_t tstamp;

    libera_hw_time_t arm_lmt;
    /** Trigger position since the ARM in sample units */
    loff_t trig_position;
} libera_dd_local_t;

struct libera_dd_device
{
    LIBERA_COMMON_DEV
    libera_U32_t decimation;
    wait_queue_head_t DD_queue;
    struct libera_fifo dd_irqevents;
    wait_queue_head_t DMA_queue;
    int pm_entry;
};

/* Libera FA device structure */
struct libera_fa_device
{
    LIBERA_COMMON_DEV
    unsigned char buf[FAI_BLOCKSIZE];
};

/* Libera PM device structure */
struct libera_pm_device
{
    LIBERA_COMMON_DEV
    libera_atom_dd_t *buf;
    libera_Ltimestamp_t PMevent;
    libera_timestamp_t tstamp;
};


struct sa_local {
	struct mutex sem;
	wait_queue_head_t wait;
	struct libera_sa_fifo pipe;
};

struct libera_registered_pipes
{
    struct sa_local *sa_pipe[LIBERA_SA_MAX_READERS];
    int registered;
    int next;
};  

struct libera_sa_device
{
    LIBERA_COMMON_DEV
    struct libera_registered_pipes pipe;  
    libera_atom_sa_t buf[SA_FIFO_DEPTH];    
    volatile libera_atom_sa_t *buf_head;
    volatile libera_atom_sa_t *buf_tail;
    unsigned inputovr;
};

/* Libera EVENT device structures */
struct libera_event_local
{
    unsigned long sc_trigVec;
    unsigned long mc_trigVec;
};

#define LIBERA_SETTIME_ST  (1 << 0)
#define LIBERA_SETTIME_MT  (1 << 1)
#define FF_PERIOD             15000 // TODO
typedef struct {
    unsigned int update;
    struct {
	libera_hw_time_t ref;
	libera_hw_time_t off;
        libera_hw_time_t scphi;
    } lst;
    struct {
	libera_hw_time_t ref;
        libera_hw_time_t off_all;
	libera_hw_time_t off;
	libera_hw_time_t off_pll;
	libera_hw_time_t mcphi;
    } lmt;
} libera_settime_t;

typedef struct evgen {
    libera_hw_time_t time;
    unsigned long event;
    struct evgen *prev;
    struct evgen *next;
} evgen_t;


typedef struct {
    evgen_t *head;
} evgen_list_t;

struct libera_event_device
{
    LIBERA_COMMON_DEV
    struct mutex CTIME_sem;
    wait_queue_head_t SC_queue;
    wait_queue_head_t MC_queue;
    wait_queue_head_t EVENT_queue;
    struct libera_circbuf paired_timestamps[TRIG_EVENTS_MAX];
    struct libera_fifo sc_timestamps[TRIG_EVENTS_MAX];
    struct libera_fifo mc_timestamps[TRIG_EVENTS_MAX];
    struct libera_event_fifo events;
    unsigned long long sc_time;
    unsigned long long mc_time;
    unsigned long sc_trigVec;
    unsigned long mc_trigVec;
    glitch_CB_t sc_self_inc;
    glitch_CB_t mc_self_inc;
    glitch_CB_t sc_trig10;
    glitch_CB_t mc_trig10;
    libera_settime_t settime;
    libera_hw_time_t HB_start_lmt;
    libera_hw_time_t HB_offset_lmt;
    int ValidTrigVector;
    evgen_t evgen;
    evgen_list_t list;
};

/* Libera ADC device structure */
#define LIBERA_ADC_BUFFER_ATOMS 1024
struct libera_adc_device
{
    LIBERA_COMMON_DEV
    wait_queue_head_t ADC_queue;
    atomic_t trig_lock;
    libera_atom_adc_t buf[LIBERA_ADC_BUFFER_ATOMS];
    libera_operator_t op;
};

/* Libera DSC device structure */
struct libera_dsc_device
{
    LIBERA_COMMON_DEV
    unsigned char buf[DSC_BLOCKSIZE];
};


/* File operations for all the minors */
extern struct file_operations libera_cfg_fops;   /* minor 0 */
extern struct file_operations libera_dd_fops;    /* minor 1 */
extern struct file_operations libera_fa_fops;    /* minor 2 */
extern struct file_operations libera_pm_fops;    /* minor 3 */
extern struct file_operations libera_sa_fops;    /* minor 4 */
extern struct file_operations libera_event_fops; /* minor 5 */
extern struct file_operations libera_adc_fops;   /* minor 6 */
extern struct file_operations libera_dsc_fops;   /* minor 7 */


/* Prototypes for shared functions & globals */
#ifdef __NO_VERSION__  // A trick to include these only where needed
extern unsigned long iobase;
extern unsigned long flmcdHz;
extern unsigned long kadc;
extern unsigned long ilkdur;
extern unsigned long pmdur;
extern unsigned long pmsize;
extern unsigned long atomsize;
extern unsigned long srcspienable;
extern unsigned long sbthreshold;
extern unsigned long sbnbefore;
extern unsigned long sbnafter;

extern struct libera_cfg_device   libera_cfg;
extern struct libera_dd_device    libera_dd;
extern struct libera_fa_device    libera_fa;
extern struct libera_pm_device    libera_pm;
extern struct libera_sa_device    libera_sa;
extern struct libera_event_device libera_event;
extern struct libera_adc_device   libera_adc;
extern struct libera_dsc_device   libera_dsc;
extern struct libera_global lgbl;

extern void
libera_dma_command(
	unsigned long source_addr,
	void* target_addr,
	unsigned long bytes);

extern loff_t
libera_llseek(struct file *file, loff_t offset, int origin);

extern ssize_t
libera_read(struct file *file, char *buf, size_t count, loff_t *f_pos);

extern ssize_t
libera_write(struct file *file, const char *buf, size_t count, loff_t *f_pos);

extern int
libera_ioctl(struct inode *inode, struct file *file,
	     unsigned int cmd, unsigned long arg);

extern int 
libera_send_event(int id, int param);

extern int 
libera_get_CTIME(libera_Ltimestamp_t* const ctime);


/** irqMask get() method. Used for WO mirroring purposes. */
static inline libera_U32_t 
get_irqMask(void)
{
    return lgbl.wom.sbc.irqMask;
}


/** irqMask set() method. Used for WO mirroring purposes. */
static inline void 
set_irqMask(libera_U32_t newMask)
{
    lgbl.wom.sbc.irqMask = newMask;
    writel(newMask, iobase + SBC_IRQ_MASK);
}

#endif /* __NO_VERSION__ */


/* Common public exported inline functions */

/** U32-word block read.
 *
 * Reads a block of U32-words into @param buf from @param address.
 * Since all Libera R/W operations must be 32-bit to ensure proper operation,
 * @param address must be a multiple of 4. The number of U32-words read from
 * @param address is @param u32_size. The number of bytes read from
 * @param address is 4 * @param u32_size.
 */
static inline void
libera_readlBlock(const unsigned long *address, unsigned long *buf, size_t u32_size)
{
    register size_t i;
    
    for (i=0; i < u32_size; i++)
	*(buf+i) = readl(address+i);
}

/** U32-word block write.
 *
 * Writes a block of U32-words from @param buf to @param address.
 * Since all Libera R/W operations must be 32-bit to ensure proper operation,
 * @param address must be a multiple of 4. The number of U32-words written to
 * @param address is @param u32_size. The number of bytes written to
 * @param address is 4 * @param u32_size.
 */
static inline void
libera_writelBlock(const unsigned long *address, unsigned long *buf, size_t u32_size)
{
    register size_t i;
    
    for (i=0; i < u32_size; i++)
	writel(*(buf+i), address + i);
}


/** Efficient (non-busy) delay in 10 miliseconds range */
static inline void
libera_ms_delay(int delay_ms)
{
    int delay_jiff;

    delay_jiff = (delay_ms*HZ)/1000;
    set_current_state(TASK_INTERRUPTIBLE);
    schedule_timeout(delay_jiff);
}


/** Efficient (non-busy) interruptible delay in jiffies */
static inline int
libera_delay_jiffies_interruptible(int delay_jiff)
{
    set_current_state(TASK_INTERRUPTIBLE);
    /*
    if (signal_pending(current)) {
	set_current_state(TASK_RUNNING);
	return -ERESTARTSYS;
    }
    */
    schedule_timeout(delay_jiff);
    
    return 0;
}


/** Efficient (non-busy) uninterruptible delay in jiffies */
static inline int
libera_delay_jiffies(int delay_jiff)
{
    set_current_state(TASK_INTERRUPTIBLE);
    schedule_timeout(delay_jiff);
    
    return 0;
}


/* Needed in various places through Libera GNU/Linux driver */
#define MIN(a,b)  ((a) < (b)) ? (a):(b)
#define XSTR(s) STR(s)
#define STR(s) #s
#define ULL(stamp) (unsigned long)(stamp >> 32), (unsigned long)(stamp & (unsigned long long) 0x00000000FFFFFFFFULL)
#ifndef FALSE
#  define FALSE 0
#endif
#ifndef TRUE
#  define TRUE  1
#endif


/* Macros to help debugging */
//#define LIBERA_SYSLOG_LEVEL   KERN_DEBUG
#define LIBERA_SYSLOG_LEVEL   KERN_CRIT
#if DEBUG >= 1
#  define ASSERT(f)  if ((f)) printk( LIBERA_SYSLOG_LEVEL LIBERA_NAME ": Error (file: %s, line: %d)\n",  __FILE__, __LINE__)
#  define VERIFY(f)  ASSERT(f)
#  define DEBUG_ONLY(f) f
#  undef  PDEBUG
#  define PDEBUG(fmt, args...) printk( LIBERA_SYSLOG_LEVEL LIBERA_NAME ": " fmt,  ## args) 
#else
#  define PDEBUG(fmt, args...) ((void)0)
#  define ASSERT(f)  ((void)0)
#  define VERIFY(f)  ((void)(0))
#  define DEBUG_ONLY(f) ((void)0)
#endif

#if DEBUG >= 2
#  define DEBUG2_ONLY(f) f
#  undef  PDEBUG2
#  define PDEBUG2(fmt, args...) printk( LIBERA_SYSLOG_LEVEL LIBERA_NAME ": " fmt,  ## args) 
#else
#  define DEBUG2_ONLY(f) ((void)0)
#  define PDEBUG2(fmt, args...) ((void)0)
#endif

#if DEBUG >= 3
#  define DEBUG3_ONLY(f) f
#  undef  PDEBUG3
#  define PDEBUG3(fmt, args...) printk( LIBERA_SYSLOG_LEVEL LIBERA_NAME ": " fmt,  ## args) 
#else
#  define DEBUG3_ONLY(f) ((void)0)
#  define PDEBUG3(fmt, args...) ((void)0)
#endif

#define LIBERA_LOG(fmt, args...) printk( LIBERA_SYSLOG_LEVEL LIBERA_NAME ": " fmt,  ## args)


static inline void
logged_writel(unsigned long v, unsigned long c, unsigned long iobase)
{
    printk(LIBERA_SYSLOG_LEVEL "W(0x%lx) = 0x%lx\n", (c - iobase + 0x14000000), v);
    __raw_writel(cpu_to_le32(v),__mem_pci(c));
}

static inline unsigned int
logged_readl(unsigned long c, unsigned long iobase)
{
    unsigned int v = le32_to_cpu(__raw_readl(__mem_pci(c)));
    printk(LIBERA_SYSLOG_LEVEL "R(0x%lx) = 0x%lx\n", 
	   (c - iobase + 0x14000000),
	   (unsigned long)v);
    return v;
}

#if DEBUG >= 4
#  define DEBUG4_ONLY(f) f
#  undef  PDEBUG4
#  define PDEBUG4(fmt, args...) printk( LIBERA_SYSLOG_LEVEL LIBERA_NAME ": " fmt,  ## args)
#  undef  writel
#  define writel(v,c)  logged_writel(v,c,iobase)
#  undef  readl
#  define readl(c) ({ unsigned int __v = logged_readl(c, iobase); __v; })

#else
#  define DEBUG4_ONLY(f) ((void)0)
#  define PDEBUG4(fmt, args...) ((void)0)
#endif


#endif /* __KERNEL__ */
#endif /* _LIBERA_KERNEL_H_ */
