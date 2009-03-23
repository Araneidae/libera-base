/* $Id: libera_event.c 2233 2008-10-20 12:14:46Z matejk $ */

//! \file libera_event.c
//! Implements Libera GNU/Linux driver (A)synchronous Event device (libera.event).

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

#define __NO_VERSION__
#include <asm/uaccess.h>
#include <asm/ioctls.h>
#include <linux/wait.h>
#include <linux/poll.h>

#include "libera_kernel.h"
#include "libera_event.h"
#include "libera_dd.h"
#include "libera.h"

extern ssize_t libera_acq_pm(void);

/** A macro for reading SC trigger */
#define GET_SC_TRIGGER_MACRO(X, timeout)                             \
  if (! (event_local->sc_trigVec & TRIGGER_BIT(X))) return -EPERM;   \
  ret = getBlockedFromFIFOuser(&dev->sc_timestamps[X],               \
		               &dev->SC_queue,                       \
		               (libera_hw_time_t *)arg,              \
			       timeout);

/** A macro for reading MC trigger */
#define GET_MC_TRIGGER_MACRO(X, timeout)                                      \
  if (! (event_local->mc_trigVec & TRIGGER_BIT(X))) return -EPERM;   \
  ret = getBlockedFromFIFOuser(&dev->mc_timestamps[X],               \
		               &dev->MC_queue,                       \
		               (libera_hw_time_t *)arg,              \
			       timeout);

/** Enable SC trigger
 *
 * Enables Sc trigger(), defined by \param mask.
 * On success, 0 is returned.
 * On error, -1 is returned.
 */
int libera_sctrig_enable(struct libera_event_device * dev,
				       const unsigned long mask)
{
  if (mask & ~TRIG_ALL_MASK) return -1;
  dev->sc_trigVec |= mask;
  /* keep bit SELF_INC always set */
  writel(dev->sc_trigVec | 0x80000000, iobase + T_SC_TRIGGER_MASK);
  PDEBUG2("SCTRIG_Enable: Written 0x%lx to T_SC_TRIGGER_MASK\n", 
	  dev->sc_trigVec | 0x80000000);

  return 0;
}


/** Enable MC trigger
 *
 * Enables Sc trigger(), defined by \param mask.
 * On success, 0 is returned.
 * On error, -1 is returned.
 */
int libera_mctrig_enable(struct libera_event_device * dev,
				       const unsigned long mask)
{
  if (mask & ~TRIG_ALL_MASK) return -1;
  dev->mc_trigVec |= mask;
  /* keep bit SELF_INC always set */
  writel(dev->mc_trigVec | 0x80000000, iobase + T_MC_TRIGGER_MASK);
  PDEBUG2("MCTRIG_Enable: Written 0x%lx to T_MC_TRIGGER_MASK\n", 
	  dev->mc_trigVec | 0x80000000);
  return 0;
}

/** Libera EVENT Device: Called on open() 
 *
 * Takes care of proper opening of the EVENT device and updates the informaton
 * for access control.
 * For details see: Alessandro Rubini et al., Linux Device Drivers, pp. 64.
 */
static int 
libera_event_open(struct inode *inode, struct file *file)
{
    struct libera_event_device *dev =
	(struct libera_event_device *) file->private_data;
    struct libera_event_local *event_local = NULL;

    if (!dev) return -ENODEV;

    mutex_lock(&dev->sem);

    /* Get a fresh event_local to store per fd */ 
    event_local = (struct libera_event_local *)
	kmalloc(sizeof(struct libera_event_local), GFP_KERNEL);
    if (event_local)
    {
	/* init all triggers */
	event_local->sc_trigVec = 0;
	event_local->mc_trigVec = 0;
    }
    else
    {
	mutex_unlock(&dev->sem);
	return -ENOMEM;
    }
    /* Store current event_local for our reference */
    file->f_version = (unsigned long)event_local;

    /* Is it an open() for writing? */
    if (file->f_mode & FMODE_WRITE) {
	/* Is it the first open() for writing */
	if (!dev->master_filp) {
	    dev->master_filp = file;
	    flush_eventFIFO(&dev->events);
	} else {
	    mutex_unlock(&dev->sem);
	    return -EBUSY;
	}
	dev->writers++;
    }

    if (file->f_mode & FMODE_READ)  dev->readers++;

    PDEBUG3("Opened libera.event (%p): readers=%d, writers=%d\n", 
	   file, dev->readers, dev->writers);

    dev->open_count++; /* internal counter */
    mutex_unlock(&dev->sem);

    return 0; /* success */
}


