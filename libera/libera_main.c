/* $Id: libera_main.c 2318 2008-11-27 09:52:56Z tomazb $ */

//! \file libera_main.c
//! Implements Libera GNU/Linux driver

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

#ifndef __KERNEL__
#  define __KERNEL__
#endif

#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/vmalloc.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26)
#include <mach/system.h>
#else
#include <linux/slab.h>
#include <linux/string.h>
#include <asm/arch-pxa/system.h>
#include <asm/irq.h>
#include <asm/dma.h>
#include <asm/io.h>
#include <asm-arm/irq.h>
#endif

#include "libera_kernel.h"
#include "libera_event.h"
#include "libera.h"

extern int
getBlockedFromFIFO(struct libera_fifo* const q,
		   wait_queue_head_t* const wq,
		   libera_hw_time_t *time);
extern int
libera_sctrig_enable(struct libera_event_device * dev,
		     const unsigned long mask);
extern int
libera_mctrig_enable(struct libera_event_device * dev,
		     const unsigned long mask);
extern int
libera_init_specific(void);

extern void
libera_SCtrig_specific(struct libera_event_device *event,
		       const libera_hw_time_t *stamp,
		       const libera_hw_time_t *stamp_raw,
		       unsigned long m, unsigned int i);
extern void
libera_MCtrig_specific(struct libera_event_device *event,
		       const libera_hw_time_t *stamp,
		       const libera_hw_time_t *stamp_raw,
		       unsigned long m, unsigned int i);

extern int
libera_valid_trigger(struct libera_event_device *event,
                     int trigno);

extern void
libera_sc_selfinc_specific(struct libera_event_device *event);

extern void
libera_mc_selfinc_specific(struct libera_event_device *event);

extern void
libera_schelp_handler_specific(struct libera_event_device  *event);

extern void
libera_mchelp_handler_specific(struct libera_event_device  *event);

extern void
libera_sa_handler_specific(void);

extern void
libera_sa_do_tasklet_specific(unsigned long data);

extern void
libera_ilk_handler_specific(unsigned long status);


extern libera_desc_t libera_desc;

/* Libera devices private status */
struct libera_cfg_device   libera_cfg;
struct libera_dd_device    libera_dd;
struct libera_fa_device    libera_fa;
struct libera_pm_device    libera_pm;
struct libera_sa_device    libera_sa;
struct libera_event_device libera_event;
struct libera_adc_device   libera_adc;
struct libera_dsc_device   libera_dsc;

void *libera_dev[] = {
    (void *)&libera_cfg,
    (void *)&libera_dd,
    (void *)&libera_fa,
    (void *)&libera_pm,
    (void *)&libera_sa,
    (void *)&libera_event,
    (void *)&libera_adc,
    (void *)&libera_dsc
};

unsigned long dummy_return_value;

/** File operations array.
 *
 * Different minors behave differently, so we use multiple fops structures.
 */
static struct file_operations *libera_fops_array[] ={
    &libera_cfg_fops,   /* minor 0 */
    &libera_dd_fops,    /* minor 1 */
    &libera_fa_fops,    /* minor 2 */
    &libera_pm_fops,    /* minor 3 */
    &libera_sa_fops,    /* minor 4 */
    &libera_event_fops, /* minor 5 */
    &libera_adc_fops,    /* minor 6 */
    &libera_dsc_fops    /* minor 7 */
};

/* Libera global parameters */
struct libera_global lgbl;

/* I/O memory base address and range - default values */
unsigned long iobase  = LIBERA_IOBASE;
const  unsigned long iorange = LIBERA_IORANGE;

/* GPIO number (IRQ-1) */
static int gpio = LIBERA_IRQ_GPIO_NR;

/* Initial MC frequency f_MC */
unsigned long flmcdHz = LIBERA_FLMC_INITIAL;

/* Default Libera ADC interlock scaling factor */
unsigned long kadc = LIBERA_KADC_INITIAL;

/* Default Libera ILK overflow duration limit */
unsigned long ilkdur = LIBERA_DUR_INITIAL;

/* Default Libera ILK overflow duration limit */
unsigned long pmdur = LIBERA_DUR_INITIAL;

/* Default PM buffer size */
unsigned long pmsize = LIBERA_PMSIZE_INITIAL;

/* Default logical atom size */
unsigned long atomsize = 0;

/* Module properties and parameters */
#if defined(MODULE)
MODULE_AUTHOR("Ales Bardorfer, Instrumentation Technologies");
MODULE_DESCRIPTION("Instrumentation Technologies Libera driver");
MODULE_LICENSE("GPL");
MODULE_SUPPORTED_DEVICE("libera");

