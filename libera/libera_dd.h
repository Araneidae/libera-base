/* $Id: libera_dd.h 2233 2008-10-20 12:14:46Z matejk $ */

//! \file libera_dd.h
//! Libera GNU/Linux driver Data on Demand (DD) device interface.

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


#ifndef _LIBERA_DD_H_
#define _LIBERA_DD_H_

/** Convert System Time (ST) to Libera System Time (LST). */
static inline void
st2lst(const struct timespec *st, 
       libera_hw_time_t *lst)
{
    /* NOTE: This conversion is safe regarding overflow in nsec part
     *       and up to 1.47e11 seconds from UNIX 0 (@ LSC = 125 MHz).
     */

    *lst = (libera_hw_time_t)st->tv_sec * LSC_FREQ +
      div_u64(((libera_hw_time_t)st->tv_nsec * LSC_FREQ), NS_IN_SECOND);

}


/** Convert Libera System Time (LST) to System Time (ST). */
static inline void
lst2st(const libera_hw_time_t *lst,
       struct timespec *st)
{
	uint32_t rem;
	st->tv_sec = div_u64_rem((*lst), LSC_FREQ, &rem);
	st->tv_nsec = rem;
	st->tv_nsec *= (NS_IN_SECOND / LSC_FREQ);
}


/** Convert Machine Time (MT) to Libera Machine Time (LMT). */
static inline void
mt2lmt(const libera_hw_time_t *mt,
       libera_hw_time_t *lmt)
{
    *lmt = (*mt) * lgbl.d;
}


/** Convert Libera Machine Time (LMT) to Machine Time (MT). */
static inline void
lmt2mt(const libera_hw_time_t *lmt,
       libera_hw_time_t *mt)
{
	*mt = div_u64((*lmt), lgbl.d);
}


/** LMT to Circular Buffer transformation
 *
 * Provides the Circular Buffer offset (position), based on LMT.
 */
static inline libera_U32_t
get_circ_offset_lmt(libera_hw_time_t *lmt)
{
    struct libera_event_device  *event  = &libera_event;

	uint64_t dummy_first_part, dummy_divisor;
	uint32_t return_value;

	dummy_first_part = (*lmt - (event->settime.lmt.off + event->HB_start_lmt) + lgbl.d/2);
	dummy_divisor = div_u64(dummy_first_part, lgbl.d);

	(void)div_u64_rem(dummy_divisor, LIBERA_DD_CIRCBUF_ATOMS, &return_value);
	
	return return_value;
}


/** MT to Circular Buffer transformation
 *
 * Provides the Circular Buffer offset (position), based on MT.
 */
static inline libera_U32_t
get_circ_offset_mt(libera_hw_time_t *mt)
{
    struct libera_event_device  *event  = &libera_event;
    
	uint64_t dummy_first_part, dummy_divisor ;
	uint32_t return_value;

	dummy_first_part =  (event->settime.lmt.off + event->HB_start_lmt);
	dummy_divisor = div_u64(dummy_first_part, lgbl.d);

	(void)div_u64_rem((*mt - dummy_divisor),LIBERA_DD_CIRCBUF_ATOMS, &return_value);

	return return_value;
};


/** Convert Libera System Time (LST) to Libera Machine Time (LMT).
 *  On success, 0 is returned.
 *  On error, meaningful negative errno is returned.
 */
static inline int
lst2lmt(const libera_hw_time_t *lst,
        libera_hw_time_t *lmt,
        const libera_Ltimestamp_t *stime)
{
  libera_hw_time_t deltaLST;
  libera_hw_time_t deltaLMT;
  int positive;

  /* This is to avoid using signed 64-bit type for division
   * (unresolved symbol __divdi3)
   */
  if (*lst > stime->lst)
    {
      deltaLST = *lst - stime->lst;
      positive = TRUE;
    }
  else
    {
      deltaLST = stime->lst - *lst;
      positive = FALSE;
    }

  /* NOTE: 64-bit unsigned data type used for deltaLMT allows for
   *       correct transformation for times up to cca. 15.7e9 LST
   *       away from the stime sync point in the worst case
   *       scenario (LMC = f_lmc = 117.440 MHz -> Diamond storage).
   *       Above these times, we will get overflow problems.
   *       This roughly corresponds to 124 seconds (@ LSC = 125 MHz).
   *       Be sure to adjust DELTA_LST_OVERFLOW according to the
   *       worst case LMC.
   */
  if (deltaLST >  DELTA_LST_OVERFLOW)
    {
      ASSERT(TRUE);
      return -ERANGE;
    }
 
  deltaLMT = div_u64((deltaLST * flmcdHz),(LSC_FREQ * FLMC_DECI_HZ));


  if (positive)
    *lmt = stime->lmt + deltaLMT;
  else
    *lmt = stime->lmt - deltaLMT;

  return 0;
}


/** Convert Libera Machine Time (LMT) to Libera System Time (LST).
 *  On success, 0 is returned.
 *  On error, meaningful negative errno is returned.
 */
static inline int
lmt2lst(const libera_hw_time_t *lmt,
        libera_hw_time_t *lst,
        const libera_Ltimestamp_t *stime)
{
  libera_hw_time_t deltaLST;
  libera_hw_time_t deltaLMT;
  int positive;

  /* This is to avoid using signed 64-bit type for division
   * (unresolved symbol __divdi3)
   */
  if (*lmt > stime->lmt)
    {
      deltaLMT = *lmt - stime->lmt;
      positive = TRUE;
    }
  else
    {
      deltaLMT = stime->lmt - *lmt;
      positive = FALSE;
    }
  /* NOTE: 64-bit unsigned data type used for deltaLST allows for
   *       correct transformation for times up to cca. 14.7e9 LMT
   *       away from the stime sync point (@ LSC = 125 MHz).
   *       Above these times, we will get overflow problems.
   *       This roughly corresponds to 124 seconds in the worst case
   *       scenario (@ LMC = f_lmc = 117.440 MHz -> Diamond storage).
   *       Be sure to adjust DELTA_LMT_OVERFLOW according to LSC.
   */
  if (deltaLMT >  DELTA_LMT_OVERFLOW)
    {
	    PDEBUG2("deltaLMT has value 0x%08lx%08lx in function %s \n",ULL(deltaLMT),__FUNCTION__);
      ASSERT(TRUE);
      return -ERANGE;
    }

  deltaLST = div_u64((deltaLMT * LSC_FREQ * FLMC_DECI_HZ), flmcdHz);
  
  if (positive)
    *lst = stime->lst + deltaLST;
  else
    *lst = stime->lst - deltaLST;
  return 0;
}

#endif /* _LIBERA_DD_H_ */