/** Libera Event Device: Called on close() 
 *
 * Takes care of proper opening of the EVENT device and updates the informaton
 * for access control.
 * For details see: Alessandro Rubini et al., Linux Device Drivers, pp. 64.
 */
static int 
libera_event_release(struct inode *inode, struct file *file)
{
    struct libera_event_device *dev =
	(struct libera_event_device *) file->private_data;
    struct libera_event_local *event_local = 
	(struct libera_event_local *)file->f_version;
    

    dev->open_count--; /* internal counter */

    if (file->f_mode & FMODE_READ) {
	dev->readers--;
	if (!(file->f_mode & FMODE_WRITE)) {
	    // Read-only
	    dev->mc_trigVec &= ~(event_local->mc_trigVec);
	    dev->sc_trigVec &= ~(event_local->sc_trigVec);
	    /* keep bit SELF_INC always set */
	    writel(dev->sc_trigVec | 0x80000000, iobase + T_SC_TRIGGER_MASK);
	    writel(dev->mc_trigVec | 0x80000000, iobase + T_MC_TRIGGER_MASK);
	}
    }
    if (file->f_mode & FMODE_WRITE) {
	dev->writers--;
	/* Check whether the last writer is closing */
	if (!dev->writers)
	{
	    dev->master_filp = NULL;
	    /* Event FIFO cleanup */
	    dev->global->event_mask = 0;
	    flush_eventFIFO(&dev->events);
	}
    }

    kfree(event_local);
    file->f_version = (unsigned long)NULL;
  

    PDEBUG3("Closed libera.event (%p): readers=%d, writers=%d\n", 
	   file, dev->readers, dev->writers);

    return 0; /* success */
}


/** Get data from FIFO and block/wait until "FIFO not empty".
 *
 * Reads data from libera_fifo \param q, after waiting/sleeping on
 * wait queue \param wq and copies the data to \param time.
 * On success, 0 is returned.
 * On error, meaningful negative errno is returned.
 */
int
getBlockedFromFIFO(struct libera_fifo* const q,
		   wait_queue_head_t* const wq,
		   libera_hw_time_t *time)
{
    struct libera_event_device  *event  = &libera_event;
    wait_queue_t wait;
    libera_hw_time_t *data;
    int ret = 0;

    /* Get the device semaphore */
    if (mutex_lock_interruptible(&event->sem))
	return -ERESTARTSYS;

    /* Check fifo size & wait/sleep if neccessary */
    if (emptyFIFO(q))
    {
	init_waitqueue_entry(&wait,current);
	add_wait_queue(wq,&wait);
	for (;;)
	{
	    set_current_state(TASK_INTERRUPTIBLE);
	    if (not_emptyFIFO(q)) {
		break;
	    }
	    if (!signal_pending(current)) {
		mutex_unlock(&event->sem);
		schedule();
		mutex_lock(&event->sem);
		continue;
	    }
	    ret = -ERESTARTSYS;
	    break;
	}
	set_current_state(TASK_RUNNING);
	remove_wait_queue(wq, &wait);
    }

    if (ret) goto out;
    
    /* Get data from FIFO */
    if (! (data = getFromFIFO(q)) ) {
	ret = -EFAULT;
	goto out;
    }
    *time = *data;

 out:
    mutex_unlock(&event->sem);
    return ret;
}


/** Copy data from FIFO to userland and block/wait until "FIFO not empty".
 *
 * Reads data from libera_fifo \param q, after waiting/sleeping on
 * wait queue \param wq and copies the data to userland \param buf.
 * On success, 0 is returned.
 * On error, meaningful negative errno is returned.
 */
