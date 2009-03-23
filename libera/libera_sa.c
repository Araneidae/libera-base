/* $Id: libera_sa.c 2233 2008-10-20 12:14:46Z matejk $ */

//! \file libera_sa.c
//! Implements Libera GNU/Linux driver Slow Acquisition (SA) device (libera.sa).

// This file is indented with tabs set to 4 characters. Please set your editor accordingly.

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

#include "libera_kernel.h"
#include "libera_sa.h"
#include "libera.h"


/** Registers a listener pipe.
 *
 * Registers a pipe and declares a listener/reader on SA device. 
 * NOTE: This is necessary in order for libera interrupt to know which
 *       SA fifo(s) to write data to.
 */
static int 
libera_register_pipe(struct libera_registered_pipes *pipe, 
		     struct sa_local *sa_fifo)
{
	int i;
	int occupied = 1;
	int next = pipe->next;
    
	pipe->sa_pipe[next] = sa_fifo; /* Store and mark pipe occupied */
	pipe->registered++;
	PDEBUG2("Registered pipe #%d on sa_pipe %p.\n", next, sa_fifo);

	/* Find new non-occupied = free pipe */
	for(i=0;i<LIBERA_SA_MAX_READERS;i++)
	{
		if (!pipe->sa_pipe[i]) {
		
			pipe->next = i;
			occupied = 1;
			break;
		}
    }

	if(occupied)
	return -1;
	else return 0;
}     


/** Unregisters a listener pipe.
 *
 * Unregisters a pipe and withdraws a listener/reader on SA device. 
 * NOTE: This is necessary in order for libera interrupt to know which
 *       fifo to write data to.
 */
static int 
libera_unregister_pipe(struct libera_registered_pipes *pipe,
		       struct sa_local *sa_fifo)
{
	int i;
	int found = 0;
    
	for(i=0;i<LIBERA_SA_MAX_READERS;i++)
    {
		if (pipe->sa_pipe[i] == sa_fifo) {
			found = 1;
			break;
		}
	}

	if (found) {
		pipe->registered--;
		pipe->sa_pipe[i] = NULL; /* Mark pipe non-occupied = free */
		pipe->next = i; /* Reuse */
		PDEBUG2("Unregistered pipe #%d on sa_pipe %p.\n", i, sa_fifo);
		return 0;
    }
	else return -1;
}     


/** Libera SA Device: Called on open() 
 *
 * Takes care of proper opening of the SA device and updates the information
 * for access control. Every successful open() creates and registers one 
 * SA pipe as well.
 *
 * For details see: Alessandro Rubini et al., Linux Device Drivers, pp. 64.
 */
static int 
libera_sa_open(struct inode *inode, struct file *file)
{
	struct libera_sa_device *dev =
			(struct libera_sa_device *) file->private_data;
	struct sa_local *pipe_sa=NULL; 

	if (!dev) return -ENODEV;

	mutex_lock(&dev->sem);

	/* We do not allow writing */
	if (file->f_mode & FMODE_WRITE) {
		mutex_unlock(&dev->sem);
		return -EACCES;
	// TODO: Consider returning Operation not permitted (-EPERM)
    }

    if (dev->pipe.registered < LIBERA_SA_MAX_READERS) {
    
		/* Get a fresh memory space for a new pipe */ 
		pipe_sa = (struct sa_local *)
				kmalloc(sizeof(struct sa_local), GFP_KERNEL);

		if (pipe_sa) {

			/* No junk in pipe_inode! */
			memset(pipe_sa, 0, sizeof(struct sa_local)); 
			mutex_init(&pipe_sa->sem);
			init_waitqueue_head(&pipe_sa->wait);
			flush_saFIFO(&pipe_sa->pipe); 
		}
		else {
			mutex_unlock(&dev->sem);
			return -ENOMEM;
		}
	
		/* Store current pipe_inode for our reference */
		file->f_version = (unsigned long)pipe_sa;
	    
		libera_register_pipe(&dev->pipe, pipe_sa);
    }
    else {
		mutex_unlock(&dev->sem);
		return -EBUSY;
	}
	
	
    /* Is it the first open() for reading */
    if (file->f_mode & FMODE_READ) {

		if (!dev->master_filp) {
			dev->master_filp = file;
		}
	}

	dev->readers++;

	PDEBUG2("Opened libera.sa (%p): readers=%d, writers=%d\n", 
		file, dev->readers, dev->writers);

	dev->open_count++; /* internal counter */
	mutex_unlock(&dev->sem);
    
	return 0;          
}


/** Libera SA Device: Called on close() 
 *
 * Takes care of proper closing of the SA device and updates the informaton
 * for access control. Every successful close() unregisters and 
 * destroys/releases one SA pipe as well.
 *
 * For details see: Alessandro Rubini et al., Linux Device Drivers, pp. 64.
 */
