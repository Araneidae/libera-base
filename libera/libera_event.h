/* $Id: libera_event.h 2192 2008-10-07 09:13:06Z matejk $ */

//! \file libera_event.h
//! Libera GNU/Linux driver (A)synchronous Event device interface.

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
#include<asm/dma.h>

#ifndef _LIBERA_EVENT_H_
#define _LIBERA_EVENT_H_

/*
 FIFO libera_event miscellaneous functions
*/

static inline void flushFIFO(struct libera_fifo *q)
{
  q->put=q->get=0;
}

static inline int lenFIFO(struct libera_fifo *q)
{
  return ( (q->put + TRIG_LOG - q->get) & TRIG_LOG_MASK );
}
static inline int not_emptyFIFO(struct libera_fifo *q)
{
  return (q->put != q->get);
}
static inline int emptyFIFO(struct libera_fifo *q)
{
  return (q->put == q->get);
}

static inline int putToFIFO(struct libera_fifo *q, unsigned long long data)
{
  register int index;
  if((index=((q->put + 1) & TRIG_LOG_MASK)) == q->get)
    {
      return -1;  /* fifo would overflow */
    }
  else
    {
      q->stamp[index] = data;
      q->put = index;
      return 0;
    }
}

static inline unsigned long long* getFromFIFO(struct libera_fifo* const q)
{
  if(q->put == q->get)
    {
      return NULL; /* fifo empty */
    }
  else
    {
      q->get = (q->get + 1) & TRIG_LOG_MASK;
      return & q->stamp[q->get];
    }
}


/******************************/
/* Paired Time CircBuf Queues */
/******************************/
static inline void flushCircBuf(struct libera_circbuf* const q)
{
    memset(q, 0, sizeof(struct libera_circbuf));
}

/* LST should always be put to CircBuf first! -> Check interrupt priority! */
static inline void putLSTtoCircBuf(struct libera_circbuf* const q, 
				   const libera_hw_time_t *lst)
{
    q->stamp[q->put].lst = *lst;    
}

/* LMT should always be put to CircBuf last! -> Check interrupt priority! */
static inline void putLMTtoCircBuf(struct libera_circbuf* const q,
				   const libera_hw_time_t *lmt)
{
    q->stamp[q->put].lmt = *lmt;
    q->put = ((q->put + 1) & TRIG_LOG_MASK);
}

static inline void putToCircBuf(struct libera_circbuf* const q,
				const libera_Ltimestamp_t *data)
{
    q->stamp[q->put] = *data;
    q->put = ((q->put + 1) & TRIG_LOG_MASK);
}

static inline libera_Ltimestamp_t*
getFromCircBuf(struct libera_circbuf* const q)
{ 
    register int index;
    index = ((q->put - 1) & TRIG_LOG_MASK);
    return & q->stamp[index];
}


/* DMA fifos */
static inline void flushDMA_FIFO(libera_dma_t *q)
{
    q->put=q->get=0;
}

static inline int lenDMA_FIFO(libera_dma_t *q)
{
    int ret;

    spin_lock(&dma_spin_lock);
    ret = ( (q->put + LIBERA_DMA_FIFO_ATOMS - q->get) & LIBERA_DMA_FIFO_MASK );
    spin_unlock(&dma_spin_lock);

    return ret;
}
static inline int tailDMA_FIFO(libera_dma_t *q)
{
    int ret;

    spin_lock(&dma_spin_lock);
    ret = ( LIBERA_DMA_FIFO_ATOMS - q->put );
    spin_unlock(&dma_spin_lock);

    return ret;
}
static inline int not_emptyDMA_FIFO(libera_dma_t *q)
{
    int ret;
    
    spin_lock(&dma_spin_lock);
    ret = (q->put != q->get);
    spin_unlock(&dma_spin_lock);

    return ret;
}
static inline int emptyDMA_FIFO(libera_dma_t *q)
{
    int ret;

    spin_lock(&dma_spin_lock);
    ret = (q->put == q->get);
    spin_unlock(&dma_spin_lock);

    return ret;
}

static inline int putToDMA_FIFO(libera_dma_t *q, unsigned int size_atoms)
{
    register int index;
    if((index=((q->put + size_atoms) & LIBERA_DMA_FIFO_MASK)) == q->get)
	{
	    return -1;  /* fifo would overflow */
	}
    else
	{
	    /* Only change the index as DMA copies the data */
	    q->put = index;
	    return 0;
	}
}




/*
 FIFO libera_event miscellaneous functions
*/

static inline void flush_eventFIFO(struct libera_event_fifo *q)
{
  q->put=q->get=0;
}

static inline int len_eventFIFO(struct libera_event_fifo *q)
{
  return ( (q->put + TRIG_LOG - q->get) & TRIG_LOG_MASK );
}
static inline int not_empty_eventFIFO(struct libera_event_fifo *q)
{
  return (q->put != q->get);
}
static inline int empty_eventFIFO(struct libera_event_fifo *q)
{
  return (q->put == q->get);
}

static inline int putTo_eventFIFO(struct libera_event_fifo *q, libera_event_t *data)
{
  register int index;
  if((index=((q->put + 1) & TRIG_LOG_MASK)) == q->get)
    {
      return -1;  /* fifo would overflow */
    }
  else
    {
      q->data[index] = *data;
      q->put = index;
      return 0;
    }
}

static inline libera_event_t* getFrom_eventFIFO(struct libera_event_fifo* const q)
{
  if(q->put == q->get)
    {
      return NULL; /* fifo empty */
    }
  else
    {
      q->get = (q->get + 1) & TRIG_LOG_MASK;
      return & q->data[q->get];
    }
}

#define T_FIFO_SIZE(X) (X & 0x1FF)
#define TRIG_VECTOR TRIG_ALL_MASK /* 9 bit TRIGGER vector */
#define TRIG_ZERO 0x1 << 22
#define SELF_INC_TRIGGER 0x200 << 22 /* free running trigger (9) */

#endif /* _LIBERA_EVENT_H_ */