static int
getBlockedFromFIFOuser(struct libera_fifo* const q,
		       wait_queue_head_t* const wq,
		       libera_hw_time_t *buf,
		       long timeout)
{
    struct libera_event_device  *event  = &libera_event;
    wait_queue_t wait;
    libera_hw_time_t *data;
    int ret = 0;

    /* Get the device semaphore */
    if (mutex_lock_interruptible(&event->sem))
	return -ERESTARTSYS;

    /* Check fifo size & wait/sleep if neccessary */
    if (emptyFIFO(q))
    {
	init_waitqueue_entry(&wait,current);
	add_wait_queue(wq,&wait);
	for (;;)
	{
	    set_current_state(TASK_INTERRUPTIBLE);
	    if (not_emptyFIFO(q)) {
		break;
	    }
	    if (!signal_pending(current)) {
		mutex_unlock(&event->sem);
		timeout = schedule_timeout(timeout);
		if (!timeout) {
		    ret = -EAGAIN;
		    break;
		}
		mutex_lock(&event->sem);
		continue;
	    }
	    ret = -ERESTARTSYS;
	    break;
	}
	set_current_state(TASK_RUNNING);
	remove_wait_queue(wq, &wait);
    }

    if (ret) goto out;

    /* Get data from FIFO */
    if (! (data = getFromFIFO(q)) ) {
	ret = -EFAULT;
	goto out;
    }
    ret = copy_to_user(buf, data, sizeof(libera_hw_time_t)) ? -EFAULT : 0;

 out:
    mutex_unlock(&event->sem);
    return ret;
}


/** Libera Event Device: Command decoder & Hardware actions 
 *
 * Takes care of decoding and dispatching the ioctl() commands.
 */