module_param(iobase, long, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC (iobase, "I/O memory region base address (default 0x1400000)");
module_param(gpio, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC (gpio, "GPIO number for IRQ hook");
module_param(flmcdHz, long, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC (flmcdHz, "Initial LMC frequency f_LMC in dHz");
module_param(kadc, long, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC (kadc, "ADC interlock scaling factor");
module_param(ilkdur, long, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC (ilkdur, "ILK overflow duration limit");
module_param(pmdur, long, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC (pmdur, "PM overflow duration limit");
module_param(pmsize, long, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC (pmsize, "Post-Mortem buffer size");
module_param(atomsize, ulong, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC (atomsize, "Logical atom size (in samples)");
#endif


/* Set up interrupt handler tasklets */
void libera_sa_do_tasklet(unsigned long);
DECLARE_TASKLET (libera_sa_tasklet, libera_sa_do_tasklet, 0);

/* Interrupt handler protection spinlock */
spinlock_t libera_irq_spinlock = SPIN_LOCK_UNLOCKED;


#include "libera_dd.h"


/** Send an asyncronous event via event-fifo.
 *
 * Sends an asyncronous event to the event fifo if enabled.
 * \param msg_id Message ID, specifiying the cause of the asynchronous event.
 * \param msg_param Message specific parameter.
 */
int
libera_send_event(int id, int param)
{
    struct libera_event_device  *event  = &libera_event;
    libera_event_t le;

    /* Event filtering - Only send the event if not masked */
    if ( ! (id & lgbl.event_mask) ) {
        PDEBUG3("Event blocked: id=0x%x param=0x%x mask=0x%x\n",
                id, param, lgbl.event_mask);
	return -EINVAL;
    }

    le.id = id;
    le.param = param;

    PDEBUG3("Sending event: id=0x%x param=0x%x mask=0x%x\n",
            id, param, lgbl.event_mask);
    if (putTo_eventFIFO(& event->events, &le) < 0 )
	printk(KERN_CRIT "libera: EVENT FIFO overflow.\n");

    wake_up_interruptible(&event->EVENT_queue);

    return 0;
}


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


/** Get current libera LMT & LST, return error on failure
 *
 * Returns current LMT & LST pair via \param ctime.
 * On success, 0 is returned.
 * On error, meaningful negative errno is returned.
 */
int
libera_get_CTIME(libera_Ltimestamp_t* const ctime)
{
    struct libera_event_device  *event  = &libera_event;
    libera_hw_time_t *data;
    int ret = 0;


    /* Mutual exclusion on get LMT & LST
     * mutex_lock_interruptible returns error if the process receives a signal.
     * NOTE: Mutexes in kernel are semaphores anyway, and we cannot use
     *       spinlocks, because we go to sleep later on.
     */
    if (mutex_lock_interruptible(&event->CTIME_sem))
	return -ERESTARTSYS;

    PDEBUG3("CTIME: Before SoftTrig iVec=0x%x, iMask=0x%lx\n",
	    readl(iobase + SBC_IRQ), (unsigned long)get_irqMask());
    /* Software trigger, SC module */
    writel(TRIGGER_BIT(T_TRIG_CTIME), iobase + T_SC_EVENTGEN);

    PDEBUG3("CTIME: After  SoftTrig iVec=0x%x, iMask=0x%lx\n",
	    readl(iobase + SBC_IRQ), (unsigned long)get_irqMask());

    /* Wait for CTIME MC interrupt.
     * NOTE: MC-fifo will always fill after SC-fifo. If we reverse
     *       the reading order (MC then SC), we can avoid the
     *       blocking read on SC, because we can be sure SC is
     *       already there.
     */
    PDEBUG3("Waiting for CTIME trigger.\n");
    ret = getBlockedFromFIFO(&event->mc_timestamps[T_TRIG_CTIME],
			     &event->MC_queue,
			     &ctime->lmt);
    if (ret) goto out;
    /* OK, we were awoken by the interrupt handler (after MC).
     * SC should be in SC-fifo, so read it (non-blocking).
     */
    if (! (data = getFromFIFO(&event->sc_timestamps[T_TRIG_CTIME])) ) {
	ret = -EFAULT;
	goto out;
    }
    ctime->lst = *data;

 out:
    mutex_unlock(&event->CTIME_sem);
    return ret;
}



/** DEBUG helper function */
static inline void
putToGlitchCB(glitch_CB_t* const q,
	      const libera_hw_time_t *stamp,
	      unsigned long  th,
	      unsigned long  tl,
	      unsigned long jiff_time)
{
    q->gt[q->put].stamp = *stamp;
    q->gt[q->put].th = th;
    q->gt[q->put].tl = tl;
    q->gt[q->put].jiff_time = jiff_time;
    q->put = ((q->put + 1) & GLITCH_LOG_MASK);
}

/** DEBUG helper function */
static inline glitch_times_t*
getFromGlitchCB(glitch_CB_t* const q, int history)
{
    register int index;
    index = ((q->put - 1 - history) & GLITCH_LOG_MASK);
    return & q->gt[index];
}
#define GET_MASK  0xFF800000UL
#define PUT_MASK  0x007FC000UL
#define GET_SHIFT 23
#define PUT_SHIFT 14

#define GLITCH_LOG_MAX 5

/** Glitch Timing logging based on jiffies difference */
static inline void
gt_log_jiff(char* const name,
	    glitch_CB_t* const q,
	    const libera_hw_time_t *stamp,
	    unsigned long  th,
	    unsigned long  tl,
	    unsigned long jiff_time,
	    unsigned int jiff_low,
	    unsigned int jiff_high)
{
    glitch_times_t *gt[GLITCH_LOG_MAX];
    long gt_diff;
    int i;

    putToGlitchCB(q, stamp, th, tl, jiff_time);
    gt[0] = getFromGlitchCB(q, 0);
    gt[1] = getFromGlitchCB(q, 1);
    gt_diff = gt[0]->jiff_time - gt[1]->jiff_time;
    if ((gt_diff < jiff_low) ||
	(gt_diff > jiff_high))
    {
	for (i=2; i< GLITCH_LOG_MAX; i++)
	    gt[i] = getFromGlitchCB(q, i);

	PDEBUG2("GLITCH: %s:\n", name);
	for (i=0; i< GLITCH_LOG_MAX; i++) {
	    PDEBUG2("(N-%u): 0x%08lx 0x%08lx  0x%08lx%08lx  0x%08lx%08lx  %lu  %lu\n",
		   i,
		   ((gt[i]->th & GET_MASK) >> GET_SHIFT),
		   ((gt[i]->th & PUT_MASK) >> PUT_SHIFT),
		   gt[i]->th,
		   gt[i]->tl,
		   ULL(gt[i]->stamp),
		   gt[i]->jiff_time,
		   T_FIFO_SIZE(gt[i]->tl));
	}
    }
}

/** Glitch Timing logging based on timestamp difference */
static inline void
gt_log_stamp(char* const name,
	     glitch_CB_t* const q,
	     const libera_hw_time_t *stamp,
	     unsigned long  th,
	     unsigned long  tl,
	     unsigned long jiff_time,
	     long long stamp_low,
	     long long stamp_high)
{
    glitch_times_t *gt[GLITCH_LOG_MAX];
    long long lgt_diff;
    int i;

    putToGlitchCB(q, stamp, th, tl, jiff_time);
    gt[0] = getFromGlitchCB(q, 0);
    gt[1] = getFromGlitchCB(q, 1);
    lgt_diff = gt[0]->stamp - gt[1]->stamp;
    if ((lgt_diff < stamp_low) ||
	(lgt_diff > stamp_high))
    {
	for (i=2; i< GLITCH_LOG_MAX; i++)
	    gt[i] = getFromGlitchCB(q, i);

	PDEBUG2("GLITCH: %s:\n", name);
	for (i=0; i< GLITCH_LOG_MAX; i++) {
	    PDEBUG2("(N-%u): 0x%08lx 0x%08lx  0x%08lx%08lx  0x%08lx%08lx  %lu  %lu\n",
		   i,
		   ((gt[i]->th & GET_MASK) >> GET_SHIFT),
		   ((gt[i]->th & PUT_MASK) >> PUT_SHIFT),
		   gt[i]->th,
		   gt[i]->tl,
		   ULL(gt[i]->stamp),
		   gt[i]->jiff_time,
		   T_FIFO_SIZE(gt[i]->tl));
	}
    }
}


/** Settime LST */
static inline void
libera_set_LST(struct libera_event_device *event)
{
    libera_Ltimestamp_t *trig =
	getFromCircBuf(&event->paired_timestamps[T_TRIG_TRIGGER]);

    PDEBUG2("libera_set_LST:\n");
    PDEBUG2("  LST_ref           = 0x%08lx%08lx\n",
	    ULL(event->settime.lst.ref));
    PDEBUG2("  LST_trig_raw      = 0x%08lx%08lx\n",
	    ULL(trig->lst));

    /* Global LST offset */
    event->settime.lst.off = event->settime.lst.ref -
        event->settime.lst.scphi - trig->lst;

    /* DEBUG report */
    PDEBUG2("  SCPHI Offset      = 0x%08lx%08lx\n",
	    ULL(event->settime.lst.scphi));
    PDEBUG2("  Global LST Offset = 0x%08lx%08lx\n",
	    ULL(event->settime.lst.off));
}


/** Settime LMT */
static inline void
libera_set_LMT(struct libera_event_device *event)
{
    libera_Ltimestamp_t *trig =
	getFromCircBuf(&event->paired_timestamps[T_TRIG_TRIGGER]);
    
    libera_hw_time_t off_pll_old = event->settime.lmt.off_pll;

    /* Disarm FA/FAI reset - not required anymore - FPGA self cleared */

    PDEBUG2("libera_set_LMT:\n");
    PDEBUG2("  LMT_ref           = 0x%08lx%08lx\n",
	    ULL(event->settime.lmt.ref));
    PDEBUG2("  LMT_trig_raw      = 0x%08lx%08lx\n",
	    ULL(trig->lmt));

    /* Global LMT offset */
    event->settime.lmt.off_all = event->settime.lmt.ref -
        event->settime.lmt.mcphi - trig->lmt +
        off_pll_old;

    /* Partial LMT offsets */
    if (lgbl.dfa)
    {
      uint32_t rem;
      (void)div_u64_rem(event->settime.lmt.off_all, lgbl.d*lgbl.dfa, &rem);
      event->settime.lmt.off_pll = rem;
    }
    else
      event->settime.lmt.off_pll = 0;
    
    event->settime.lmt.off =
	event->settime.lmt.off_all -
	event->settime.lmt.off_pll;

    /* DEBUG report */
    PDEBUG2("  MCPHI Offset      = 0x%08lx%08lx\n",
	    ULL(event->settime.lmt.mcphi));
    PDEBUG2("  Global LMT Offset = 0x%08lx%08lx\n",
	    ULL(event->settime.lmt.off_all));
    PDEBUG2("  Driver LMT Offset = 0x%08lx%08lx\n",
	    ULL(event->settime.lmt.off));
    PDEBUG2("  PLL LMT Offset    = 0x%08lx%08lx\n",
	    ULL(event->settime.lmt.off_pll));

    /* SA sync - stop & schedule appropriate start */
    writel(SA_STOP, iobase + SA_CONTROL);
    event->list.head->event = 1 << T_EG_SA;
    event->list.head->time = trig->lmt +
        20*1024*lgbl.dfa*lgbl.d - // 2 seconds delay to ensure SA start event
        (event->settime.lmt.off_pll - off_pll_old);
}


/** Sets libera LMT and/or LST time to a desired value */
static inline void
libera_set_time(struct libera_event_device *event)
{
    if (event->settime.update & LIBERA_SETTIME_ST)
	libera_set_LST(event);
    if (event->settime.update & LIBERA_SETTIME_MT)
	libera_set_LMT(event);

    event->settime.update = 0;
}


/** Libera SC interrupt handler.
 *
 */
static inline void
libera_sc_handler(void)
{
  struct libera_event_device  *event  = &libera_event;
  struct libera_cfg_device    *cfg    = &libera_cfg;
  unsigned long long stamp;
  unsigned long long stamp_raw;
  register void *p;
  unsigned long tl, th, tex;
  register unsigned long m = TRIG_ZERO;
  register unsigned int i = 0;
  DEBUG2_ONLY(int count = 0);

  p = &stamp;
  tl = readl(iobase + T_SC_STATUSL);
  while( T_FIFO_SIZE(tl) )
    {
      th = readl(iobase + T_SC_STATUSH);
      tex = readl(iobase + T_SC_FIFOADV);

      ((unsigned long *) p)[0] = (th & 0x03FFFFFFUL);
      ((unsigned long *) p)[1] = 0;

      if ( tl & SELF_INC_TRIGGER )
      {
	  event->sc_time += 0x4000000ULL;
	  PDEBUG3("SC: SELF_INC: 0x%08lx%08lx, stamp26= 0x%08lx%08lx\n",
		  ULL(event->sc_time), ULL(stamp));
	  DEBUG3_ONLY(gt_log_jiff("SC_SELF_INC",
				 &event->sc_self_inc,
				 &event->sc_time,
				 th, tl,
				 jiffies,
				 GT_SC_JIFF_LOW,
				 GT_SC_JIFF_HIGH);
		     );

          libera_sc_selfinc_specific(event);
      }

      /* Construct the whole 38+26 = 64 bit timestamp */
      stamp |= (event->sc_time & 0xFFFFFFFFFC000000ULL);

      /* Settime offset */
      stamp_raw = stamp;
      stamp += event->settime.lst.off;

      /* Fill all FIFOs from FPGA timing module SC */
      while ( tl & TRIG_VECTOR )
      {
	while (!(tl & m) )
	{
	  i++; m <<= 1;
	}
	tl &= ~m; //clear current vector bit
	PDEBUG3("SC_trig%u: 0x%08lx%08lx  %lu  %lu\n",
		i,
		ULL(stamp),
		jiffies,
		T_FIFO_SIZE(tl));

	DEBUG3_ONLY(if (i == 10) {
	               gt_log_stamp("SC_TRIG10",
		       &event->sc_trig10,
		       &stamp_raw,
		       th, tl,
		       jiffies,
		       GT_SC_TRIG10_LOW,
		       GT_SC_TRIG10_HIGH);
	});

	switch(i)
	{
	case T_TRIG_TRIGGER:
	    if (cfg->param[LIBERA_CFG_TRIGMODE] == LIBERA_TRIGMODE_SET) {
	        /* Read the first half of atom
		 * Use stamp_raw for SET trigger!
		 */
	      putLSTtoCircBuf(&event->paired_timestamps[i], &stamp_raw);
	      PDEBUG2("SET TRIGGER: LST_raw = 0x%08lx%08lx, j = %lu\n",
		      ULL(stamp_raw), jiffies);
	    } else {
	        /* Read the first half of atom
		 * Use stamp with offset for GET trigger!
		 */
	        putLSTtoCircBuf(&event->paired_timestamps[i], &stamp);
		PDEBUG2("GET TRIGGER: LST = 0x%08lx%08lx, j = %lu\n",
			ULL(stamp), jiffies);
	    }
	    break;

	    /* Handle specific triggers */
	default:
	    libera_SCtrig_specific(event, &stamp, &stamp_raw, m, i);
	} // switch
      } // while ( th & TRIG_VECTOR )

      /* Deadlock avoidance */
      DEBUG2_ONLY(
          if (count++ > 512){
	    PDEBUG2("SC: DEADLOCK AVOIDED (file: %s, line: %d) ",
		    __FILE__, __LINE__);
	    PDEBUG2("T_FIFO_SIZE(tl) = %lu\n", T_FIFO_SIZE(tl));
	    return;
	  });

      /* Reinit */
      i = 0; m = TRIG_ZERO;
      /* NOTE: FPGA "address cacheing" avoidance.
       *       A dummy FPGA register read is needed between two
       *       T_SC_STATUS reads, regardless of the timing between those
       *       two reads.
       *       IRQ status register (SBC_IRQ) should be used
       *       for this purpose.
       */
      readl(iobase + SBC_IRQ);
      tl = readl(iobase + T_SC_STATUSL);
      readl(iobase + SBC_IRQ);
    }  // while( T_FIFO_SIZE(tl) )
  wake_up_interruptible(&event->SC_queue);
}


/** Libera MC interrupt handler.
 *
 */
static inline void
libera_mc_handler(void)
{
  struct libera_event_device  *event  = &libera_event;
  struct libera_cfg_device    *cfg    = &libera_cfg;
  int trigmode;
  unsigned long long stamp_raw;
  unsigned long long stamp;
  register void *p;
  unsigned long tl, th, tex;
  register unsigned long m = TRIG_ZERO;
  register unsigned int i = 0;
  DEBUG2_ONLY(int count = 0);

  p = &stamp;
  tl = readl(iobase + T_MC_STATUSL);
  while ( T_FIFO_SIZE(tl) )
    {
      th = readl(iobase + T_MC_STATUSH);
      tex = readl(iobase + T_MC_FIFOADV);

      ((unsigned long *) p)[0] = (th & 0x3FFFFFFUL);
      ((unsigned long *) p)[1] = 0;
      if ( tl & SELF_INC_TRIGGER )
      {
	  DEBUG_ONLY(static unsigned long count; count++;);

	  event->mc_time += 0x4000000ULL;
	  PDEBUG3("MC: SELF_INC: 0x%08lx%08lx, stamp26= 0x%08lx%08lx\n",
		  ULL(event->mc_time), ULL(stamp));
	  DEBUG3_ONLY(gt_log_jiff("MC_SELF_INC",
				 &event->mc_self_inc,
				 &event->mc_time,
				 th, tl,
				 jiffies,
				 GT_MC_JIFF_LOW,
				 GT_MC_JIFF_HIGH);
		     );

          libera_mc_selfinc_specific(event);
      }

      /* Construct the whole 38+26 = 64 bit timestamp */
      stamp |= (event->mc_time & 0xFFFFFFFFFC000000ULL);

      /* Settime offsets */
      stamp_raw = stamp;
      stamp += event->settime.lmt.off;

      /* Fill all FIFOs from FPGA timing module MC */
      while ( tl & TRIG_VECTOR )
      {
	while (!(tl & m) )
	{
	  i++; m <<= 1;
	}
	tl &= ~m; //clear current vector bit
	PDEBUG3("MC_trig%u: 0x%08lx%08lx  %lu  %lu\n",
		i,
		ULL(stamp),
		jiffies,
		T_FIFO_SIZE(tl));

	DEBUG3_ONLY(if (i == 10) {
	  gt_log_stamp("MC_TRIG10",
		       &event->mc_trig10,
		       &stamp_raw,
		       th, tl,
		       jiffies,
		       GT_MC_TRIG10_LOW,
		       GT_MC_TRIG10_HIGH);
	});

	switch(i)
	{
	case T_TRIG_TRIGGER:
	    trigmode = cfg->param[LIBERA_CFG_TRIGMODE];
	    if (trigmode == LIBERA_TRIGMODE_SET) {
	        /* Read the second half of atom & increment CircBuf pointer
		 * Use stamp_raw for SET trigger!
		 */
	        putLMTtoCircBuf(&event->paired_timestamps[i], &stamp_raw);
		PDEBUG2("SET TRIGGER: LMT_raw = 0x%08lx%08lx, j = %lu\n",
			ULL(stamp_raw), jiffies);

		/* SET time */
		libera_set_time(event);
		/* NOTE: If there were more than 2 trigmodes, we would have
		 *       to set it to the previous/last mode.
		 *       Since there are only two of them, we know it's
		 *       a GET after the SET.
		 */
		cfg->param[LIBERA_CFG_TRIGMODE] = LIBERA_TRIGMODE_GET;
	    } else {
	        /* Read the second half of atom & increment CircBuf pointer
		 * Use stamp with offset for GET trigger!
		 */
                if (!libera_valid_trigger(event,i)) break;
	        putLMTtoCircBuf(&event->paired_timestamps[i], &stamp);
		PDEBUG2("GET TRIGGER: LMT = 0x%08lx%08lx, j = %lu\n",
                        ULL(stamp), jiffies);
	    }
	    /* Notify userland about TRIGGER trigger event  */
	    if ( trigmode == LIBERA_TRIGMODE_GET )
		libera_send_event(LIBERA_EVENT_TRIGGET, 0);
	    else if ( trigmode == LIBERA_TRIGMODE_SET )
		libera_send_event(LIBERA_EVENT_TRIGSET, 0);
	    break;

	default:
	    /* All the other triggers */
	    libera_MCtrig_specific(event, &stamp, &stamp_raw, m, i);
	} // switch(i)
      } // while ( th & TRIG_VECTOR )

      /* Deadlock avoidance */
      DEBUG2_ONLY(
          if (count++ > 512){
	      PDEBUG2("MC: DEADLOCK AVOIDED (file: %s, line: %d) ",
		      __FILE__, __LINE__);
	      PDEBUG2("T_FIFO_SIZE(tl) = %lu\n", T_FIFO_SIZE(tl));
	      return;
	  });

      /* Reinit */
      i = 0; m = TRIG_ZERO;
      /* NOTE: FPGA "address cacheing" avoidance.
       *       A dummy FPGA register read is needed between two
       *       T_MC_STATUS reads, regardless of the timing between those
       *       two reads.
       *       IRQ status register (SBC_IRQ) should be used
       *       for this purpose.
       */
      readl(iobase + SBC_IRQ);
      tl = readl(iobase + T_MC_STATUSL);
      readl(iobase + SBC_IRQ);
    }  // while ( T_FIFO_SIZE(tl) )
  wake_up_interruptible(&event->MC_queue);
}


/** Libera SC helper interrupt handler.
 *
 */
static inline void
libera_schelp_handler(void)
{
    struct libera_event_device  *event  = &libera_event;

    PDEBUG3("SC helper, event->sc_time = 0x%08lx%08lx, j = %lu\n",
	    ULL(event->sc_time), jiffies);

    /* Common part */

    /* Specific part */
    libera_schelp_handler_specific(event);
}

/** Libera MC helper interrupt handler.
 *
 */
static inline void
libera_mchelp_handler(void)
{
    struct libera_event_device  *event  = &libera_event;

    PDEBUG3("MC helper, event->mc_time = 0x%08lx%08lx, j = %lu\n",
	    ULL(event->mc_time), jiffies);

    /* Common part */

    /* Specific part */
    libera_mchelp_handler_specific(event);
}


/** Libera SC/MC helper interrupt handler.
 *
 */
static inline void
libera_scmchelp_handler(void)
{
    unsigned long int_status;

    /* Acknowledge SC/MC helper */
    int_status = readl(iobase + SBC_IRQ2);
    PDEBUG3("Heper IRQ: int_status = 0x%08lx\n", int_status);

    if (int_status & (1<<0)) {
	libera_schelp_handler();
    }

    if (int_status & (1<<8)) {
	libera_mchelp_handler();
    }

}


/** Libera SA interrupt handler Top Half (TH).
 *
 * Takes care of reading the data from SA fifo on FPGA to internal buffer.
 * The rest is done in the tasklet Bottom Half (BH).
 */
static inline void
libera_sa_handler(void)
{
    /* Common part. */

    /* Specific part. */
    libera_sa_handler_specific();

    /* Leave the rest for the tasklet */
    tasklet_schedule(&libera_sa_tasklet);
}


/** Libera SA interrupt handler tasklet  Bottom Half (BH).
 *
 * Takes care of writing the data to SA pipes (software buffers).
 * The responsibility of this function is to signal FPGA fifo overflows &
 * software pipe overflows as well.
 *
 * This function (BH) can be interrupted, by its corresponding TH.
 * Data corruption prevention is therefore essential and solved by using
 * a circular buffer (dev->buf) betwen top and bottom half.
 * This buffer has got nothing to do with the SA fifos between bottom half and
 * reader(s) in userland.
 */
void libera_sa_do_tasklet(unsigned long data)
{
    /* Common part. */

    /* Specific part. */
    libera_sa_do_tasklet_specific(data);
}


/** Libera DD interrupt handler.
 *
 * Libera DD interrupt handler is only used for signaling that DD OB-fifo
 * is not empty. To keep the global interrupt handler simple, we only
 * acknowledge the DD interrupt by disabling it. The OB-fifo is emptied
 * and DD interrupt re-enabled afterwards in DD section of the driver.
 */
static inline void
libera_dd_handler(void)
{
    struct libera_dd_device  *dd  = &libera_dd;

    PDEBUG3("DD irq: Entering DD handler.\n");

    /* Acknowledge DD interrupt by disabling it.
     * NOTE: By only disabling the DD interrupt, we gain a lot of
     *       the DD handler (it finishes very quicly), but the Libera
     *       driver takes all the responsibilities to empty the OB-fifo
     *       later in DD section. That section is responsible to
     *       re-enable the DD interrupt.
     *       By all means, reading the OB-fifo should be avoided in
     *       the interrupt handler.
     */
    PDEBUG3("DD irq: Disabling DD interrupt.\n");
    set_irqMask((get_irqMask() & ~LIBERA_INTERRUPT_DD_MASK));

    /* Wake up the OB-fifo reading section */
    if (putToFIFO(&dd->dd_irqevents, 0) < 0)
	printk(KERN_CRIT "libera: DD irq: FIFO overflow.\n");
    PDEBUG3("DD irq: Waking up OB-fifo section...\n");
    wake_up_interruptible(&dd->DD_queue);
}

/** Libera ILK interrupt handler.
 *
 * Libera ILK interrupt handler is used for Interlock event notifications.
 * The staus is kept in cfg until reset.
 */
static inline void
libera_ilk_handler(void)
{
    unsigned long status = readl(iobase + ILK_STATUS);

    PDEBUG3("ILK irq: Entering ILK handler.\n");

    libera_ilk_handler_specific(status);
    /* Reading the Interlock status will reset the IRQ. */
    libera_send_event(LIBERA_EVENT_INTERLOCK, status);
}


/** Libera interrupt handler
 *
 * The same IRQ can and will be triggered by various sources:
 *  - SA fifo not empty
 *  - DD fifo not empty
 *  - MC & SC Timing
 *
 * The specific reason(s) why an interrupt was triggered are obtained from
 * the Libera IRQ registers: Interrupt Vector and Interrupt Mask Vector.
 * After handling the interrupt, this handler will not reset the
 * corresponding bit in the interrupt vector. It is the responsibility of
 * the FPGA to reset the interrupt vector bit upon a specific action
 * (i.e. after reading a sample from FIFO).
 *
 * \param irq The IRQ number.
 * \param dev_id Client data (unused).
 * \param regs Processor registers before the interrupt.
 *
 * For details see: Alessandro Rubini et al., Linux Device Drivers and Linux source code.
 */
static irqreturn_t
libera_interrupt(int irq, void *dev_id)
{
    unsigned int_vec;
    unsigned long flags;
    DEBUG2_ONLY(int count = 0);


    PDEBUG3("ENTERING INTERRUPT HANDLER!\n");

    /* Check if it's our IRQ, otherwise return immediately! */
    if (irq != lgbl.irq) return IRQ_NONE;

    spin_lock_irqsave(&libera_irq_spinlock, flags);
    /* Who is bothering us?
     * NOTE: It is assumed, that an empty int_vec (int_vec = 0) implicitly
     *       defines that the hardware IRQ signal GPIO(n) is HIGH (inactive).
     *       This assumption was confirmed by Borut Solar on Sept 14, 2004.
     */
    while( (int_vec = (readl(iobase + SBC_IRQ) & get_irqMask())) )
    {
	/* NOTE: It is vital for the atomicity of CTIME LMT & LST
	 *       capturing, that SC handler is called before MC handler.
	 */

	PDEBUG3("In IntWhile loop, (iVec & iMask)=0x%x\n", int_vec);
	/* 0th priority interrupt */
	if (int_vec & LIBERA_INTERRUPT_ILK_MASK)
	{
	    PDEBUG3("Calling ILK int. handler.\n");
	    libera_ilk_handler();
	}

	/* 1st priority interrupt */
	if (int_vec & LIBERA_INTERRUPT_SC_MASK)
	{
	    PDEBUG3("Calling SC int. handler.\n");
	    libera_sc_handler();
	}

	/* 2nd priority interrupt */
	if (int_vec & LIBERA_INTERRUPT_MC_MASK)
	{
	    PDEBUG3("Calling MC int. handler.\n");
	    libera_mc_handler();
	}

	/* 3rd & 4th priority interrupt */
	if (int_vec & (LIBERA_INTERRUPT_HELPSC_MASK |
		       LIBERA_INTERRUPT_HELPMC_MASK))
	{
	    PDEBUG3("Calling SCMC helper handler.\n");
	    libera_scmchelp_handler();
	}

	/* 5th priority interrupt */
	if (int_vec & LIBERA_INTERRUPT_DD_MASK)
	{
	    PDEBUG3("Calling DD int. handler.\n");
	    libera_dd_handler();
	}

	/* 6th priority interrupt */
	if (int_vec & LIBERA_INTERRUPT_SA_MASK)
	{
	    PDEBUG3("Calling SA int. handler.\n");
	    libera_sa_handler();
	}

	/* Deadlock avoidance */
	DEBUG2_ONLY(
	    if (count++ > 256){
		PDEBUG2("INTERRUPT DEADLOCK AVOIDED (file: %s, line: %d)\n ",
			__FILE__, __LINE__);
		PDEBUG2("Further interrupts disabled due to unhandled int. iVec=0x%x int_vec=0x%x\n", readl(iobase + SBC_IRQ), int_vec);
		return IRQ_NONE;
	    });
    } /* while(int_vec) */

    PDEBUG3("LEAVING INT. HANDLER, (iVec & iMask)=0x%x\n",
	   readl(iobase + SBC_IRQ) & get_irqMask());
    spin_unlock_irqrestore(&libera_irq_spinlock, flags);

    return IRQ_HANDLED;
}

/** Start a new DMA transfer in DMAC */
void
libera_dma_command(unsigned long source_addr,
                   void* target_addr,
                   unsigned long bytes)
{
    libera_dma_t *dma = &lgbl.dma;
    dma->DMAC_transfer = TRUE;

    PDEBUG3("Starting DMA transfer from 0x%lx to 0x%lx (bus:0x%lx), size = %lu bytes (%lu atoms)\n",
            source_addr,
            target_addr,
            virt_to_bus(target_addr),
            bytes,
	    (bytes/sizeof(libera_atom_dd_t)));
    DCSR(dma->chan) |= DCSR_NODESC;
    DCSR(dma->chan) &= ~DCSR_RUN;
    DSADR(dma->chan) = source_addr; //virt_to_bus(source_addr);
    DTADR(dma->chan) = virt_to_phys(target_addr);
    DCMD(dma->chan) = 0;
    DCMD(dma->chan) |= (DCMD_INCTRGADDR | DCMD_INCSRCADDR |
			DCMD_ENDIRQEN |
			DCMD_BURST32 |
			DCMD_WIDTH4 |
			bytes);
    DCSR(dma->chan) |= DCSR_RUN;
}


/** Calculate new DMAC transfer size */
void libera_dma_get_DMAC_csize(libera_dma_t *dma)
{
    long csize_temp;

    /* NOTE: There are several restrictions to DMAC
     *       transfer size (dma->csize):
     *       1. It should be <= dma->remaining, the remaining ammount
     *          of data (atoms) to be transfered via DMA.
     *       2. It should be <= LIBERA_DMA_BLOCK_ATOMS, the max. DMA
     *          transfer block.
     *       3. It should be <= the OB-fifo size, the ammount of data
     *          (atoms) waiting in FPGA OB-fifo.
     *       4. It should be <= the linear space left in DMA fifo,
     *          that is, the range from DMA fifo put pointer to the end
     *          of DMA fifo. Otherwise, the DMAC would write beyond
     *          the DMA fifo boundary; DMAC is not aware of our
     *          DMA fifo put pointer wrapping.
     *       5. It should be <= the space left in DMA fifo to prevent
     *          DMA fifo overflow.
     */
    csize_temp = MIN(
		     (MIN( dma->remaining,
			   LIBERA_DMA_BLOCK_ATOMS )),
		     (MIN( tailDMA_FIFO(dma),
			   DD_OB_SIZE(dma->obFIFOstatus) ))
		     );
    dma->csize = MIN(
		     (LIBERA_DMA_FIFO_ATOMS - 1 - lenDMA_FIFO(dma)),
		     csize_temp
		     );

    PDEBUG3("New dma->csize = %lu\n", dma->csize);
    PDEBUG3("  dma->remaining = %lu\n", dma->remaining);
    PDEBUG3("  tailDMA_FIFO = %u\n", tailDMA_FIFO(dma));
    PDEBUG3("  OB_size = %lu\n", DD_OB_SIZE(dma->obFIFOstatus));
    PDEBUG3("  DMA_SBC_fifo_left = %u\n", LIBERA_DMA_FIFO_ATOMS - 1 - lenDMA_FIFO(dma));
}


/** DMA interrupt handler */
void
libera_dma_interrupt(int irq, void *dev_id)
{
    struct libera_dd_device  *dd  = &libera_dd;
    libera_dma_t *dma = &lgbl.dma;
    unsigned long dcsr = DCSR(dma->chan);
    unsigned long flags;

    spin_lock_irqsave(&dma_spin_lock, flags);
    /* End interrupt */
    if (dcsr & DCSR_ENDINTR) {
        /* Acknowledge interrupt */
        PDEBUG3("DMA_int: End interrupt.\n");
        DCSR(dma->chan) |= DCSR_ENDINTR;

	/* Refresh OB status */
	dma->obFIFOstatus = readl(iobase + DD_OB_STATUS);

	/* Check Overrun */
	if (DD_OB_OVERRUN(dma->obFIFOstatus)) {
	    dma->Overrun = TRUE;
	    PDEBUG("Overrun: OB_status = 0x%08lx in dma irq.\n", dma->obFIFOstatus);
	    /* NOTE: The OVERRUN bit is set when the CB-PUT and
	     *       CB-GET pointers point to the same PAGE.
	     *       This can only happen when the CB-PUT pointer
	     *       caches the CB-GET pointer; a real overrun
	     *       situation.
	     *       CB-GET pointer can never catch the CB-PUT
	     *       pointer because of the SDRAM-OB_FIFO
	     *       synchronization in FPGA.
	     */
	}

        /* Refresh DMA state */
        dma->remaining -= dma->csize;
        if (putToDMA_FIFO(dma, dma->csize) < 0)
            printk(KERN_CRIT "libera: DMA irq: FIFO overflow.\n");
	libera_dma_get_DMAC_csize(dma);

        if (dma->csize > 0) {
	    /* Initiate new DMA transfer */
	    // TODO: LIBERA_IOBASE is not configurable !!!
	    libera_dma_command((LIBERA_IOBASE + DD_OB_FIFOBASE),
			       &dma->buf[dma->put],
			       dma->csize*sizeof(libera_atom_dd_t));
        } else {
            /* NOTE: This is not neccessarily the end of DMA transfer.
	     *       dma->csize == 0 is a regular state and will happen
	     *       when OB-fifo size < 8.
	     *       We will just silently let the reader process
	     *       (main DMA fifo reading loop) decide how to handle
	     *       this situation.
	     */
	    dma->DMAC_transfer = FALSE;
            DEBUG_ONLY(if (dma->csize < 0) {
                PDEBUG("libera: DMA irq: Negative dma->csize = %ld.\n",
		       dma->csize);
            });
        }

        /* Wake up the reader process */
        if (!dma->aborting)
            wake_up_interruptible(&dd->DMA_queue);
    }

    /* Acknowledge other DMA interrupts */
    if (dcsr & DCSR_BUSERR) {
        printk(KERN_CRIT "libera: DMA irq: DMA BUS ERROR!\n");
        DCSR(dma->chan) |= DCSR_BUSERR;
    }
    if (dcsr & DCSR_STARTINTR) {
        PDEBUG3("libera: DMA irq: Start interrupt.\n");
        DCSR(dma->chan) |= DCSR_STARTINTR;
    }

    spin_unlock_irqrestore(&dma_spin_lock, flags);

    return;
}

/** Called on seek() system call on a device.
 *
 * For details see: Alessandro Rubini et al., Linux Device Drivers, pp. 63.
 */
loff_t
libera_llseek(struct file *file, loff_t offset, int origin)
{
    /* Unseekable by default */
    return -ESPIPE;
}


/** Called on read() system call on a device.
 *
 * For details see: Alessandro Rubini et al., Linux Device Drivers, pp. 63.
 */
ssize_t
libera_read(struct file *file, char *buf, size_t count, loff_t *f_pos)
{
    /* Cannot read() by default */
    return -EINVAL;
}


/** Called on write() system call.
 *
 * For details see: Alessandro Rubini et al., Linux Device Drivers, pp. 63.
 */
ssize_t
libera_write(struct file *file, const char *buf, size_t count, loff_t *f_pos)
{
    /* Cannot write() by default */
    return -EINVAL;
}


/** Called on ioctl() system call.
 *
 * For details see: Alessandro Rubini et al., Linux Device Drivers, pp. 63.
 */
int
libera_ioctl(struct inode *inode, struct file *file,
	     unsigned int cmd, unsigned long arg)
{
    /* No ioctl() by default */
    return -EINVAL;
}


/** Called on open() system call.
 *
 * Main (dispatcher) open(). Dispatches open() system call to specific
 * open() methods for a corresponding device, accoording to minor numbers.
 *
 * For details see: Alessandro Rubini et al., Linux Device Drivers, pp. 63.
 */
static int
libera_open(struct inode *inode, struct file *file)
{
    unsigned int minor = MINOR(inode->i_rdev);

    /* Check minor number overflow */
    if (minor > LIBERA_MAX_MINOR)
	return -ENODEV;

    /* Make device data the private data */
    file->private_data = (void *) libera_dev[minor];

    /* Dispatch requests acoording to minor numbers to specific open() */
    file->f_op = libera_fops_array[minor];
    return file->f_op->open(inode, file);
}


/** Called on close() system call.
 *
 * Because of our dispatch mechanism on open(), this function should never
 * be called.
 *
 * For details see: Alessandro Rubini et al., Linux Device Drivers, pp. 63.
 */
static int
libera_release(struct inode *inode, struct file *file)
{
    /* NOTE: This should never be called because we dispatch on open()
     *       acoording to minor numbers.
     */
    LIBERA_LOG("This should never happen!!! file: %s, line: %d",
	       __FILE__, __LINE__);

    return 0;
}


/** Dispatcher (main) file operations
 *
 * For details see: Alessandro Rubini et al., Linux Device Drivers, pp. 66.
 */
static struct file_operations libera_fops = {
    owner:          THIS_MODULE,
    llseek:	    libera_llseek,
    read:           libera_read,
    write:          libera_write,
    ioctl:          libera_ioctl,
    open:           libera_open,
    release:        libera_release,
};

/** Initialize libera internal device structures.
 *
 * Initializes libera internal device structures. The device structures are
 * diffeerent because of differences in the devices, e.g. CFG, DD etc.
*/
static void libera_init_devs(void)
{
    struct libera_cfg_device   *dev_cfg   = &libera_cfg;
    struct libera_dd_device    *dev_dd    = &libera_dd;
    struct libera_fa_device    *dev_fa    = &libera_fa;
    struct libera_pm_device    *dev_pm    = &libera_pm;
    struct libera_sa_device    *dev_sa    = &libera_sa;
    struct libera_event_device *dev_event = &libera_event;
    struct libera_adc_device   *dev_adc   = &libera_adc;
    struct libera_dsc_device   *dev_dsc   = &libera_dsc;
    register int i;

    /* no junk in structures */
    memset(dev_cfg,   0, sizeof(struct libera_cfg_device));
    memset(dev_dd,    0, sizeof(struct libera_dd_device));
    memset(dev_fa,    0, sizeof(struct libera_fa_device));
    memset(dev_pm,    0, sizeof(struct libera_pm_device));
    memset(dev_sa,    0, sizeof(struct libera_sa_device));
    memset(dev_event, 0, sizeof(struct libera_event_device));
    memset(dev_adc,   0, sizeof(struct libera_adc_device));
    memset(dev_dsc,   0, sizeof(struct libera_dsc_device));

    /*
     * Device specific initialization follows
     */

    /* CFG */
    dev_cfg->minor = LIBERA_MINOR_CFG;
    dev_cfg->global = &lgbl;
    dev_cfg->spinlock = SPIN_LOCK_UNLOCKED;
    mutex_init(&dev_cfg->sem);
    dev_cfg->param[LIBERA_CFG_TRIGMODE] = LIBERA_TRIGMODE_GET;

    /* DD */
    dev_dd->minor = LIBERA_MINOR_DD;
    dev_dd->global = &lgbl;
    dev_dd->spinlock = SPIN_LOCK_UNLOCKED;
    mutex_init(&dev_dd->sem);
    dev_dd->decimation = 1;
    init_waitqueue_head(&dev_dd->DD_queue);
    init_waitqueue_head(&dev_dd->DMA_queue);

    /* FA */
    dev_fa->minor = LIBERA_MINOR_FA;
    dev_fa->global = &lgbl;
    dev_fa->spinlock = SPIN_LOCK_UNLOCKED;
    mutex_init(&dev_fa->sem);

    /* PM */
    dev_pm->minor = LIBERA_MINOR_PM;
    dev_pm->global = &lgbl;
    dev_pm->spinlock = SPIN_LOCK_UNLOCKED;
    mutex_init(&dev_pm->sem);

    /* SA */
    dev_sa->minor = LIBERA_MINOR_SA;
    dev_sa->global = &lgbl;
    dev_sa->spinlock = SPIN_LOCK_UNLOCKED;
    mutex_init(&dev_sa->sem);
    dev_sa->buf_head = dev_sa->buf;
    dev_sa->buf_tail = dev_sa->buf;
    dev_sa->inputovr = FALSE;

    /* EVENT */
    dev_event->minor = LIBERA_MINOR_EVENT;
    dev_event->global = &lgbl;
    dev_event->spinlock = SPIN_LOCK_UNLOCKED;
    mutex_init(&dev_event->sem);
    mutex_init(&dev_event->CTIME_sem);
    init_waitqueue_head(&dev_event->SC_queue);
    init_waitqueue_head(&dev_event->MC_queue);
    init_waitqueue_head(&dev_event->EVENT_queue);
    for (i=0; i < TRIG_EVENTS_MAX; i++)
    {
	flushFIFO(&dev_event->sc_timestamps[i]);
	flushFIFO(&dev_event->mc_timestamps[i]);
	flushCircBuf(&dev_event->paired_timestamps[i]);
	memset(&dev_event->sc_self_inc, 0, sizeof(glitch_CB_t));
	memset(&dev_event->mc_self_inc, 0, sizeof(glitch_CB_t));
	memset(&dev_event->sc_trig10, 0, sizeof(glitch_CB_t));
	memset(&dev_event->mc_trig10, 0, sizeof(glitch_CB_t));
    }
    dev_event->HB_start_lmt = 0x8000000;
    /*
       dev_event->HB_start_lmt = d * LIBERA_DD_CIRCBUF_ATOMS;
       while (dev_event->HB_start_lmt < 0x8000000)
       dev_event->HB_start_lmt += (d * LIBERA_DD_CIRCBUF_ATOMS);
    */

    dev_event->ValidTrigVector = 0xffffffff;

    dev_event->list.head = &dev_event->evgen;
    dev_event->list.head->time = dev_event->HB_start_lmt;
    dev_event->list.head->event = ((1 << T_EG_DDC) |
                                   (1 << T_EG_HB)  |
                                   (1 << T_EG_FAI) |
                                   (1 << T_EG_FA)  |
                                   (1 << T_EG_SA));
    dev_event->list.head->prev = NULL;
    dev_event->list.head->next = NULL;

    /* ADC */
    dev_adc->minor = LIBERA_MINOR_ADC;
    dev_adc->global = &lgbl;
    dev_adc->spinlock = SPIN_LOCK_UNLOCKED;
    mutex_init(&dev_adc->sem);
    init_waitqueue_head(&dev_adc->ADC_queue);
    dev_adc->op = NULL;

    /* DSC */
    dev_dsc->minor = LIBERA_MINOR_DSC;
    dev_dsc->global = &lgbl;
    dev_dsc->spinlock = SPIN_LOCK_UNLOCKED;
    mutex_init(&dev_dsc->sem);


    /* Libera globals initialization & WO Mirroring */
    memset(&lgbl, 0, sizeof(struct libera_global));

}


void __you_cannot_kmalloc_that_much(void)
{
  LIBERA_LOG("You cannot kmalloc that much!\n");
}


/** Libera driver initialization.
 *
 * Registers the character device, requests the I/O memory region and
 * installs the interrupt handler.
 * NOTE: Goto statements are generally not a good programming
 *       practice, but module initialization is AFAIK one of the few cases
 *       where goto statements are a way to go, as no one in the kernel will
 *       clean after ourselves in case something bad happens during the
 *       initialization.
 */
static int __init libera_init(void)
{
    struct libera_event_device *event  = &libera_event;
    struct libera_pm_device    *dev_pm = &libera_pm;
    int ret;

    /* Module parameters sanity check */
    if (flmcdHz == 0)
    {
	ASSERT(TRUE);
	return -EINVAL;
    }

    /* Initialize Libera internal dev structures */
    libera_init_devs();

    /* I/O memory resource allocation */
    iobase = (unsigned)ioremap_nocache(iobase, iorange);
    ret = check_mem_region(iobase, iorange);
    if (ret < 0)
    {
	LIBERA_LOG("Unable to claim I/O memory, range 0x%lx-0x%lx\n",
		   iobase, iobase+iorange-1);
	if (ret == -EINVAL)
	    LIBERA_LOG("Invalid I/O memory range.\n");
	else if (ret == -EBUSY)
	    LIBERA_LOG("I/O memory already in use.\n");
	else
	    LIBERA_LOG("check_mem_region() returned %d.\n", ret);
	goto err_IOmem;
    }
    else if (request_mem_region(iobase, iorange, LIBERA_NAME) == NULL)
    {
	LIBERA_LOG("I/O memory region request failed (range 0x%lx-0x%lx)\n",
		   iobase, iobase+iorange-1);
	ret = -ENODEV;
	goto err_IOmem;
    }

    /* Register character device for communication via VFS */
    ret = register_chrdev(LIBERA_MAJOR, LIBERA_NAME, &libera_fops);
    if (ret < 0) {
	LIBERA_LOG("Unable to register major %d.\n", LIBERA_MAJOR);
	if (ret == -EINVAL)
	    LIBERA_LOG("Invalid major number.\n");
	else if (ret == -EBUSY)
	    LIBERA_LOG("Device with major %d already registered.\n",
		       LIBERA_MAJOR);
	else
	    LIBERA_LOG("register_chrdev() returned %d.\n", ret);

	goto err_ChrDev;
    }

    /* Libera Brilliance detection */
    lgbl.feature = readl(iobase + FPGA_FEATURE_ITECH);
    if ( LIBERA_IS_BPM(lgbl.feature) ) {
        lgbl.dfa = readl(iobase + FPGA_INFO_DEC_CIC_FIR);
        lgbl.d = readl(iobase + FPGA_INFO_DEC_DDC);
    }
    else {
        lgbl.dfa = 0;
        lgbl.d = 0;
    }

    /* Default Libera FA decimation DFA */
    lgbl.dcic = lgbl.dfa >> 16;
    lgbl.num_dfir = lgbl.dfa % (1 << 16);
    lgbl.dfa = lgbl.dcic * lgbl.num_dfir; // DFA = D_CIC * D_FIR
    lgbl.num_dfir *= 3; // number of FIR coefficients

    /* Syslog entry */
    printk(LIBERA_SYSLOG_LEVEL "Libera %s%s, version %s (%s %s)\n",
	   libera_desc.name,
           LIBERA_IS_BRILLIANCE(lgbl.feature) ? " BRILLIANCE" : "",
           XSTR( RELEASE_VERSION ),
           __DATE__, __TIME__);
    printk(LIBERA_SYSLOG_LEVEL "       Initial F_LMC = %ld dHz\n", flmcdHz);
    printk(LIBERA_SYSLOG_LEVEL "       D   = %ld \n", lgbl.d);
    printk(LIBERA_SYSLOG_LEVEL "       DFA = %ld \n", lgbl.dfa);
    printk(LIBERA_SYSLOG_LEVEL "       CIC = %ld \n", lgbl.dcic);
    printk(LIBERA_SYSLOG_LEVEL "       FIR = %ld \n", lgbl.num_dfir);
    LIBERA_LOG("I/O memory range 0x%lx-0x%lx\n",
	       iobase, iobase+iorange-1);

    /* DMA buffer */
    lgbl.dma.buf = (libera_atom_dd_t *)
	__get_free_pages(GFP_KERNEL|__GFP_DMA,
			 LIBERA_DMA_PAGE_ORDER);
    if (!lgbl.dma.buf) {
        LIBERA_LOG("FATAL: Can't allocate DMA buffer.\n");
        ret = -ENOMEM;
        goto err_DMABUF;
    }

    /* DMA channel */
    lgbl.dma.chan = pxa_request_dma(LIBERA_NAME, DMA_PRIO_HIGH,
				    libera_dma_interrupt, 0);
    if (lgbl.dma.chan < 0) {
        LIBERA_LOG("FATAL: Can't register DMA channel.\n");
        ret = -ECHRNG;
        goto err_DMACH;
    }
    LIBERA_LOG("DMA%i: %lu bytes at 0x%08lx\n",
	       lgbl.dma.chan,
	       PAGE_SIZE << LIBERA_DMA_PAGE_ORDER,
	       (unsigned long)lgbl.dma.buf);

    lgbl.dma.aborting = FALSE;

    /* PM buffer */
    if (pmsize > LIBERA_PMSIZE_MAX) {
        LIBERA_LOG("FATAL: Requested PM buffer to big, max=%d.\n",
                LIBERA_PMSIZE_MAX);
        ret = -EINVAL;
        goto err_PMBUF;
    }

    dev_pm->buf = (libera_atom_dd_t *)
        vmalloc(pmsize*sizeof(libera_atom_dd_t));
    if (!dev_pm->buf) {
        LIBERA_LOG("FATAL: Can't allocate PM buffer.\n");
        ret = -ENOMEM;
        goto err_PMBUF;
    }
    memset(dev_pm->buf, 0, pmsize*sizeof(libera_atom_dd_t));
    PDEBUG("Allocated and cleared PM buffer size %d (%d bytes).\n",
            pmsize, pmsize*sizeof(libera_atom_dd_t));

    /* XCEP GPIO for IRQ */
    set_irqMask(0UL); /* Initial IRQ disable */

    pxa_gpio_mode(gpio);

    lgbl.irq = IRQ_GPIO(gpio);


    /* Interrupt handler registration */
    ret = request_irq(lgbl.irq, libera_interrupt,
		      IRQF_DISABLED, LIBERA_NAME, NULL);
    if (ret) {
	LIBERA_LOG("Can't register interrupt on IRQ %i\n", lgbl.irq);
	lgbl.irq = -1;
	goto err_IRQ;
    }
    else {
	LIBERA_LOG("Registered interrupt on IRQ %i\n", lgbl.irq);
	// Clear any Helper IRQ
	readl(iobase + SBC_IRQ2);
	set_irqMask(LIBERA_INTERRUPT_SC_MASK |
		    LIBERA_INTERRUPT_MC_MASK |
		    LIBERA_INTERRUPT_HELPSC_MASK |
		    LIBERA_INTERRUPT_HELPMC_MASK |
		    LIBERA_INTERRUPT_DD_MASK |
		    LIBERA_INTERRUPT_SA_MASK |
		    LIBERA_INTERRUPT_ILK_MASK);
	libera_sctrig_enable(event,
			     TRIGGER_BIT(T_TRIG_CTIME) |
			     TRIGGER_BIT(T_TRIG_TRIGGER) |
			     TRIGGER_BIT(T_TRIG_POST_MORTEM)
			     );
	libera_mctrig_enable(event,
			     TRIGGER_BIT(T_TRIG_CTIME) |
			     TRIGGER_BIT(T_TRIG_TRIGGER) |
			     TRIGGER_BIT(T_TRIG_POST_MORTEM)
			     );
    }

    set_irq_type(IRQ_GPIO(gpio),  IRQ_TYPE_EDGE_RISING);

    /* Initialize Libera family memeber specific internal dev structures */
    libera_init_specific();

    /* Report FPGA info */
#ifdef FPGA_REPORT
    fpga_info = readl(iobase + FPGA_INFO_COMPILE_TIME);
    printk(LIBERA_SYSLOG_LEVEL "Libera FPGA (%02lu%02lu%02lu) build %u\n",
           (fpga_info & 0xffff0000UL) >> 16,
           (fpga_info & 0x0000ff00UL) >> 8,
           (fpga_info & 0x000000ffUL),
           readl(iobase + FPGA_INFO_BUILDNO)
           );
    fpga_info = readl(iobase + FPGA_INFO_ID);
    memcpy(fpga_id, &fpga_info, sizeof(fpga_info));
    fpga_id[4] = 0;
    printk(LIBERA_SYSLOG_LEVEL "       ID  = %s\n", fpga_id);
    printk(LIBERA_SYSLOG_LEVEL "       D   = %u\n",
           readl(iobase + FPGA_INFO_DEC_DDC));
    printk(LIBERA_SYSLOG_LEVEL "       CIC_FIR = 0x%08lx\n",
           readl(iobase + FPGA_INFO_DEC_CIC_FIR));
#endif

    /* Module loaded OK */
    return 0;

 err_IRQ:
    vfree(dev_pm->buf);

 err_PMBUF:
    pxa_free_dma(lgbl.dma.chan);

 err_DMACH:
    free_pages((unsigned long)lgbl.dma.buf, LIBERA_DMA_PAGE_ORDER);

 err_DMABUF:
    unregister_chrdev(LIBERA_MAJOR, LIBERA_NAME);

 err_ChrDev:
    release_mem_region(iobase,iorange);

 err_IOmem:

    return ret;
}


/** Libera driver cleanup
 *
 * Deinstalls the interrupt handler, unregisters the character device and
 * frees the I/O memory region.
 */
static void __exit libera_exit(void)
{
	struct libera_pm_device    *dev_pm = &libera_pm;

	/* Uninstall interrupt handler */
	if (lgbl.irq >= 0) {
	set_irqMask(0UL);
	writel(0UL, iobase + T_SC_TRIGGER_MASK);
	writel(0UL, iobase + T_MC_TRIGGER_MASK);
		free_irq(lgbl.irq, NULL);
	}

	/* PM buffer */
	vfree(dev_pm->buf);

	/* DMA */
	if (lgbl.dma.chan >= 0)
	pxa_free_dma(lgbl.dma.chan);
	free_pages((unsigned long)lgbl.dma.buf, LIBERA_DMA_PAGE_ORDER);

	/* Unregister character device for communication via VFS */
	unregister_chrdev(LIBERA_MAJOR, LIBERA_NAME);

	/* Release I/O memory regions */
	if (iobase != 0) {
		release_mem_region(iobase, iorange);
		iounmap((void *)(iobase & PAGE_MASK));
	}
	printk(KERN_INFO LIBERA_NAME " : Libera %s, version %s (%s %s): unloaded.\n",
	libera_desc.name, XSTR( RELEASE_VERSION ), __DATE__, __TIME__);
}



/** Register module init and cleanup functions.
 *
 * Registers module initialization and cleanup functions.
 * NOTE: This is the modern mechanism for registering a module's
 *       initialization and cleanup functions. Doing it this way instead of
 *       using init_module() and cleanup_module() is crucial for possible
 *       future inclusion into a monolythic linux kernel (not a module).
 *
 * For details see: Alessandro Rubini et al., Linux Device Drivers, pp. 35.
 */
module_init(libera_init);
module_exit(libera_exit);