static int 
libera_sa_release(struct inode *inode, struct file *file)
{
	struct libera_sa_device *dev =
		(struct libera_sa_device *) file->private_data;
	struct sa_local  *pipe_sa = (struct sa_local *)file->f_version; 

    
	mutex_lock(&dev->sem);	
    
	if (libera_unregister_pipe(&dev->pipe, pipe_sa) < 0) {
		PDEBUG("Cannot unregister pipe, pipe_sa address %p\n",pipe_sa);
		goto out;
    }
	
	kfree(pipe_sa);
	file->f_version = (unsigned long)NULL;
   

out:
	dev->open_count--; /* internal counter */
	dev->readers--;
    
	/* Check whether the last reader is closing */
	if (!dev->readers) {
		dev->master_filp = NULL;
	}
  
	mutex_unlock(&dev->sem);

	PDEBUG2("Closed libera.sa device: readers=%d, writers=%d\n", 
		dev->readers, dev->writers);

	return 0;
}


/** Libera SA Device: Called on read()
 *
 * Reads data from sa fifo and blocks if necessary.
 * Takes care of retrieving SA samples from the SA registered fifo and 
 * passes it to suserland. Userland read() will block until 
 * SA fifo size =/= 0.
 */
static ssize_t 
libera_sa_read(struct file *file, char *buf, size_t count, loff_t *f_pos)
{
	struct sa_local *sa_pipe = (struct sa_local *) file->f_version;
	ssize_t ret;
	wait_queue_t wait;
	wait_queue_head_t* wq = &sa_pipe->wait;
	size_t sa_count, i;
	libera_atom_sa_t *data;
   
	ret = 0; 

    /* Seeks are not allowed on pipes */
	if (*f_pos != file->f_pos) {

		ret = -ESPIPE;
		goto out_nolock;  

	}

    /* Always return 0 on null read */
	if (0 == count) {
		ret = 0;
		goto out_nolock;
	}
    
    /* Get the device semaphore */
	if (mutex_lock_interruptible(&sa_pipe->sem)){

		ret = -ERESTARTSYS;
		goto out_nolock;
	}

    
    /* Filter out strange, non-atomically-dividable values. */
	if (count % sizeof(libera_atom_sa_t)) {
		PDEBUG("SA: read(): Inapropriate count size.\n");
		ret = -EINVAL;
		goto out;
    } 
			      

    /* Check fifo size & wait/sleep if neccessary */
	if (empty_saFIFO(&sa_pipe->pipe)) {
    
		if (file->f_flags & O_NONBLOCK) {
			ret=-EAGAIN;
			goto out;
		}
		init_waitqueue_entry(&wait,current);
		add_wait_queue(wq,&wait);
		for (;;)
		{
			set_current_state(TASK_INTERRUPTIBLE);
			if (not_empty_saFIFO(&sa_pipe->pipe)) {
				break;
			}
			if (!signal_pending(current)) {
				mutex_unlock(&sa_pipe->sem);
				schedule();
				mutex_lock(&sa_pipe->sem);
				continue;
			}
			ret = -ERESTARTSYS;
			break;
		}
		set_current_state(TASK_RUNNING);
		remove_wait_queue(wq, &wait);
	}

	if (ret) goto out;

	sa_count = count/sizeof(libera_atom_sa_t);
	sa_count = sa_count < len_saFIFO(&sa_pipe->pipe) ?
				sa_count : len_saFIFO(&sa_pipe->pipe);  

	/* Get data from FIFO */
	for(i=0; i < sa_count; i++) {
		if (! (data = getFrom_saFIFO(&sa_pipe->pipe)) ) {
			ret = -EFAULT;
			goto out;
		}
		if (copy_to_user(buf, data, sizeof(libera_atom_sa_t))) {
		 	ret = -EFAULT;
		 	goto out;
		}
		else {
			ret += sizeof(libera_atom_sa_t);
			buf += sizeof(libera_atom_sa_t);
		}  
	}
	
out:
	mutex_unlock(&sa_pipe->sem);
    
out_nolock:   
	return ret;

}


/**  Libera SA Device file operations
 *
 * For details see: Alessandro Rubini et al., Linux Device Drivers, pp. 66.
 */
struct file_operations libera_sa_fops = {
	owner:          THIS_MODULE,
	llseek:	        libera_llseek,      /* not specific, use default */
	read:           libera_sa_read,
	write:          libera_write,       /* not specific, use default */
	ioctl:          libera_ioctl,       /* not specific, use default */
	open:           libera_sa_open,
	release:        libera_sa_release
};