static int 
libera_event_cmd(struct libera_event_device *dev, struct file *file,
	       unsigned int cmd, unsigned long arg)
{   
    struct libera_event_local *event_local = 
	(struct libera_event_local *)file->f_version;
    struct libera_cfg_device *cfg = &libera_cfg;
    int ret = 0;
    register unsigned short int i;
    unsigned long temp;


    // TODO: Change all ioctl() calls to pass a pointer as an argument
    //       instead of value.
    /* Decode vaild commands */
    switch(cmd)
    {
    case LIBERA_EVENT_SET_DAC_A:
      // if DAC would overflow return invalid argument
      if (arg & ~0xFFFFUL) return -EINVAL; 
      writel(arg, iobase + EVENT_MC_DAC);
      break;
    case LIBERA_EVENT_SET_DAC_B:
      // if DAC would overflow return invalid argument
      if (arg & ~0xFFFFUL) return -EINVAL; 
      writel(arg, iobase + EVENT_SC_DAC);
      break;
    case LIBERA_EVENT_ENABLE_SC_TRIG:
      PDEBUG3("Enabling SC TRIG: arg=0x%08lx\n", arg);
      PDEBUG3("(arg & ~TRIG_ALL_MASK) = 0x%08lx\n", (arg & ~TRIG_ALL_MASK));
      PDEBUG3("(arg & dev->mc_trigVec) = 0x%08lx\n", (arg & dev->mc_trigVec));
      // if vector is zero or too long ret invalid argument
      if ((arg == 0) || (arg & ~TRIG_ALL_MASK)) return -EINVAL; 
      // if trigger already enabled return EBUSY
      if (arg & dev->sc_trigVec) return -EBUSY;
      event_local->sc_trigVec |= arg;
      i=0;
      //flush all to be enabled fifos
      PDEBUG3("(arg & TRIG_ALL_MASK) = 0x%08lx\n", (arg & TRIG_ALL_MASK));
      while (arg & TRIG_ALL_MASK)
	{
	  register int m=TRIG_ZERO;
	  while (!(m & arg)) 
	    {
	      i++; m <<= 1;
	    }
	  PDEBUG3("Flushing SC fifo %u\n", i);
	  flushFIFO(& dev->sc_timestamps[i]);
	  arg &= ~m;
	}
      PDEBUG3("Calling libera_sctrig_enable(), 0x%08lx\n", event_local->sc_trigVec);
      libera_sctrig_enable(dev, event_local->sc_trigVec);
      break;
    case LIBERA_EVENT_ENABLE_MC_TRIG:
      PDEBUG3("Enabling MC TRIG: arg=0x%08lx\n", arg);
      PDEBUG3("(arg & ~TRIG_ALL_MASK) = 0x%08lx\n", (arg & ~TRIG_ALL_MASK));
      PDEBUG3("(arg & dev->mc_trigVec) = 0x%08lx\n", (arg & dev->mc_trigVec));
      // if vector is zero or too long return invalid argument
      if ((arg == 0) || (arg & ~TRIG_ALL_MASK)) return -EINVAL; 
      // if trigger already enabled return EBUSY
      if (arg & dev->mc_trigVec) return -EBUSY;
      event_local->mc_trigVec |= arg;
      i=0;
      //flush all to be enabled fifos
      PDEBUG3("(arg & TRIG_ALL_MASK) = 0x%08lx\n", (arg & TRIG_ALL_MASK));
      while (arg & TRIG_ALL_MASK)
	{
	  register int m=TRIG_ZERO;
	  while (!(m & arg)) 
	    {
	      i++; m <<= 1;
	    }
	  PDEBUG3("Flushing MC fifo %u\n", i);
	  flushFIFO(& dev->mc_timestamps[i]);
	  arg &= ~m;
	}
      PDEBUG3("Calling libera_mctrig_enable(), 0x%08lx\n", event_local->mc_trigVec);
      libera_mctrig_enable(dev, event_local->mc_trigVec);
      break;
    case LIBERA_EVENT_GET_SC_TRIGGER_19:
      return -ENOSYS; //GET_SC_TRIGGER_MACRO(19, MAX_SCHEDULE_TIMEOUT)
      break;
    case LIBERA_EVENT_GET_MC_TRIGGER_19:
      return -ENOSYS; //GET_MC_TRIGGER_MACRO(19, MAX_SCHEDULE_TIMEOUT)
      break;
    case LIBERA_EVENT_GET_SC_TRIGGER_10:
      GET_SC_TRIGGER_MACRO(6, HZ)
      break;
    case LIBERA_EVENT_GET_SC_TRIGGER_9:
      GET_SC_TRIGGER_MACRO(5, HZ)
      break;
    case LIBERA_EVENT_GET_MC_TRIGGER_10:
      GET_MC_TRIGGER_MACRO(6, HZ)
      break;
    case LIBERA_EVENT_GET_MC_TRIGGER_1:
        GET_MC_TRIGGER_MACRO(1, MAX_SCHEDULE_TIMEOUT)
            break;
    case LIBERA_EVENT_GET_MC_TRIGGER_0:
        GET_MC_TRIGGER_MACRO(0, MAX_SCHEDULE_TIMEOUT)
            break;
    case LIBERA_EVENT_SET_SC_EVENT:
      writel(TRIGGER_BIT(5), iobase + T_SC_EVENTGEN);
      break;
    case LIBERA_EVENT_GET_FLMC:
	ret = copy_to_user((libera_U32_t *)arg, &flmcdHz, sizeof(flmcdHz));
	break;
    case LIBERA_EVENT_SET_FLMC:
	ret = copy_from_user(&temp, (libera_U32_t *)arg, sizeof(temp));
	if (temp == 0)
	{
	    PDEBUG("Received flmcdHz=0. Keeping old flmcdHz=%lu\n",
		   flmcdHz);
	}
	else
	{
	    flmcdHz = temp;
	    PDEBUG3("New FLMC_dHz = %lu\n", flmcdHz);
	}
	break;

    case LIBERA_EVENT_GET_MCPHI:
	ret = copy_to_user((libera_hw_time_t *)arg, 
			   &dev->settime.lmt.mcphi,
			   sizeof(libera_hw_time_t));
	break;
    case LIBERA_EVENT_SET_MCPHI:
	ret = copy_from_user(&dev->settime.lmt.mcphi,
			     (libera_hw_time_t *)arg,
			     sizeof(libera_hw_time_t));
	break;

    case LIBERA_EVENT_GET_SCPHI:
	ret = copy_to_user((libera_hw_time_t *)arg, 
			   &dev->settime.lst.scphi,
			   sizeof(libera_hw_time_t));
	break;
    case LIBERA_EVENT_SET_SCPHI:
	ret = copy_from_user(&dev->settime.lst.scphi,
			     (libera_hw_time_t *)arg,
			     sizeof(libera_hw_time_t));
	break;

    case LIBERA_EVENT_SET_NCO:
        {
            libera_U32_t nco;
            ret = copy_from_user(&nco,
                                 (libera_U32_t *)arg,
                                 sizeof(libera_U32_t));
            
            PDEBUG("NCO = 0x%08x\n", nco);
            writel(nco, iobase + DDC_NCO);
        }
        break;

    case LIBERA_EVENT_SET_MCPLL:
	ret = copy_from_user(&temp,
			     (libera_U32_t *)arg,
			     sizeof(libera_U32_t));
        PDEBUG("MCPLL status: %lu\n", temp);
        cfg->param[LIBERA_CFG_MCPLL] = temp & FAI_PLL_STATUS_MASK;
        writel((readl(iobase + FAI_SW_STATUS) & ~FAI_PLL_STATUS_MASK) | cfg->param[LIBERA_CFG_MCPLL],
               iobase + FAI_SW_STATUS);
        libera_send_event(LIBERA_EVENT_CFG, LIBERA_CFG_MCPLL);
	break;

    case LIBERA_EVENT_SET_SCPLL:
	ret = copy_from_user(&temp,
			     (libera_U32_t *)arg,
			     sizeof(libera_U32_t));
        PDEBUG("SCPLL status: %lu\n", temp);
        cfg->param[LIBERA_CFG_SCPLL] = temp & FAI_PLL_STATUS_MASK;
        libera_send_event(LIBERA_EVENT_CFG, LIBERA_CFG_SCPLL);
	break;

    case LIBERA_EVENT_GET_CTIME:
	{    
	    libera_Ltimestamp_t ctime;
	    libera_get_CTIME(&ctime);
	    ret = copy_to_user((libera_Ltimestamp_t *)arg,
			       &ctime, sizeof(libera_Ltimestamp_t));
	}
	break;

	/* ST: GET/SET System time */
    case LIBERA_EVENT_GET_ST:
	{    
	    libera_Ltimestamp_t ctime;
	    struct timespec  st;
	    libera_get_CTIME(&ctime);
	    lst2st(&ctime.lst, &st);
	    ret = copy_to_user((struct timespec *)arg,
			       &st, sizeof(struct timespec));
	}
	break;

    case LIBERA_EVENT_SET_ST:
	{
	    libera_HRtimestamp_t ts;
	    ret = copy_from_user(&ts, (libera_HRtimestamp_t *)arg, 
				 sizeof(libera_HRtimestamp_t));
	    st2lst(&ts.st, &dev->settime.lst.ref);
	    cfg->param[LIBERA_CFG_TRIGMODE] = LIBERA_TRIGMODE_SET;
	    dev->settime.update |= LIBERA_SETTIME_ST;
	    PDEBUG2("ST: set_LST(0x%08lx%08lx) armed for next trigger.\n",
		    ULL(dev->settime.lst.ref));
	}
	break;

	/* MT: GET/SET Machine time */
    case LIBERA_EVENT_GET_MT:
	{    
	    libera_Ltimestamp_t ctime;
	    libera_hw_time_t mt;
	    libera_get_CTIME(&ctime);
	    lmt2mt(&ctime.lmt, &mt);
	    ret = copy_to_user((libera_hw_time_t *)arg,
			       &mt, sizeof(libera_hw_time_t));
	}
	break;
	
    case LIBERA_EVENT_SET_MT:
	{
#ifdef SETTIME_MT
	    libera_hw_time_t mt;
	    ret = copy_from_user(&mt,
                                 (libera_hw_time_t *)arg, 
				 sizeof(libera_hw_time_t));
	    mt2lmt(&mt, &dev->settime.lmt.ref);
#else
            libera_HRtimestamp_t ts;
	    ret = copy_from_user(&ts,
                                 (libera_HRtimestamp_t *)arg, 
				 sizeof(libera_HRtimestamp_t));
            
            PDEBUG("MT = 0x%08lx%08lx\n", ULL(ts.mt));
            PDEBUG("P  = %lu\n", ts.phase);
            if (ts.phase >= lgbl.d)
                return -EINVAL;

            mt2lmt(&ts.mt, &dev->settime.lmt.ref);
            dev->settime.lmt.ref += ts.phase;
            PDEBUG("LMT_ref  = 0x%08lx%08lx\n", ULL(dev->settime.lmt.ref));
#endif
	    cfg->param[LIBERA_CFG_TRIGMODE] = LIBERA_TRIGMODE_SET;
	    dev->settime.update |= LIBERA_SETTIME_MT;
	    /* Arm FA/FAI reset */
	    writel(0xffffffff, iobase + FAI_COUNTER_RESET);
	    PDEBUG2("MT: set_LMT(0x%08lx%08lx) armed for next trigger.\n",
		    ULL(dev->settime.lmt.ref));
	}
	break;

    case LIBERA_EVENT_GET_TRIG_TRIGGER:
	{    
	    libera_Ltimestamp_t *last_trigger;
	    last_trigger = 
	      getFromCircBuf(&dev->paired_timestamps[T_TRIG_TRIGGER]);
	    ret = copy_to_user((libera_Ltimestamp_t *)arg,
			       last_trigger, sizeof(libera_Ltimestamp_t));
	}
	break; 

    case LIBERA_EVENT_GET_FIFOLEN_SC:
        /* NOTE: Although usually a GET method would only require
         *       copying to userland we need to read the FIFO No.
         *       first in order to get the index.
         */
        {
            libera_U32_t fifo, val;
            ret = copy_from_user(&fifo,
                                 (libera_U32_t *)arg,
                                 sizeof(libera_U32_t));
            if (ret) return ret;
            if (fifo >= TRIG_EVENTS_MAX)
                return -EINVAL;
            
            val = lenFIFO(&dev->sc_timestamps[fifo]);
            ret = copy_to_user((libera_U32_t *)arg,
                               &val,
                               sizeof(libera_U32_t));
            PDEBUG3("GET_FIFOLEN_SC: fifo=%u, val=%u\n", fifo, val);
        }
        break;

    case LIBERA_EVENT_GET_FIFOLEN_MC:
        /* NOTE: Although usually a GET method would only require
         *       copying to userland we need to read the FIFO No.
         *       first in order to get the index.
         */
        {
            libera_U32_t fifo, val;
            ret = copy_from_user(&fifo,
                                 (libera_U32_t *)arg,
                                 sizeof(libera_U32_t));
            if (ret) return ret;
            if (fifo >= TRIG_EVENTS_MAX)
                return -EINVAL;
            
            val = lenFIFO(&dev->mc_timestamps[fifo]);
            ret = copy_to_user((libera_U32_t *)arg,
                               &val,
                               sizeof(libera_U32_t));
            PDEBUG3("GET_FIFOLEN_MC: fifo=%u, val=%u\n", fifo, val);
        }
        break;

DEBUG_ONLY(
    case LIBERA_EVENT_GET_TRIGGER_BLOCKED:
	{
	    libera_Ltimestamp_t trigger;
	    ret = getBlockedFromFIFO(&dev->sc_timestamps[T_TRIG_TRIGGER],
				     &dev->SC_queue,
				     &trigger.lst);
	    ret = getBlockedFromFIFO(&dev->mc_timestamps[T_TRIG_TRIGGER],
				     &dev->MC_queue,
				     &trigger.lmt); 
	    ret = copy_to_user((libera_Ltimestamp_t *)arg,
			       &trigger, sizeof(libera_Ltimestamp_t));
	}
	break;
);

	/* Enable/disable event propagation to event-fifo */
    case LIBERA_EVENT_GET_MASK:
	ret = copy_to_user((libera_U32_t *)arg,
			   &dev->global->event_mask,
			   sizeof(libera_U32_t));
	break;

    case LIBERA_EVENT_SET_MASK:
	/* File descriptor access control - Must be a WRITER */
      if (dev->master_filp != file) {
	    return -EPERM;
      }
      ret = copy_from_user(&dev->global->event_mask,
			   (libera_U32_t *)arg, 
			   sizeof(libera_U32_t));
	break;

    case LIBERA_EVENT_FLUSH:
	/* File descriptor access control - Must be a WRITER */
	if (dev->master_filp != file)
	    return -EPERM;

	flush_eventFIFO(&dev->events);
	break;

    case LIBERA_EVENT_ACQ_PM:
	ret = libera_acq_pm();
      	break;

    case FIONREAD:
	return put_user(len_eventFIFO(&dev->events)*sizeof(libera_event_t), 
			(int *)arg);
	break;
 
DEBUG_ONLY(
    case LIBERA_EVENT_EVENT_SIM:
    {
	libera_Ltimestamp_t ctime;
	
        PDEBUG3("Simulating LIBERA_NM_TRIGGER event.\n");
	/* Get CTIME first */
	ret = libera_get_CTIME(&ctime);
	if (ret) return ret;
	/* Fill in the fake trigger */
	putLSTtoCircBuf(&dev->paired_timestamps[T_TRIG_TRIGGER], &ctime.lst);
	putLMTtoCircBuf(&dev->paired_timestamps[T_TRIG_TRIGGER], &ctime.lmt);
	/* Notify userland about TRIGGER trigger event  */
	if ( cfg->param[LIBERA_CFG_TRIGMODE] == LIBERA_TRIGMODE_GET )
	    libera_send_event(LIBERA_EVENT_TRIGGET, 0);
	if ( cfg->param[LIBERA_CFG_TRIGMODE] == LIBERA_TRIGMODE_SET )
	    libera_send_event(LIBERA_EVENT_TRIGSET, 0);
    }
    break;
);

DEBUG_ONLY(
    case LIBERA_EVENT_SET_OFFPLL:
        ret = copy_from_user(&dev->settime.lmt.off_pll,
			     (libera_hw_time_t *)arg,
			     sizeof(libera_hw_time_t));
	PDEBUG2("New PLL LMT offset = 0x%08lx%08lx\n",
		ULL(dev->settime.lmt.off_pll));
	break;
);

DEBUG_ONLY(
    case LIBERA_EVENT_PEEK:
    {
	libera_peek_poke_t peek;
	ret = copy_from_user(&peek, (libera_peek_poke_t *)arg, sizeof(peek));
	peek.value = readl(iobase + peek.offset);
	ret = copy_to_user((libera_peek_poke_t *)arg, &peek, sizeof(peek));
    }
    break;

    case LIBERA_EVENT_POKE:
    {
	libera_peek_poke_t poke;
	ret = copy_from_user(&poke, (libera_peek_poke_t *)arg, sizeof(poke));
	writel(poke.value, iobase + poke.offset);
    }
    break;
    );
 
    default:
	/* NOTE: Returning -EINVAL for invalid argument would make more sense, 
	 *       but acoording to POSIX -ENOTTY should be returned in 
	 *       such cases.
	 */
	PDEBUG("EVENT: Invalid ioctl() argument (file: %s, line: %d)\n", 
	       __FILE__, __LINE__);
	return -ENOTTY;

    } /* switch(cmd) */
    return ret;
}


