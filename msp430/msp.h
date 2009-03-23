
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/ioport.h>
#include <linux/errno.h>
#include <linux/sched.h>
//#include <asm/io.h>


/** MSP GNU/Linux driver version */
#ifndef RELEASE_VERSION
#define RELEASE_VERSION unknown
#endif

/* IRQ or polling */
//#define MSP_IRQ

/* Default module parameters */
#define MSP_IOBASE           0x10000000
#define MSP_IORANGE          0x00000010

/**********************/
/* Register addresses */
/**********************/
#define CPLD_OFFSET 0x00000000

#define MSP_RESET  0xe + CPLD_OFFSET

#define SSSR_TFLMASK     0xF00
#define MSP_RX_MASK      0x000000FF

/* MSP reset */
#define MRR_PGM      0x04
#define MRR_TCK      0x02
#define MRR_NMI      0x01

/* SSP control register defs */
#define SSCR0_DSS_M(n)   ((n-1) & 0x0000000f)
#define SSCR0_FORMAT_M_SPI  (0x0 << 4)
#define SSCR0_EXTCLK   (1 << 6)
#define SSCR0_SSE      (1 << 7)
#define SSCR0_SCR_M(div) ((div << 8) & 0x0000ff00)

#define SSCR1_RIE      (1 << 0)
#define SSCR1_TIE      (1 << 1)
#define SSCR1_LBM      (1 << 2)
#define SSCR1_SPO      (1 << 3)
#define SSCR1_SPH      (1 << 4)
#define SSCR1_MWDS16   (1 << 5)
#define SSCR1_TFT_M(n)   ((n & 0xf) << 6)
#define SSCR1_RFT_M(n)   ((n & 0xf) << 10)

/* SSP Status register defs */
#define SSSR_TNF       (1 << 2)
#define SSSR_RNE       (1 << 3)
#define SSSR_BSY       (1 << 4)
#define SSSR_TFS       (1 << 5)
#define SSSR_RFS       (1 << 6)
#define SSSR_ROR       (1 << 7)
#define SSSR_TFL(sssr) (((sssr >> 8)  + 1) & 0x0000000f)
#define SSSR_RFL(sssr) (((sssr >> 12) + 1) & 0x0000000f)


/* GPIO alternate function */
#define GAFR_SSP_MASTER 0x005a8000
#define GAFR_SSP_ZERO   0xff003fff


/* MSP protocol defines */
#define MSP_BAUD_DIVISOR            0x0f
#define MSP_PROTO_END               0x10
#define MSP_PROTO_SKIP              0
#define MSP_PROTO_CONV_START        0x10
#define MSP_PROTO_CONV_IN_PROGRESS  0xef
#define MSP_PROTO_CONV_TEST         0xa5


/* MSP ADC properties */
#define MSP_ADC_BIT    12
#define MSP_ADC_RANGE  (1 << MSP_ADC_BIT)  
#define MSP_ADC_VREF   2500  // mV

/* The module name. Used for logging. */
#define MSP_NAME "msp"


/* Major and minor numbers for device files */
/* NOTE: Major numbers in ranges 60-63, 120-127 and 240-254 
   are reserved for experimental use.
   For the inclusion in the official linux kernel one has to
   apply for the assignment of a unique major number. 
*/
#define MSP_MAJOR         241
#define MSP_MAX_MINOR       0

#define MSP_MAX_INTERVAL    1


/** Efficient (non-busy) delay in 10 miliseconds range */
static inline void
msp_ms_delay(int delay_ms)
{
    int delay_jiff;

    delay_jiff = (delay_ms*HZ)/1000;
    set_current_state(TASK_INTERRUPTIBLE);
    schedule_timeout(delay_jiff);
}


/** Efficient (non-busy) interruptible delay in jiffies */
static inline int
msp_delay_jiffies_interruptible(int delay_jiff)
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
msp_delay_jiffies(int delay_jiff)
{
    set_current_state(TASK_UNINTERRUPTIBLE);
    schedule_timeout(delay_jiff);
    
    return 0;
}


/* Needed in various places through MSP GNU/Linux driver */
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
//#define MSP_SYSLOG_LEVEL   KERN_DEBUG
#define MSP_SYSLOG_LEVEL   KERN_CRIT
#if DEBUG >= 1
#  define ASSERT(f)  if ((f)) printk( MSP_SYSLOG_LEVEL MSP_NAME ": Error (file: %s, line: %d)\n",  __FILE__, __LINE__)
#  define VERIFY(f)  ASSERT(f)
#  define DEBUG_ONLY(f) f
#  undef  PDEBUG
#  define PDEBUG(fmt, args...) printk( MSP_SYSLOG_LEVEL MSP_NAME ": " fmt,  ## args) 
#else
#  define PDEBUG(fmt, args...) ((void)0)
#  define ASSERT(f)  ((void)0)
#  define VERIFY(f)  ((void)(0))
#  define DEBUG_ONLY(f) ((void)0)
#endif

#if DEBUG >= 2
#  define DEBUG2_ONLY(f) f
#  undef  PDEBUG2
#  define PDEBUG2(fmt, args...) printk( MSP_SYSLOG_LEVEL MSP_NAME ": " fmt,  ## args) 
#else
#  define DEBUG2_ONLY(f) ((void)0)
#  define PDEBUG2(fmt, args...) ((void)0)
#endif

#if DEBUG >= 3
#  define DEBUG3_ONLY(f) f
#  undef  PDEBUG3
#  define PDEBUG3(fmt, args...) printk( MSP_SYSLOG_LEVEL MSP_NAME ": " fmt,  ## args) 
#else
#  define DEBUG3_ONLY(f) ((void)0)
#  define PDEBUG3(fmt, args...) ((void)0)
#endif

#define MSP_LOG(fmt, args...) printk( MSP_SYSLOG_LEVEL MSP_NAME ": " fmt,  ## args)


static inline void
logged_writel(unsigned long v, unsigned long c, unsigned long iobase)
{
    printk(MSP_SYSLOG_LEVEL "W(0x%lx) = 0x%lx\n", (c - iobase + 0x14000000), v);
    __raw_writel(cpu_to_le32(v),__mem_pci(c));
}

static inline unsigned int
logged_readl(unsigned long c, unsigned long iobase)
{
    unsigned int v = le32_to_cpu(__raw_readl(__mem_pci(c)));
    printk(MSP_SYSLOG_LEVEL "R(0x%lx) = 0x%lx\n", 
	   (c - iobase + 0x14000000),
	   (unsigned long)v);
    return v;
}

#if DEBUG >= 4
#  define DEBUG4_ONLY(f) f
#  undef  PDEBUG4
#  define PDEBUG4(fmt, args...) printk( MSP_SYSLOG_LEVEL MSP_NAME ": " fmt,  ## args)
#  undef  writel
#  define writel(v,c)  logged_writel(v,c,iobase)
#  undef  readl
#  define readl(c) ({ unsigned int __v = logged_readl(c, iobase); __v; })

#else
#  define DEBUG4_ONLY(f) ((void)0)
#  define PDEBUG4(fmt, args...) ((void)0)
#endif


#endif /* __KERNEL__ */


/*************************/
/* Userspace definitions */
/*************************/



