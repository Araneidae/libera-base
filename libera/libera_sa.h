/* $Id: libera_sa.h 2192 2008-10-07 09:13:06Z matejk $ */

//! \file libera_sa.h
//! Libera GNU/Linux driver Slow Acquisition (SA) device interface.

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


#ifndef _LIBERA_SA_H_
#define _LIBERA_SA_H_

/*
 FIFO libera_sa_sa miscellaneous functions
*/

static inline void flush_saFIFO(struct libera_sa_fifo *q)
{
	q->put=q->get=0;
}

static inline int len_saFIFO(struct libera_sa_fifo *q)
{
	return ( (q->put + SA_LOG - q->get) & SA_LOG_MASK );
}

static inline int not_empty_saFIFO(struct libera_sa_fifo *q)
{
	return (q->put != q->get);
}

static inline int empty_saFIFO(struct libera_sa_fifo *q)
{
	return (q->put == q->get);
}

static inline int free_saFIFO(struct libera_sa_fifo *q)
{
	int len;
	len = len_saFIFO(q);
	return ( SA_LOG-len); 
}

static inline int putTo_saFIFO(struct libera_sa_fifo *q, libera_atom_sa_t *data)
{
  register int index;
  if((index=((q->put + 1) & SA_LOG_MASK)) == q->get)
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

static inline libera_atom_sa_t* getFrom_saFIFO(struct libera_sa_fifo* const q)
{
  if(q->put == q->get)
    {
      return NULL; /* fifo empty */
    }
  else
    {
      q->get = (q->get + 1) & SA_LOG_MASK;
      return & q->data[q->get];
    }
}

static inline  ssize_t libera_pipe_write (struct sa_local *sa_pipe, const char *buf, size_t count)
{
	ssize_t ret, written=0;
	size_t sa_count,i;


	/* Null write succeeds */
	ret=0;
	if (0==count) goto out;

	/* Filter out non-divisable count values */
	if (count%sizeof(libera_atom_sa_t)){
	
		PDEBUG("SA:libera_pipe_write(): Inappropriate count size. \n");
		ret=-EINVAL;
		goto out;
	}

	
	sa_count = count/sizeof(libera_atom_sa_t);


	/* check if there is enough free space in pipe */

	if (free_saFIFO(&sa_pipe->pipe) < sa_count) {
		ret=-EFAULT;
		goto out;
	}
	

	/* write must be atomic  */
	for(i=0;i<sa_count;i++){

		libera_atom_sa_t data;

		memcpy(&data,buf,sizeof(libera_atom_sa_t));

		if (putTo_saFIFO(&sa_pipe->pipe,&data)) {
			PDEBUG("SA:putTo_saFIFO: fifo would owerflow\n");
			ret=written;
			goto out;
		}
		
		written += sizeof(libera_atom_sa_t);	
		buf +=sizeof(libera_atom_sa_t);
		ret=written;
	}	

out:
	return ret;

}
#endif /* _LIBERA_SA_H_ */