/** Libera Event Device: Called on ioctl() 
 *
 * Takes care of sanity checking of the ioctl argument and of access control.
 * For details see: Alessandro Rubini et al., Linux Device Drivers, pp. 128-135.
 */
static int
libera_event_ioctl(struct inode *inode, struct file *file,
		 unsigned int cmd, unsigned long arg)
{
    struct libera_event_device *dev =
	(struct libera_event_device *) file->private_data;
    int err = 0;

    /* Sanity checks on cmd */
    if ( (_IOC_TYPE(cmd) != LIBERA_EVENT_MAGIC) &&
	 (cmd != FIONREAD) ) 
	return -ENOTTY;

    if (_IOC_DIR(cmd) & _IOC_READ)
        err = !access_ok(VERIFY_WRITE, (void *)arg, _IOC_SIZE(cmd));
    else if (_IOC_DIR(cmd) & _IOC_WRITE)
        err =  !access_ok(VERIFY_READ, (void *)arg, _IOC_SIZE(cmd));
    if (err) {
	ASSERT(TRUE);
	return -EFAULT;
    }
    
#ifdef EVENT_SINGLE_WRITER
    /* Access control:
     * Only one application at a time is granted permission to change 
     * the parameters (SET_ methods). The access to the device should 
     * not be limited for reading (GET_ methods).
     */
    if (_IOC_DIR(cmd) == _IOC_WRITE) /* Is this a SET_ method? */
    {
	/* File descriptor access control */
	if (dev->master_filp != file)
	    return -EPERM;
    }
#endif

    /* Decode comands and take action on hardware */
    return libera_event_cmd(dev, file, cmd, arg);
}


/** Libera EVENT Device: Called on read()
 *
 * Reads data from event fifo and blocks if necessary.
 */
static ssize_t 
libera_event_read(struct file *file, char *buf, size_t count, loff_t *f_pos)
{
    struct libera_event_device *dev =
    	(struct libera_event_device *) file->private_data;
    wait_queue_head_t* wq = &dev->EVENT_queue;
    struct libera_event_fifo* q = &dev->events;
    wait_queue_t wait;
    libera_event_t *data;
    size_t event_count, i;
    ssize_t ret = 0;

    /* Get the device semaphore */
    if (mutex_lock_interruptible(&dev->sem))
	return -ERESTARTSYS;

    /* Is it a non-blocking read() ? */
    if ( (file->f_flags & O_NONBLOCK) &&
	 empty_eventFIFO(&dev->events) ) {
	ret = -EAGAIN;
	goto out;
    }

    /* Sanity check regarding count */
    if(!count) goto out;

    /* Filter out strange, non-atomically-dividable values. */
    if (count % sizeof(libera_event_t)) {
	PDEBUG("EVENT: read(): Inapropriate count size.\n");
    	ret = -EINVAL;
	goto out;
    }
			      

    /* Check fifo size & wait/sleep if neccessary */
    if (empty_eventFIFO(q))
    {
	init_waitqueue_entry(&wait,current);
	add_wait_queue(wq,&wait);
	for (;;)
	{
	    set_current_state(TASK_INTERRUPTIBLE);
	    if (not_empty_eventFIFO(q)) {
		break;
	    }
	    if (!signal_pending(current)) {
		mutex_unlock(&dev->sem);
		schedule();
		mutex_lock(&dev->sem);
		continue;
	    }
	    ret = -ERESTARTSYS;
	    break;
	}
	set_current_state(TASK_RUNNING);
	remove_wait_queue(wq, &wait);
    }

    if (ret) goto out;

    event_count = count/sizeof(libera_event_t);
    event_count = event_count < len_eventFIFO(&dev->events) ?
	event_count : len_eventFIFO(&dev->events);

    /* Get data from FIFO */
    for(i=0; i < event_count; i++) {
	if (! (data = getFrom_eventFIFO(q)) ) {
	    ret = -EFAULT;
	    goto out;
	}
	if (copy_to_user(buf, data, sizeof(libera_event_t))) {
	    ret = -EFAULT;
	    goto out;
	}
	else {
	    ret += sizeof(libera_event_t);
	    buf += sizeof(libera_event_t);
	}
    }
	
 out:
    mutex_unlock(&dev->sem);
    return ret;
}


/** Libera EVENT Device: Called on select() / poll()
 *
 * Checks whether event(s) are pending in the event fifo and sets the 
 * non-blocking operations mask acoordingly.
 */
unsigned int libera_event_poll(struct file *file, poll_table *wait)
{
    struct libera_event_device *dev = 
	(struct libera_event_device *)file->private_data;
    unsigned int mask = 0;
    
    poll_wait(file, &dev->EVENT_queue,  wait);
    if ( not_empty_eventFIFO(&dev->events) )
	mask |= POLLIN | POLLRDNORM;  /* readable */

    return mask;
}


/**  Libera Event Device file operations
 *
 * For details see: Alessandro Rubini et al., Linux Device Drivers, pp. 66.
 */
struct file_operations libera_event_fops = {
    owner:          THIS_MODULE,
    read:           libera_event_read,
    poll:           libera_event_poll,
    ioctl:          libera_event_ioctl,
    open:           libera_event_open,
    release:        libera_event_release,
};
