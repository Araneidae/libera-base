/* $Id: ebpp.c 2361 2008-12-12 12:26:13Z tomazb $ */

/** \file ebpp.c */
/** Implements Libera Electron Beam Position Processor (EBPP) specifics. */

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
#include "libera_event.h"
#include "libera_dd.h"
#include "libera_sa.h"
#include "libera.h"

/* max duration for ILK and PM */
#define MAXDUR 4095

extern int
getBlockedFromFIFO(struct libera_fifo* const q,
		   wait_queue_head_t* const wq,
		   libera_hw_time_t *time);

extern void 
libera_dma_get_DMAC_csize(libera_dma_t *dma);

extern int 
libera_sctrig_enable(struct libera_event_device * dev,
		     const unsigned long mask);
extern int 
libera_mctrig_enable(struct libera_event_device * dev,
		     const unsigned long mask);


/** Global Libera description */
libera_desc_t libera_desc = {
    magic: LIBERA_MAGIC,
    name: "EBPP",
    fpga: "Unknown",
};


/** Helper function for writing to the DD Command Buffer (CB) FIFO.
 *
 * Constructs a DD command and writes it to Command Buffer FIFO.
 * On success, 0 is returned.
 * On error, meaningful negative errno is returned.
 */
static ssize_t
libera_dd_write_CBfifo(size_t cb_StartAddr, 
		       size_t cb_NoAtoms,
		       unsigned long decimation)
{
    ssize_t ret = 0;
    unsigned long cb_FiltDec;
    unsigned long cbFIFO_H;
    unsigned long cbFIFO_L;


    /* First prepare/construct command for CB_FIFO */
    cb_FiltDec = (decimation > 1) ? DD_CBH_DEC_64 : DD_CBH_DEC_1;

    PDEBUG2("cb_StartAddr = 0x%x\n", cb_StartAddr);
    PDEBUG2("cb_NoAtoms = 0x%x\n", cb_NoAtoms);

    cb_NoAtoms = cb_NoAtoms * decimation;
    if (cb_NoAtoms > DD_MAX_NOATOMS)
    {
        PDEBUG("DD: (cbFIFO cmd): Maximum No. of atoms exceeded (%lu>%d).\n",
	       (unsigned long)cb_NoAtoms, DD_MAX_NOATOMS);
	return -EINVAL;
    }

    cbFIFO_H = 
	( (cb_FiltDec & ((1 << DD_CBH_FDEC_WIDTH)-1)) << DD_CBH_FDEC_LSB ) |
	(cb_StartAddr & ((1 << DD_CBH_SADDR_WIDTH)-1));
    cbFIFO_L = 
	( (cb_NoAtoms & ((1 << DD_CBL_NOAT_WIDTH)-1)) << DD_CBL_NOAT_LSB );
    
    PDEBUG3("cbFIFO = 0x%08lx%08lx\n", 
	    (unsigned long)cbFIFO_H, 
	    (unsigned long)cbFIFO_L);

    /* Write to  CB FIFO */
    writel(cbFIFO_H, iobase + DD_CB_FIFO_H);
    writel(cbFIFO_L, iobase + DD_CB_FIFO_L);

    return ret;
}

unsigned long scale_kadc(unsigned int limit)
{
    unsigned long long overflow_limit = (limit & 0x0000ffff);
    overflow_limit *= (kadc*kadc);
    // overflow_limit /= (unsigned long long)1e6;
    overflow_limit = div_u64(overflow_limit, (unsigned long) 1e6);
    return (unsigned long)overflow_limit;
}

/** Libera Configuration Device: Specific GET methods.
 *
 * Takes care of CFG GET actions on EBPP Libera family member specific
 * parameters.
 */
int 
libera_cfg_get_specific(struct libera_cfg_device *dev,
                        libera_cfg_request_t *req)
{
  int ret = 0;

  switch (req->idx) {
    case LIBERA_CFG_XOFFSET:
    case LIBERA_CFG_YOFFSET:
    case LIBERA_CFG_QOFFSET:
    case LIBERA_CFG_KX:
    case LIBERA_CFG_KY:
    case LIBERA_CFG_ILK_XLOW:
    case LIBERA_CFG_ILK_XHIGH:
    case LIBERA_CFG_ILK_YLOW:
    case LIBERA_CFG_ILK_YHIGH:
    case LIBERA_CFG_ILK_MODE:
    case LIBERA_CFG_ILK_OVERFLOW_LIMIT:
    case LIBERA_CFG_ILK_OVERFLOW_DUR:
    case LIBERA_CFG_ILK_GAIN_LIMIT:
    case LIBERA_CFG_ILKSTATUS:
    case LIBERA_CFG_PMOFFSET:
    case LIBERA_CFG_EXTSWITCH:
    case LIBERA_CFG_SWDELAY:
    case LIBERA_CFG_TRIGDELAY:
    case LIBERA_CFG_BCD_XOFFSET:
    case LIBERA_CFG_BCD_YOFFSET:
    case LIBERA_CFG_PM_XLOW:
    case LIBERA_CFG_PM_XHIGH:
    case LIBERA_CFG_PM_YLOW:
    case LIBERA_CFG_PM_YHIGH:
    case LIBERA_CFG_PM_MODE:
    case LIBERA_CFG_PM_OVERFLOW_LIMIT:
    case LIBERA_CFG_PM_OVERFLOW_DUR:
    case LIBERA_CFG_SR_ENABLE:
    case LIBERA_CFG_SR_CSPI_ENABLE:
    case LIBERA_CFG_SR_AVERAGING_STOP:
    case LIBERA_CFG_SR_AVERAGE_WINDOW:
    case LIBERA_CFG_SR_START:
    case LIBERA_CFG_SR_WINDOW:
    case LIBERA_CFG_SP_THRESHOLD:
    case LIBERA_CFG_SP_N_BEFORE:
    case LIBERA_CFG_SP_N_AFTER:
      req->val = dev->param[req->idx];
      break;
    case LIBERA_CFG_DEC_DDC:
      req->val = dev->global->d;
      break;
    case LIBERA_CFG_DFA:
      req->val = dev->global->dfa;
      break;
    case LIBERA_CFG_DDC_MAFLENGTH:
      req->val = readl(iobase + DDC_MAF_LENGTH);
      break;
    case LIBERA_CFG_DDC_MAFDELAY:
      req->val = readl(iobase + DDC_MAF_DELAY);
      break;
    case LIBERA_CFG_MAX_ADC:
      req->val = readl(iobase + ADC_MAX)&0xFFFF;
      break;
    case LIBERA_CFG_AVERAGE_SUM:
    {
      unsigned long denominator = readl(iobase + FA_NSUMS);
      unsigned long long dividend1, dividend2;

      if (denominator>0)
      {
        dividend1 = readl(iobase + FA_SUMSUMS_LSW);
        dividend2 = readl(iobase + FA_SUMSUMS_MSW);
        req->val = div_u64(dividend1+(dividend2<<32), denominator);
      }
      else
      {
        req->val = 0;
      }
      break;
    }
    default:
      PDEBUG("CFG: Invalid GET request index %d (file: %s, line: %d)\n", 
             req->idx, __FILE__, __LINE__);
      return -EINVAL;

  } // switch (req->idx)

  return ret;
}


/** Libera Configuration Device: Specific SET methods.
 *
 * Takes care of CFG SET actions on EBPP Libera family member specific
 * parameters.
 */
int 
libera_cfg_set_specific(struct libera_cfg_device *dev,
                        libera_cfg_request_t *req)
{
  int           ret = 0;
  unsigned long val;

  switch (req->idx) {
    case LIBERA_CFG_XOFFSET:
      dev->param[req->idx] = req->val;
      writel(req->val + dev->param[LIBERA_CFG_BCD_XOFFSET], 
             iobase + FA_OFFSET);
      break;
    case LIBERA_CFG_YOFFSET:
      dev->param[req->idx] = req->val;
      writel(req->val + dev->param[LIBERA_CFG_BCD_YOFFSET], 
             iobase + FA_OFFSET + sizeof(unsigned long));
      break;

    case LIBERA_CFG_QOFFSET:
    case LIBERA_CFG_KX:
    case LIBERA_CFG_KY:
      dev->param[req->idx] = req->val;
      writel(dev->param[req->idx], 
             iobase + FA_OFFSET + 
             (req->idx - LIBERA_CFG_CUSTOM_FIRST)*sizeof(unsigned long));
      break;

      /* NOTE: ILK atom is written @ LIBERA_CFG_ILK_GAIN_LIMIT and
       *       PM atom is written @ LIBERA_CFG_PM_OVERFLOW_DUR atom member.
       *       All the others are mirrored until then.
       */
    case LIBERA_CFG_ILK_XLOW:
    case LIBERA_CFG_ILK_XHIGH:
    case LIBERA_CFG_ILK_YLOW:
    case LIBERA_CFG_ILK_YHIGH:
    case LIBERA_CFG_ILK_MODE:
    case LIBERA_CFG_PM_MODE:
    case LIBERA_CFG_PM_XLOW:
    case LIBERA_CFG_PM_XHIGH:
    case LIBERA_CFG_PM_YLOW:
    case LIBERA_CFG_PM_YHIGH:
      dev->param[req->idx] = req->val;
      break;

    case LIBERA_CFG_PM_OVERFLOW_LIMIT:
    case LIBERA_CFG_ILK_OVERFLOW_LIMIT:
      /* Check range */
    {
      unsigned long MAX = LIBERA_IS_BRILLIANCE(lgbl.feature) ? 32767 : 2047;
      unsigned long adc_count = req->val;
      if ( ( adc_count <= MAX ) ) {
        adc_count >>= LIBERA_IS_BRILLIANCE(lgbl.feature) ? 4 : 0;
        // NOTE: limit = sqr(acd_count)/256
        adc_count *= adc_count;
        dev->param[req->idx] = ( req->val << 16 ) | ( ( adc_count >> 8) & 0x0000ffff );
        PDEBUG("ADC^2 >> 8 = %lu(0x%08lx)\n",
               adc_count >> 8, adc_count >> 8);
      }
      else
        ret = -EINVAL;
    }
    break;


    case LIBERA_CFG_ILK_OVERFLOW_DUR:
      /* Check range */
      if ( ( req->val <= ilkdur ) &&
           ( req->val <= MAXDUR ) )
        dev->param[req->idx] = req->val;
      else
        ret = -EINVAL;
      break;

    case LIBERA_CFG_ILK_GAIN_LIMIT:
      dev->param[req->idx] = req->val;
      {
        unsigned int attn_sum;
        // Clip ACD count part and use only limit = adc^2 >> 8.
        unsigned long overflow_limit;
        overflow_limit = scale_kadc(dev->param[LIBERA_CFG_ILK_OVERFLOW_LIMIT]);
        // Clip dBm part and use only Sum(ATTN).
        attn_sum = dev->param[LIBERA_CFG_ILK_GAIN_LIMIT] & 0x0000ffff;

        PDEBUG("Interlock: %u %d %d %d %d %lu(0x%08x) %u %u(0x%08x)\n",
               dev->param[LIBERA_CFG_ILK_MODE],
               dev->param[LIBERA_CFG_ILK_XLOW],
               dev->param[LIBERA_CFG_ILK_XHIGH],
               dev->param[LIBERA_CFG_ILK_YLOW],
               dev->param[LIBERA_CFG_ILK_YHIGH],
               overflow_limit,
               dev->param[LIBERA_CFG_ILK_OVERFLOW_LIMIT],
               dev->param[LIBERA_CFG_ILK_OVERFLOW_DUR],
               attn_sum,
               dev->param[LIBERA_CFG_ILK_GAIN_LIMIT]);

        /* Pass to FPGA */        
        writel(dev->param[LIBERA_CFG_ILK_XLOW],  iobase + ILK_XLOW);
        writel(dev->param[LIBERA_CFG_ILK_XHIGH], iobase + ILK_XHIGH);
        writel(dev->param[LIBERA_CFG_ILK_YLOW],  iobase + ILK_YLOW);
        writel(dev->param[LIBERA_CFG_ILK_YHIGH], iobase + ILK_YHIGH);
        writel(attn_sum, iobase + DSC_ILK_GAIN_LIMIT);
        writel(overflow_limit,
               iobase + DSC_ILK_OVERFLOW_LIMIT);
        writel(dev->param[LIBERA_CFG_ILK_OVERFLOW_DUR],
               iobase + DSC_ILK_OVERFLOW_DUR);
        writel(dev->param[LIBERA_CFG_ILK_MODE], iobase + ILK_CONTROL);
      }
      break;

    case LIBERA_CFG_ILKSTATUS:
      PDEBUG("Reset Interlock status.\n");
      dev->param[req->idx] = 0;
      break;

    case LIBERA_CFG_PMOFFSET:
      PDEBUG("Set Post Mortem Offset: %d\n", req->val);
      dev->param[req->idx] = req->val;
      break;

    case LIBERA_CFG_EXTSWITCH:
    {
      unsigned int source = readl(iobase + DSC_SW_SOURCE);

      if (req->val)
        source |= DSC_SW_EXTERNAL;
      else
        source &= ~DSC_SW_EXTERNAL;

      writel(source, iobase + DSC_SW_SOURCE);
      PDEBUG("Set Switching Source: %d\n", req->val);
      dev->param[req->idx] = req->val;
    }
    break;

    case LIBERA_CFG_SWDELAY:
      PDEBUG("Set Switching Delay: %d\n", req->val);
      dev->param[req->idx] = req->val;
      writel(dev->param[req->idx], iobase + DSC_SW_DELAY);
      break;

    case LIBERA_CFG_DDC_MAFLENGTH:
      //TODO: Check FPGA design feature register
      PDEBUG("Set Moving Average Filter Length: %d\n", req->val);
      writel(req->val, iobase + DDC_MAF_LENGTH);
      break;

    case LIBERA_CFG_DDC_MAFDELAY:
      //TODO: Check FPGA design feature register
      PDEBUG("Set Moving Average Filter Delay: %d\n", req->val);
      writel(req->val, iobase + DDC_MAF_DELAY);
      break;

    case LIBERA_CFG_TRIGDELAY:
      PDEBUG("Changing Trigger Delay to: %d\n", req->val);
      dev->param[req->idx] = req->val;
      val = readl(iobase + T_TRIG_DELAY) & 0xc000ffff;
      val |= (req->val & 0x3fff) << 16;
      writel(val, iobase + T_TRIG_DELAY);
      break;

    case LIBERA_CFG_PM_OVERFLOW_DUR:
      if ( ( req->val <= pmdur ) &&
           ( req->val <= MAXDUR ) )
        dev->param[req->idx] = req->val;
      else {

        ret = -EINVAL;
        break;
      }
      unsigned long overflow_limit;
      overflow_limit = scale_kadc(dev->param[LIBERA_CFG_PM_OVERFLOW_LIMIT]);
      PDEBUG("PostMortem: %u %d %d %d %d %lu(0x%08x) %u\n",
             dev->param[LIBERA_CFG_PM_MODE],
             dev->param[LIBERA_CFG_PM_XLOW],
             dev->param[LIBERA_CFG_PM_XHIGH],
             dev->param[LIBERA_CFG_PM_YLOW],
             dev->param[LIBERA_CFG_PM_YHIGH],
             overflow_limit,
             dev->param[LIBERA_CFG_PM_OVERFLOW_LIMIT],
             dev->param[LIBERA_CFG_PM_OVERFLOW_DUR]);

      /* Pass to FPGA */        
      val = readl(iobase + T_TRIG_DELAY) & 0xFFFF3FFF;
      val |= (dev->param[LIBERA_CFG_PM_MODE] & 3)<<14;
      writel(val, iobase + T_TRIG_DELAY);

      writel(dev->param[LIBERA_CFG_PM_XLOW],  iobase + PM_XLOW);
      writel(dev->param[LIBERA_CFG_PM_XHIGH], iobase + PM_XHIGH);
      writel(dev->param[LIBERA_CFG_PM_YLOW],  iobase + PM_YLOW);
      writel(dev->param[LIBERA_CFG_PM_YHIGH], iobase + PM_YHIGH);

      writel(overflow_limit,
             iobase + DSC_PM_OVERFLOW_LIMIT);

      writel(dev->param[LIBERA_CFG_PM_OVERFLOW_DUR],
             iobase + DSC_PM_OVERFLOW_DUR);

      dev->pm_freezed = 0;
      PDEBUG("Sent PM parameters to FPGA and unfrozen buffer.\n");
      break;

    case LIBERA_CFG_SR_ENABLE:
      PDEBUG("Changing SR_ENABLE to: %d\n", req->val);
      dev->param[req->idx] = req->val;
      writel(req->val, iobase + FA_ENABLE);
      break;

    case LIBERA_CFG_SR_CSPI_ENABLE:
      PDEBUG("Changing SR_CSPI_ENABLE to: %d\n", req->val);
      dev->param[req->idx] = req->val;
      break;

    case LIBERA_CFG_SR_AVERAGING_STOP:
      PDEBUG("Changing SR_AVERAGING_STOP to: %d\n", req->val);
      dev->param[req->idx] = req->val;
      writel(req->val & 0x1F, iobase + FA_AVE_STOP);
      break;

    case LIBERA_CFG_SR_AVERAGE_WINDOW:
      PDEBUG("Changing SR_AVERAGE_WINDOW to: %d\n", req->val);
      dev->param[req->idx] = req->val;
      writel(req->val & 0x1F, iobase + FA_AVE_WIN);
      break;

    case LIBERA_CFG_SR_START:
      PDEBUG("Changing SR_START to: %d\n", req->val);
      dev->param[req->idx] = req->val;
      writel(req->val & 0x1F, iobase + FA_START);
      break;

    case LIBERA_CFG_SR_WINDOW:
      PDEBUG("Changing SR_WINDOW to: %d\n", req->val);
      dev->param[req->idx] = req->val;
      writel(req->val & 0x7F, iobase + FA_WINDOW);
      break;

    case LIBERA_CFG_SP_THRESHOLD:
      PDEBUG("Changing SB_THRESHOLD to: %d\n", req->val);
      dev->param[req->idx] = req->val;
      break;

    case LIBERA_CFG_SP_N_BEFORE:
      PDEBUG("Changing SB_N_BEFORE to: %d\n", req->val);
      dev->param[req->idx] = req->val;
      break;

    case LIBERA_CFG_SP_N_AFTER:
      PDEBUG("Changing SB_N_AFTER to: %d\n", req->val);
      dev->param[req->idx] = req->val;
      break;

    default:
      if ((req->idx >= LIBERA_CFG_COEFF)
          && (req->idx < LIBERA_CFG_POLYPHASE_FIR + LIBERA_CFG_FIR_MAX)) {

        int i; // Filter coefficients are written at once in a loop.

        PDEBUG2("Set coefficient[%d] to: %d\n", req->idx, req->val);
        dev->param[req->idx] = req->val;

        if (req->idx ==
            (LIBERA_CFG_NOTCH1 + LIBERA_CFG_NOTCH_MAX - 1)) {

          writel(1, iobase + FA_FIFO_RESET);
          PDEBUG("Set Notch1 coefficients.\n");
          for (i = 0; i < LIBERA_CFG_NOTCH_MAX; i++) {
            PDEBUG2("notch1[%d]: %d",
                    i, dev->param[LIBERA_CFG_NOTCH1 + i]);
            writel(dev->param[LIBERA_CFG_NOTCH1 + i],
                   iobase + FA_NOTCH1);
          }
        }
        else if (req->idx ==
                 (LIBERA_CFG_NOTCH2 + LIBERA_CFG_NOTCH_MAX - 1)) {

          writel(1, iobase + FA_FIFO_RESET);
          PDEBUG("Set Notch2 coefficients.\n");
          for (i = 0; i < LIBERA_CFG_NOTCH_MAX; i++) {
            PDEBUG2("notch2[%d]: %d",
                    i, dev->param[LIBERA_CFG_NOTCH2 + i]);
            writel(dev->param[LIBERA_CFG_NOTCH2 + i],
                   iobase + FA_NOTCH2);
          }
        }
        else if (req->idx ==
                 (LIBERA_CFG_POLYPHASE_FIR + dev->global->num_dfir - 1)) {

          writel(1, iobase + FA_FIFO_RESET);
          PDEBUG("Set Polyphase FIR coefficients.\n");
          for (i = 0; i < dev->global->num_dfir; i++) {
            PDEBUG2("FIR[%d]: %d",
                    i, dev->param[LIBERA_CFG_POLYPHASE_FIR + i]);
            writel(dev->param[LIBERA_CFG_POLYPHASE_FIR + i],
                   iobase + FA_POLYPHASE_FIR);
          }
        }
        break;
      }

      PDEBUG("CFG: Invalid SET request index %d (file: %s, line: %d)\n", 
             req->idx, __FILE__, __LINE__);
      return -EINVAL;

  } // switch (req->idx)

  return ret;
}


/** Libera ADC Device: Libera Electron (non-Brilliance) ADC transformation.
 *
 *  Transforms ADC atom's values from [0..4095] to [-2047..2048].
 *  @param in Not used.
 *  @param out Pointer to the CSPI_ADC_ATOM to transform.
 */
int ebpp_transform_adc( const void *in, void *out )
{
    libera_atom_adc_t *p = out;
    
    if (p->ChA > 2048) p->ChA -= 4096;
    if (p->ChB > 2048) p->ChB -= 4096;
    if (p->ChC > 2048) p->ChC -= 4096;
    if (p->ChD > 2048) p->ChD -= 4096;

    return 0;
}

/** Returns the ADC transform operator. */
libera_operator_t get_adc_transform(void)
{
    return LIBERA_IS_BRILLIANCE(lgbl.feature) ? NULL : ebpp_transform_adc;
}

#define LIBERA_ILK_ADC_12 1900
#define LIBERA_ILK_ADC_16 30000
/** Initialize Libera EBPP specific internal device structures.
 *
 * Initializes Libera EBPP specific internal device structures. 
 */
int libera_init_specific(void)
{
    struct libera_cfg_device   *dev_cfg   = &libera_cfg;
    struct libera_adc_device   *dev_adc   = &libera_adc;
    struct libera_event_device *event = &libera_event;

    int i;
    int ret = 0;
    unsigned int ilk_adc;


    /* Libera Brilliance */
    dev_adc->op = get_adc_transform();
    ilk_adc = LIBERA_IS_BRILLIANCE(lgbl.feature) ? 
        LIBERA_ILK_ADC_16 : LIBERA_ILK_ADC_12; 

    /* Mirror init */
    dev_cfg->param[LIBERA_CFG_XOFFSET] = 0;
    dev_cfg->param[LIBERA_CFG_YOFFSET] = 0;
    dev_cfg->param[LIBERA_CFG_QOFFSET] = 0;
    dev_cfg->param[LIBERA_CFG_KX] = 1e7;
    dev_cfg->param[LIBERA_CFG_KY] = 1e7;
    dev_cfg->param[LIBERA_CFG_ILK_XLOW] = (unsigned int)(-1000000L);
    dev_cfg->param[LIBERA_CFG_ILK_XHIGH] = 1e6;
    dev_cfg->param[LIBERA_CFG_ILK_YLOW] = (unsigned int)(-1000000L);
    dev_cfg->param[LIBERA_CFG_ILK_YHIGH] = 1e6;
    dev_cfg->param[LIBERA_CFG_ILK_MODE] = 0x03;
    dev_cfg->param[LIBERA_CFG_ILK_OVERFLOW_LIMIT] = ilk_adc;
    dev_cfg->param[LIBERA_CFG_ILK_OVERFLOW_DUR] = 5;
    /* Note: LIBERA_CFG_ILK_GAIN_LIMIT holds both dBm and Sum(ATTN).
     *       From gain.conf:
     *       -40 dBm: -40    16   16   0.5  10
     */
    dev_cfg->param[LIBERA_CFG_ILK_GAIN_LIMIT] = 
        LIBERA_IS_BRILLIANCE(lgbl.feature) ?
        ((unsigned int) -40 << 16) | (0+0) :
	((unsigned int) -40 << 16) | (16+16);

    // fix for bug 157,380
    readl(iobase + ADC_MAX);

    dev_cfg->param[LIBERA_CFG_PM_XLOW] = (unsigned int)(-1000000L);
    dev_cfg->param[LIBERA_CFG_PM_XHIGH] = 1e6;
    dev_cfg->param[LIBERA_CFG_PM_YLOW] = (unsigned int)(-1000000L);
    dev_cfg->param[LIBERA_CFG_PM_YHIGH] = 1e6;
    dev_cfg->param[LIBERA_CFG_PM_OVERFLOW_LIMIT] = ilk_adc;
    dev_cfg->param[LIBERA_CFG_PM_OVERFLOW_DUR] = 5;

    dev_cfg->param[LIBERA_CFG_SR_ENABLE] = 1;
    dev_cfg->param[LIBERA_CFG_SR_CSPI_ENABLE] = 1;
    dev_cfg->param[LIBERA_CFG_SR_AVERAGING_STOP] = -1;
    dev_cfg->param[LIBERA_CFG_SR_AVERAGE_WINDOW] = 8;
    dev_cfg->param[LIBERA_CFG_SR_START] = -3;
    dev_cfg->param[LIBERA_CFG_SR_WINDOW] = 8;

    if (LIBERA_IS_BRILLIANCE(lgbl.feature))
        dev_cfg->param[LIBERA_CFG_SP_THRESHOLD] = 400;
    else
        dev_cfg->param[LIBERA_CFG_SP_THRESHOLD] = 100;
    dev_cfg->param[LIBERA_CFG_SP_N_BEFORE] = 1;
    dev_cfg->param[LIBERA_CFG_SP_N_AFTER] = lgbl.d / 3; // 33%

    /* FPGA/HW init */
    for (i=LIBERA_CFG_CUSTOM_FIRST; i < LIBERA_CFG_CUSTOM_LAST; i++) {
	libera_cfg_request_t req;
	
	req.idx = i;
	req.val = dev_cfg->param[i];
	libera_cfg_set_specific(dev_cfg, &req);
    }


    /* Enable additional triggers */
    ret = libera_sctrig_enable(event, 
                               TRIGGER_BIT(T_TRIG_POST_MORTEM)
                               );
    if (ret) return ret;

    return ret;
}


/** Takes care of proper trigger sequence and can signal to ignore a
 *  certain trigger.
 */
int
libera_valid_trigger(struct libera_event_device *event, 
                     int trigno)
{
    /* No restriction to trigger sequence in EBPP */
    return 1;
}


/** Libera EBPP specific SC trigger handler */
void 
libera_SCtrig_specific(struct libera_event_device *event,
		       const libera_hw_time_t *stamp,
		       const libera_hw_time_t *stamp_raw,
		       unsigned long m, unsigned int i)
{
    struct libera_cfg_device *cfg = &libera_cfg;
    switch(i) 
    {
    case T_TRIG_MC_PRESCALER: // ..._MC_PRES... is not an error here!
    case T_TRIG_SC_PRESCALER:
        if (putToFIFO(& event->sc_timestamps[i], *stamp_raw) < 0 && 
	    (event->sc_trigVec & m)) //this should never happen
	  printk(KERN_CRIT "libera: SC irq: FIFO %d overflow.\n",i);
	break;

    case T_TRIG_POST_MORTEM:
	/* Insert the last PM piece and leave the rest for PM tasklet */
	if (!cfg->pm_freezed)
	    putLSTtoCircBuf(&event->paired_timestamps[i], stamp);
        break;
      
    default:
        if (putToFIFO(& event->sc_timestamps[i], *stamp) < 0 && 
	    (event->sc_trigVec & m)) //this should never happen
	  printk(KERN_CRIT "libera: SC irq: FIFO %d overflow.\n",i);
    }
}

/** Libera EBPP specific MC trigger handler */
void 
libera_MCtrig_specific(struct libera_event_device *event,
		       const libera_hw_time_t *stamp,
		       const libera_hw_time_t *stamp_raw,
		       unsigned long m, unsigned int i)
{
    struct libera_cfg_device *cfg = &libera_cfg;
    switch(i) 
    {
    case T_TRIG_POST_MORTEM:
	if (!cfg->pm_freezed) {
	    /* Insert the last PM piece and leave the rest for PM tasklet */
	    putLMTtoCircBuf(&event->paired_timestamps[i], stamp);
	    /* Notify userland about PM event */
	    PDEBUG3("POST PORTEM: LMT = 0x%08lx%08lx, j = %lu\n",
		    ULL(*stamp), jiffies);
	    libera_send_event(LIBERA_EVENT_PM, pmsize);
	    // freeze PM buffer for mode 1 and 2, overwrite for 0
	    if ( cfg->param[LIBERA_CFG_PM_MODE] ) {
		cfg->pm_freezed = 1;
	    }
	}
	else
	    PDEBUG2("PM buffer frozen, skipped event and acquisition.\n");
	break;

    case T_TRIG_MC_PRESCALER:
	if (putToFIFO(& event->mc_timestamps[i], 
		      *stamp_raw - event->settime.lmt.off_pll) < 0 && 
	    (event->mc_trigVec & m)) //this should never happen
	    printk(KERN_CRIT "libera: MC irq: FIFO %d overflow.\n",i);
	break;
	
    default:
	if (putToFIFO(& event->mc_timestamps[i], *stamp) < 0 && 
	    (event->mc_trigVec & m)) //this should never happen
	    printk(KERN_CRIT "libera: MC irq: FIFO %d overflow.\n",i);
    }
}


/** Libera EBPP specific SC helper IRQ handler */
void
libera_schelp_handler_specific(struct libera_event_device  *event)
{
    /* Unused. */
}


/** Libera EBPP specific MC helper IRQ handler */
void
libera_mchelp_handler_specific(struct libera_event_device  *event)
{
    DEBUG_ONLY(static unsigned long count; count++;);

    /* Start HB at appropriate time -- simple event generation */
    if ( ((event->mc_time + 0x4000000ULL) <= event->list.head->time ) &&
	 ( (event->mc_time + 0x8000000ULL) > event->list.head->time ) ) {
	libera_hw_time_t offset26;

	offset26 = event->list.head->time - (event->mc_time + 0x4000000ULL);
	DEBUG2_ONLY(
		    if (offset26 >= 0x4000000ULL)
		    PDEBUG2("VERY STRANGE: offset26 >= 0x4000000ULL\n");
		    );
	if (offset26 < 0x2000000ULL) {
	    // Schedule event
	    writel(event->list.head->event,
		   iobase + T_MC_EVENTGENL);
	    writel((unsigned long)(offset26 & 0x00000000FFFFFFFFULL),
		   iobase + T_MC_EVENTGENH);
	    PDEBUG("HELPER #%lu: EVGEN: 0x%08lx: offset26 = 0x%08lx\n", count,
                   event->list.head->event,
		   (unsigned long)(offset26 & 0x00000000FFFFFFFFULL));
	} 
    }
}


/** Libera EBPP specific SC SELF_INC trigger handler */
void
libera_sc_selfinc_specific(struct libera_event_device  *event)
{
    /* Unused. */
}


/** Libera EBPP specific MC SELF_INC trigger handler */
void
libera_mc_selfinc_specific(struct libera_event_device  *event)
{
    DEBUG_ONLY(static unsigned long count; count++;);

    /* Start HB at appropriate time -- simple event generation */
    if ( (event->mc_time <= event->list.head->time ) &&
         ( (event->mc_time + 0x4000000ULL) > event->list.head->time ) ) {
        libera_hw_time_t offset26;
        
        offset26 = event->list.head->time - event->mc_time;
        DEBUG2_ONLY(
                    if (offset26 >= 0x4000000ULL)
                    PDEBUG2("VERY STRANGE: offset26 >= 0x4000000ULL\n");
                    );
        if (offset26 >= 0x2000000ULL) {
            // Schedule event
            writel(event->list.head->event,
                   iobase + T_MC_EVENTGENL);
            writel((unsigned long)(offset26 & 0x00000000FFFFFFFFULL),
                   iobase + T_MC_EVENTGENH);
            PDEBUG("SELF #%lu: EVGEN: 0x%08lx: offset26 = 0x%08lx\n", count,
                   event->list.head->event,
                   (unsigned long)(offset26 & 0x00000000FFFFFFFFULL));
        } 
    }
}


/** Increments SA circular buffer pointer in a way that nobody sees
 *  an intermediate value.
 */
static inline void libera_sa_incr_atom(volatile libera_atom_sa_t **atomp)
{
    struct libera_sa_device  *dev  = &libera_sa;

    if (*atomp == (dev->buf + SA_FIFO_DEPTH - 1))
        *atomp = dev->buf;  /* Wrap */
    else
        (*atomp)++;    
}

/** Libera SA interrupt handler EBPP specific Top Half (TH).
 *
 * Takes care of reading the data from SA fifo on FPGA to internal buffer.
 * The rest is done in the tasklet Bottom Half (BH).
 */
void
libera_sa_handler_specific(void)
{
    struct libera_sa_device  *dev  = &libera_sa;
    unsigned FIFOstatus;
    DEBUG_ONLY(int rcount = 0);
    DEBUG2_ONLY(int count = 0);
    DEBUG3_ONLY(static int lcount);

    /* Read status once */ 
    FIFOstatus = readl(iobase + SA_STATUS);
    PDEBUG3("SA irq: Entering SA handler: status = 0x%08x\n", FIFOstatus);

    /* Check for Input Overrun */
    if (SA_INOVR(FIFOstatus))
	dev->inputovr = TRUE;

    /* Read all the available atoms from FIFO.
     * Reading in one block (multiple atoms) for greater efficiency is 
     * out of the question, beacuse of the circular buffer between top
     * and bottom half!
     */
    do
    {
	DEBUG3_ONLY(
	    if ( !(lcount++ % 10) ) {
		PDEBUG3("Reading from %p and %p\n", 
			(unsigned long *)dev->buf_head,
			(unsigned long *)dev->buf_head +
			sizeof(libera_atom_sa_t)/sizeof(unsigned long)/2);
	    }
	    );
	/* Read SA data first */
	libera_readlBlock((unsigned long *)(iobase + SA_FIFO_BASE),
			  (unsigned long *)dev->buf_head,
			  (sizeof(libera_atom_sa_t)/sizeof(unsigned long))/2);
	/* Add FAI additional 8 integers */
	libera_readlBlock((unsigned long *)(iobase + FAI_CORR_BASE),
			  (unsigned long *)(dev->buf_head) + 
			  sizeof(libera_atom_sa_t)/sizeof(unsigned long)/2,
			  (sizeof(libera_atom_sa_t)/sizeof(unsigned long))/2);
	libera_sa_incr_atom(&dev->buf_head);

	DEBUG_ONLY(
		   rcount++;
		   if (rcount > 1) {
		       PDEBUG("SA IRQ: Read %ux8 ints.\n", rcount);
		   }
		   );

	/* Refresh status */
	FIFOstatus = readl(iobase + SA_STATUS);
	PDEBUG3("SA status (loop #%u) = 0x%08x\n", count, FIFOstatus);

	/* Check for Input Overrun */
	if (SA_INOVR(FIFOstatus))
	    dev->inputovr = TRUE;

	/* Deadlock avoidance */
	DEBUG2_ONLY(if (count++ > 10) return;);
    } while(SA_SIZE(FIFOstatus));
}

/** Libera SA interrupt handler tasklet EBPP specific Bottom Half (BH).
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
void libera_sa_do_tasklet_specific(unsigned long data)
{
    struct libera_sa_device  *dev  = &libera_sa;
    
    int pipe_no;
    ssize_t ret;
    
    /* FIFO Input Overrun check */
    if (dev->inputovr)
    {
	dev->inputovr = FALSE;
	PDEBUG("WARNING: FPGA SA buffer overflow.\n");
	libera_send_event(LIBERA_EVENT_OVERFLOW,
			  LIBERA_OVERFLOW_SA_FPGA);
    }

    do 
    {
	/* Dispatch data to SA pipes */
	for (pipe_no = 0; pipe_no < LIBERA_SA_MAX_READERS; pipe_no++) {
	    if (dev->pipe.sa_pipe[pipe_no])
	    {
		/* We need to wake up the reader as libera_pipe_write() 
		 * does not take care of that already. 
		 */
		ret = libera_pipe_write(dev->pipe.sa_pipe[pipe_no], 
					(char *)(dev->buf_tail), 
					sizeof(libera_atom_sa_t));
		if (ret == -EFAULT) {
		    PDEBUG("WARNING: buffer overflow in SA pipe #%d\n", 
			   pipe_no);
		    libera_send_event(LIBERA_EVENT_OVERFLOW, 
				      LIBERA_OVERFLOW_SA_DRV);
		}
		else {
			wake_up_interruptible(&dev->pipe.sa_pipe[pipe_no]->wait);
		}    
	   }
	}
	/* Increment buffer tail pointer */
	libera_sa_incr_atom(&dev->buf_tail);
	libera_send_event(LIBERA_EVENT_SA, 1);
    } while (dev->buf_tail != dev->buf_head);
}

/** Libera Interlock handler EBPP specific.
 * Stores interlock cause in params.
 */
void libera_ilk_handler_specific(unsigned long status)
{
     struct libera_cfg_device   *dev_cfg   = &libera_cfg;
     // Keep the bits for cfg queries.
     dev_cfg->param[LIBERA_CFG_ILKSTATUS] |= status;
}


/** Libera EBPP DD Device: Called on lseek()
 *
 * Seek is (ab)used for providing the timing request information to the Libera
 * GNU/Linux driver. 
 * SEEK_SET mode is used for absolute timing definition (ST), 
 * SEEK_CUR is used for absolute timing definition (MT), and 
 * SEEK_END is used for trigger defined timing (last trigger). 
 * File version (f_version) stores all the relevant timing info.
 *
 * For details see: Alessandro Rubini et al., Linux Device Drivers, pp. 163.
 */
loff_t
libera_dd_llseek_specific(struct file *file, loff_t time, int whence)
{
    libera_Ltimestamp_t ctime;
    loff_t ret;
    libera_dd_local_t *dd_local = (libera_dd_local_t *)file->f_version;
    libera_Qtimestamp_t *Qts = &dd_local->Qts;

    /*
     * NOTE: The "whence" parameter is used to identify the time
     *       definition mode, e.g. EXPLICIT (ST & MT) and 
     *       IMPLICIT (trigger). 
     */

    switch(whence) {
    case 0: /* SEEK_SET */
	/* Interval starting time given in System Time (ST) */
	Qts->request = LIBERA_DD_TIME_EXPLICIT_ST;
	DEBUG3_ONLY(
	{
	    unsigned long timeh;
	    unsigned long timel;
	    timeh = time >> 32;
	    timel = time & (unsigned long long) 0x00000000FFFFFFFFULL;
	    PDEBUG2("llseek(): timeh = 0x%08lx\n", timeh);
	    PDEBUG2("llseek(): timel = 0x%08lx\n", timel);
	});
	/* "Current time" special request */
	if (time == 0ULL)
	{
	    ret = libera_get_CTIME(&ctime);
	    if (ret) return ret;
	    Qts->L.lst = ctime.lst;
	    lst2st(&(Qts->L.lst), &(Qts->U.st));
	}
	else
	{
	    memcpy(&(Qts->U.st), (struct timespec *)&time, 
		   sizeof(struct timespec));
	    st2lst(&(Qts->U.st), &(Qts->L.lst));
	}
        break;

    case 1: /* SEEK_CUR */
	/* Interval starting time given in Machine Time (MT) */
	Qts->request = LIBERA_DD_TIME_EXPLICIT_MT;
	DEBUG3_ONLY(
	{
	    unsigned long timeh;
	    unsigned long timel;
	    timeh = time >> 32;
	    timel = time & (unsigned long long) 0x00000000FFFFFFFFULL;
	    PDEBUG2("llseek(): timeh = 0x%08lx\n", timeh);
	    PDEBUG2("llseek(): timel = 0x%08lx\n", timel);
	});
	/* "Current time" special request */
	if (time == 0ULL)
	{
	    ret = libera_get_CTIME(&ctime);
	    if (ret) return ret;
	    Qts->L.lmt = ctime.lmt;
	    lmt2mt(&(Qts->L.lmt), &(Qts->U.mt));
	}
	else
	{
	    memcpy(&(Qts->U.mt), (libera_hw_time_t *)&time, 
		   sizeof(libera_hw_time_t));
	    mt2lmt(&(Qts->U.mt), &(Qts->L.lmt));
	}

        break;

    case 2: /* SEEK_END */
	/* Interval starting time defined by external trigger */
	Qts->request = LIBERA_DD_TIME_IMPLICIT;
	Qts->trigoffset = time;
	break;

    default: /* can't happen */
        return -EINVAL;
    }
    
    return 0;
}

/** Atomic helper for libera_OB_fifo_sync() */
inline int dma_no_data(unsigned long *obFIFOstatus) {
    int ret;
    libera_dma_t *dma = &lgbl.dma;
    
    /* (dma->remaining) &&
       !(DD_OB_SIZE(*obFIFOstatus)) && !dma->Overrun */
    spin_lock(&dma_spin_lock);
    ret = ( (dma->remaining) &&
            !(DD_OB_SIZE(*obFIFOstatus)) && !dma->Overrun );
    spin_unlock(&dma_spin_lock);
    
    return ret;
}

/** Helper function for OB-fifo synchronization.
 *
 * Checks OB-fifo status register and waits (by blocking) for available 
 * data in OB-fifo.
 * At least one DD atom should be present in OB-fifo for both DMA 
 * and non-DMA transfers to be able to continue properly.
 * This translates to OB-fifo size > 8,
 * OB-fifo size > sizeof(libera_atom_dd_t)/sizeof(unsigned long).
 * On success, 0 is returned.
 * On error, meaningful negative errno is returned.
 */
static inline ssize_t libera_OB_fifo_sync(unsigned long *obFIFOstatus)
{
    int wait_count = 0;
    ssize_t ret = 0;
    libera_dma_t *dma = &lgbl.dma;
    
    while ( dma_no_data(obFIFOstatus) ) {
	/* FIFO size not large enough, but still expecting more
	 * data to appear through OB-fifo -> reading too fast.
	 */
	PDEBUG3("DD read(): Reading TOO FAST: sleeping...\n");
	libera_delay_jiffies(DD_WAIT_STEP);
	*obFIFOstatus = readl(iobase + DD_OB_STATUS);
	/* NOTE: The OVERRUN bit cannot be set if the driver is chasing
	 *       the FPGA CB-PUT pointer (reading too fast), because 
	 *       of the SDRAM-OB_FIFO synchronization in FPGA.
	 *       Checking the overrun bit here is therefore not 
	 *       necessary. 
	 *       Even without this SDRAM-OB_FIFO sync in FPGA it would 
	 *       be highly unlikely for OVERRUN to occur in such
	 *       a situation; that is, driver reading from the OB_FIFO 
	 *       faster than the FPGA is (capable of) writing to.
	 *
	 *       On the other hand... if the driver is (reading) too fast,
	 *       it does not hurt to burn some CPU cycles for OVERRUN check.
	 *       Let's do it anyway, just to be on the safe side.
	 */
	if (DD_OB_OVERRUN(*obFIFOstatus)) {
            spin_lock(&dma_spin_lock);
	    dma->Overrun = TRUE;
            spin_unlock(&dma_spin_lock);
	    PDEBUG("Overrun: OB_status = 0x%08lx in fifo sync.\n", dma->obFIFOstatus);
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
	if ( wait_count++ > HZ) {
            spin_lock(&dma_spin_lock);
	    PDEBUG("DD: Timeout waiting for OB-fifo data.\n");
	    PDEBUG("dma->remaining = %ld , OB_status = %lu\n",
		    dma->remaining, *obFIFOstatus);
	    PDEBUG("Error in file: %s, line: %d)\n", 
		   __FILE__, __LINE__);
            spin_unlock(&dma_spin_lock);
	    return -EIO;
	}
    }

    return ret;
}


/** Check if data retrieval is possible and wait in LMT units if necessary */
static int
wait_lmt(libera_Ltimestamp_t *ctime,
	 libera_hw_time_t *start_lmt,
	 unsigned long span_atoms)
{
    libera_S64_t delta_LMT_start; /* LMT after start of req. interval */
    libera_S64_t delta_LMT_end;   /* LMT after end   of req. interval */
    libera_hw_time_t LMT_delay;
    long timeout_jiff;
    int ret = 0;
    DEBUG2_ONLY(int wait_count);

    delta_LMT_start = ctime->lmt - *start_lmt;

    /* Safety margin */
    if ( delta_LMT_start > 
	 (LIBERA_DD_CIRCBUF_ATOMS * lgbl.d) - LIBERA_DD_CIRCBUF_SAFE )
    { /* Too late! */
        PDEBUG("Request TOO LATE: delta_LMT_start = 0x%08lx%08lx\n",
	       ULL(delta_LMT_start));
        return -ENODATA; /* No data available */
    }
    /* Have we passed the end of interval already? */
    delta_LMT_end = delta_LMT_start - (LIBERA_DD_READSYNC_MARGIN * lgbl.d);

    if (delta_LMT_end < 0)
    {
	/* Data request is early -> Wait a bit... but not too long! */
	if (-delta_LMT_end > (LIBERA_DD_MAX_INADVANCE * lgbl.d)) 
	{
	    /* Too early! */
	    return -EAGAIN; /* Try again later. */
	} 
	else
	{
	    DEBUG2_ONLY(wait_count = 0);
	    do
	    {
		DEBUG2_ONLY(
		if (++wait_count > DD_WAIT_TIMEOUT)
		{
		    PDEBUG2("DD (timing): Timeout waiting for LMT after %d wait cycles\n", 
			       wait_count-1);
		    return -EDEADLK;
		});
		
		LMT_delay = -delta_LMT_end;
		timeout_jiff = div_u64((LMT_delay * FLMC_DECI_HZ * HZ), flmcdHz) + 1;

		PDEBUG2("Sleeping %ld jiff (LMT_delay = 0x%08lx%08lx)...\n", 
		       timeout_jiff, ULL(LMT_delay));
		ret = libera_delay_jiffies_interruptible(timeout_jiff);
		if (ret) return ret;
		/* We are back. */
		/* Check the time again TODO: This might not be necessary. */
		ret = libera_get_CTIME(ctime);
		if (ret) {
		    PDEBUG2("wait_lmt(): Error reading CTIME.\n");
		    return ret;
		}
		delta_LMT_end = ctime->lmt - *start_lmt - 
		    (LIBERA_DD_READSYNC_MARGIN * lgbl.d);
	    } while ( delta_LMT_end < 0);
	}
    }

    return ret;
}


/** Check if data retrieval is possible and wait in LMT units if necessary */
static int
wait_lst(libera_Ltimestamp_t *ctime,
	 libera_hw_time_t *start_lst,
	 unsigned long span_atoms)
{
    libera_S64_t delta_LST_start; /* LST after start of req. interval */
    libera_S64_t delta_LST_end;   /* LST after end   of req. interval */
    libera_hw_time_t LST_delay;
    long timeout_jiff;
    int ret = 0;
    DEBUG2_ONLY(int wait_count);

    delta_LST_start = ctime->lst - *start_lst;

    /* Safety margin */
    if ( delta_LST_start > 
	 (LIBERA_DD_CIRCBUF_ATOMS * lgbl.d) - LIBERA_DD_CIRCBUF_SAFE )
    { /* Too late! */
        PDEBUG("Request TOO LATE: delta_LST_start = 0x%08lx%08lx\n",
	       ULL(delta_LST_start));
        return -ENODATA; /* No data available */
    }
    /* Have we passed the end of interval already? */
    delta_LST_end = delta_LST_start - (LIBERA_DD_READSYNC_MARGIN * lgbl.d);

    if (delta_LST_end < 0)
    {
	/* Data request is early -> Wait a bit... but not too long! */
	if (-delta_LST_end > (LIBERA_DD_MAX_INADVANCE * lgbl.d)) 
	{
	    /* Too early! */
	    return -EAGAIN; /* Try again later. */
	} 
	else
	{
	    DEBUG2_ONLY(wait_count = 0);
	    do
	    {
		DEBUG2_ONLY(
		if (++wait_count > DD_WAIT_TIMEOUT)
		{
		    PDEBUG2("DD (timing): Timeout waiting for LST after %d wait cycles\n", 
			       wait_count-1);
		    return -EDEADLK;
		});
		
		LST_delay = -delta_LST_end;
		timeout_jiff = div_u64((LST_delay * HZ), LSC_FREQ) + 1;

		PDEBUG2("Sleeping %ld jiff (LST_delay = 0x%08lx%08lx)...\n", 
		       timeout_jiff, ULL(LST_delay));
		ret = libera_delay_jiffies_interruptible(timeout_jiff);
		if (ret) return ret;
		/* We are back. */
		/* Check the time again TODO: This might not be necessary. */
		ret = libera_get_CTIME(ctime);
		if (ret){
		    PDEBUG2("wait_lst(): Error reading CTIME.\n");
		    return ret;
		}
		delta_LST_end = ctime->lst - *start_lst - 
		    (LIBERA_DD_READSYNC_MARGIN * lgbl.d);
	    } while ( delta_LST_end < 0);
	}
    }

    return ret;
}


/** Atomic DMA helper fcn */
inline int dma_needs_start(void) {
    libera_dma_t *dma = &lgbl.dma;
    int ret;
    
    spin_lock(&dma_spin_lock);
    ret = (!dma->DMAC_transfer) && (dma->remaining);
    spin_unlock(&dma_spin_lock);
    
    return ret;
}


/** Helper function for reading from the DD Output Buffer (OB) FIFO, via
 *  DMA fifo, using DMA transfer, to userspace.
 *
 * Reads \param atom_count atoms from DD Output Buffer (OB) FIFO
 * to userland buffer \param buf.
 * On success, the number of read atoms is returned.
 * On error, meaningful negative errno is returned.
 */
static inline ssize_t
libera_dd_transfer_OBfifo_DMA(char *userbuf, 
			      size_t atom_count,
			      char *buf)
{
    struct libera_dd_device  *dd  = &libera_dd;
    libera_dma_t *dma = &lgbl.dma;
    wait_queue_t wait;

    size_t last_yield = jiffies;
    unsigned long OBAtoms = 0;
    unsigned long fifo_atoms;
    register unsigned long i;
    ssize_t sync_ret;
    int ret = 0;

    DEBUG_ONLY(int sleep_count = 0);
    DEBUG2_ONLY(int dma_while_count = 0);

    spin_lock(&dma_spin_lock);

    /* Get status */
    dma->obFIFOstatus = readl(iobase + DD_OB_STATUS);
    PDEBUG3("Starting DMA OB transfer: OB_status = 0x%08lx, OB_size = 0x%lx\n",
	    dma->obFIFOstatus, DD_OB_SIZE(dma->obFIFOstatus));

    /* Initialize DMA structures */
    dma->Overrun = FALSE;
    dma->DMAC_transfer = FALSE;
    dma->remaining = atom_count;
    dma->aborting = FALSE;
    dma->written = 0;
    PDEBUG3("Flushing DMA fifo...\n");
    flushDMA_FIFO(dma);
    libera_dma_get_DMAC_csize(dma);
		    
    spin_unlock(&dma_spin_lock);

    /* NOTE: OB-fifo size should be > 0 here, otherwise we would not get 
     *       DD interrupt, however, this does not guarantee that 
     *       dma->csize > 0 as well.
     *       This is highly unlikely, since SBC is slow compared to FPGA,
     *       but to eliminate the risk of getting DMA bus errors, we
     *       sync to OB-fifo size before starting dma transfers.
     */
    sync_ret = libera_OB_fifo_sync(&dma->obFIFOstatus);
    if ( sync_ret < 0 ) return sync_ret;
    // TODO: LIBERA_IOBASE is not configurable !!!
    spin_lock(&dma_spin_lock);
    libera_dma_get_DMAC_csize(dma);
    libera_dma_command((LIBERA_IOBASE + DD_OB_FIFOBASE),
                       &dma->buf[dma->put],
                       dma->csize*sizeof(libera_atom_dd_t));
    spin_unlock(&dma_spin_lock);

    /* Loop until the DMA transfer is over */
    do {
#if 0 // UNINTERRUPTIBLE
        /* Check fifo size & wait/sleep if neccessary */
        while (emptyDMA_FIFO(dma))
        {
            DEFINE_WAIT(waitl);

            prepare_to_wait(&dd->DMA_queue,&waitl, TASK_UNINTERRUPTIBLE);
            if (emptyDMA_FIFO(dma)) {
                //schedule();
                if (!schedule_timeout(100)) {
                    PDEBUG("Timeout in file: %s, line: %d\n",
                           __FILE__, __LINE__);
                }
                
            }
            finish_wait(&dd->DMA_queue,&wait);
            DEBUG_ONLY(
                       if ( sleep_count++ > 10000) {
                           PDEBUG("BREAKING DMA sleeping loop.\n");
                           PDEBUG("Error in file: %s, line: %d)\n",
                                  __FILE__, __LINE__);
                           ret = -EDEADLK;
                           break;
                       });
        }
        
        if (ret) return ret;
#else // INTERRRUPTIBLE
        /* Check fifo size & wait/sleep if neccessary */
        if (emptyDMA_FIFO(dma))
	{
	    init_waitqueue_entry(&wait,current);
	    add_wait_queue(&dd->DMA_queue,&wait);
	    for (;;)
	    {
	        set_current_state(TASK_INTERRUPTIBLE);
		if (not_emptyDMA_FIFO(dma)) {
		    break;
		}
		if (!signal_pending(current) && !dd->pm_entry) {
		    schedule();
		    continue;
		}
                // mark aborting condition
                dma->aborting = TRUE;
		// NOTE: This will only work for small read() requests.
		DEBUG2_ONLY(
		    if ( sleep_count++ > 10000) {
			PDEBUG2("BREAKING DMA sleeping loop.\n");
			PDEBUG2("Error in file: %s, line: %d)\n", 
				__FILE__, __LINE__);
			ret = -EDEADLK;
			break;
		    });
		if (dd->pm_entry)
		    PDEBUG("aborting due to pm entry.\n");
		else
		    PDEBUG("aborting due to pending signal!\n");
		ret = -ERESTARTSYS;
		break;
	    }
	    set_current_state(TASK_RUNNING);	
	    remove_wait_queue(&dd->DMA_queue, &wait);
	}
	
        if (ret) return ret;
#endif

        /* Get data from DMA fifo */
        fifo_atoms = lenDMA_FIFO(dma);
        PDEBUG3("DMA-OB: fifo_atoms = %lu\n", fifo_atoms);
        if (fifo_atoms)
	{
	    /* Userland transfer */
	    if (userbuf) {
		for (i=0; i < fifo_atoms; i++) {
		    if ((OBAtoms++ < atom_count) && !(dma->Overrun))
		    {
			if (copy_to_user(userbuf, &dma->buf[dma->get],
					 sizeof(libera_atom_dd_t))) {
			    return -EFAULT;
			}
			else
			{
			    dma->written++;
			    userbuf += sizeof(libera_atom_dd_t);
			}
		    }
                    spin_lock(&dma_spin_lock);
		    dma->get = (dma->get + 1) & LIBERA_DMA_FIFO_MASK;
                    spin_unlock(&dma_spin_lock);
		}
		PDEBUG3("Copied %lu atoms to Userland.\n", i);
	    }

	    /* Kernel buffer transfer */
	    if (buf) {
		for (i=0; i < fifo_atoms; i++) {
		    if ((OBAtoms++ < atom_count) && !(dma->Overrun))
		    {
			memcpy(buf, &dma->buf[dma->get],
			       sizeof(libera_atom_dd_t));
			dma->written++;
			buf += sizeof(libera_atom_dd_t);
		    }
                    spin_lock(&dma_spin_lock);
		    dma->get = (dma->get + 1) & LIBERA_DMA_FIFO_MASK;
                    spin_unlock(&dma_spin_lock);
		}
		PDEBUG3("Copied %lu atoms to kernel buffer.\n", i);
	    }
	}
	else
	{
	    /* DMA FIFO empty ?!
	     * This should never happen when we're awake.
	     */
	    printk(KERN_CRIT "DD read(): BUG: Awake when DMA FIFO empty ?!\n");
	    return -EFAULT;
	}

        /* Give other threads a chance */
        if ( jiffies > (last_yield + LIBERA_YIELD_INTERVAL) ) {
            PDEBUG("Yielding: j: %10lu, a: %10lu\n", jiffies, dma->written);
            last_yield = jiffies;
            yield();
            if (signal_pending(current) || dd->pm_entry) {
                // mark aborting condition
                dma->aborting = TRUE;
                if (dd->pm_entry)
                    PDEBUG("aborting due to pm entry.\n");
                else
                    PDEBUG("Aborting due to pending signal!\n");
                return -ERESTARTSYS;
            }
        }
            
	/* Start DMA again if needed */
	/* (!dma->DMAC_transfer) && (dma->remaining)*/
        if ( dma_needs_start() ) {
            spin_lock(&dma_spin_lock);
	    /* Refresh status */
	    dma->obFIFOstatus = readl(iobase + DD_OB_STATUS);
		
	    /* Check Overrun */
	    if (DD_OB_OVERRUN(dma->obFIFOstatus)) {
		dma->Overrun = TRUE;
		PDEBUG("Overrun: OB_status = 0x%08lx in transfer.\n", dma->obFIFOstatus);
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
            spin_unlock(&dma_spin_lock);

	    /* Reading speed sync */
	    sync_ret = libera_OB_fifo_sync(&dma->obFIFOstatus);
	    if ( sync_ret < 0 ) return sync_ret;

	    spin_lock(&dma_spin_lock);

	    libera_dma_get_DMAC_csize(dma);
	    
	    /* NOTE: At this stage, it is still possible that dma->csize == 0.
	     *       If it equals zero, it can only mean that our DMA fifo
	     *       is full. In this case we just pass control to the 
	     *       main DMA while loop to empty the DMA fifo first.
	     */	    
	    if (!(dma->csize > 0)) {
		if  ( (LIBERA_DMA_FIFO_ATOMS - 1 - lenDMA_FIFO(dma)) == 0) {
                    spin_unlock(&dma_spin_lock);
		    continue;
                }
	    }

	    /* NOTE: At this stage, dma->csize should not in any case be 
	     *       dma->csize == 0. If it equals zero, we will get
	     *       DMA BUS ERROR -> print debug info.
	     */
	    DEBUG_ONLY(
	        if (!(dma->csize > 0)) {
		    PDEBUG("dma->csize = %ld\n", 
			   dma->csize);
		    PDEBUG("dma->remaining = %ld\n", 
			   dma->remaining);
		    PDEBUG("dma->obFIFOstatus = 0x%lx\n", 
			   dma->obFIFOstatus);
		    PDEBUG("OB_size = %lu\n",
			   DD_OB_SIZE(dma->obFIFOstatus));
		    PDEBUG("OB_busy = %lu\n",
			   DD_OB_BUSY(dma->obFIFOstatus));
		    PDEBUG("tailDMA_FIFO(dma) = %d\n", 
			   tailDMA_FIFO(dma));
		    PDEBUG("DMA fifo free = %d\n",
			   (LIBERA_DMA_FIFO_ATOMS - 1 - lenDMA_FIFO(dma)));
		    PDEBUG("DMA put = %lu\n", dma->put);
		    PDEBUG("DMA get = %lu\n", dma->get);
		    PDEBUG("OBAtoms = %lu\n", OBAtoms);
		    PDEBUG("dma->written = %lu\n", 
			   (unsigned long)dma->written);
		});
	    
	    // TODO: LIBERA_IOBASE is not configurable !!!
	    if (!dma->Overrun) {
	        libera_dma_command((LIBERA_IOBASE + DD_OB_FIFOBASE),
				   &dma->buf[dma->put],
				   dma->csize*sizeof(libera_atom_dd_t));
	    } else {
	      /* Stop the DMA transfer in case of overrrun */
                spin_unlock(&dma_spin_lock);
                break;
	    }
            spin_unlock(&dma_spin_lock);
	}

	// NOTE: This will only work for small read() requests.
	DEBUG2_ONLY({
	    if ( dma_while_count++ > 10000000) {
		PDEBUG2("BREAKING DMA while loop (dma->remaining = %ld, lenDMA_FIFO(dma) = %d).\n", dma->remaining, lenDMA_FIFO(dma));
		PDEBUG2("Error in file: %s, line: %d)\n", 
			__FILE__, __LINE__);
		return -EDEADLK;
	    }
	});
	
    } while ( (dma->remaining) ||
              (not_emptyDMA_FIFO(dma)) );
    

    /* End of DMA transfer
     * Check for OB_size inconsistency.
     */
    spin_lock(&dma_spin_lock);
    dma->obFIFOstatus = readl(iobase + DD_OB_STATUS);
    PDEBUG2("End of DMA transfer: OB_status = 0x%lx, OB_size = 0x%lx\n",
	    dma->obFIFOstatus,
	    DD_OB_SIZE(dma->obFIFOstatus));    
    /* Only check for resudual OB_size if there was no OB overrun. */
    if (!dma->Overrun && DD_OB_SIZE(dma->obFIFOstatus)) {
        /* This should never happen. */
	PDEBUG("DD read(): BUG: OB_size=0x%lx after DMA transfer.\n",
	       DD_OB_SIZE(dma->obFIFOstatus));
	PDEBUG("Error in file: %s, line: %d)\n", 
	       __FILE__, __LINE__);
	/* NOTE: Emptying OB-fifo for future DD requests is not
         *       necessary since OB_SIZE is reset upon every CB_FIFO request.
	 */
        spin_unlock(&dma_spin_lock);
	return -EIO;
    }


    /* Check for FPGA size consistency */
    if ( !dma->Overrun && (OBAtoms != atom_count) )
    {
        PDEBUG("DD read(): FPGA size inconsistency: %d : %lu\n",
	       atom_count, OBAtoms);
        spin_unlock(&dma_spin_lock);
	return -EFAULT;
    }

    DEBUG_ONLY(if (dma->Overrun) PDEBUG("Circular buffer OVERRUN!\n"));    

    spin_unlock(&dma_spin_lock);
    return dma->written;
}


/** Helper function for reading from the DD Output Buffer (OB) FIFO, via
 *  DMA fifo, using DMA transfer, to kernel buffer.
 */
static inline ssize_t
libera_dd_read_OBfifo_DMA(char *buf, size_t atom_count)
{
    return libera_dd_transfer_OBfifo_DMA(NULL, 
					 atom_count,
					 buf);
}


/** Libera EBPP DD Device: Called on read()
 *
 * Reads data from proper position in the circular buffer.
 * \param count denotes the No. of aoms to read in bytes.
 * The returned data, via \param buf, consists of raw data from 
 * Circular Buffer.
 * Only "atom-aligned" read() requests are allowed.
 *
 * On success, number of read and returned bytes is returned.
 * On failure, meaningful negative errno is returned.
 *
 * For details see: Alessandro Rubini et al., Linux Device Drivers, pp. 64.
 */
ssize_t 
libera_dd_read_specific(struct file *file, char *buf,
			size_t count, loff_t *f_pos)
{
    struct libera_dd_device *dev =
    	(struct libera_dd_device *) file->private_data;
    libera_dd_local_t *dd_local = (libera_dd_local_t *)file->f_version;
    libera_Qtimestamp_t *Qts = &dd_local->Qts;

    ssize_t ret = 0;
    ssize_t CB_ret;
    ssize_t OB_ret = 0;
    ssize_t IRQ_ret;
    unsigned long span_atoms;
    libera_Ltimestamp_t ctime;
    libera_Ltimestamp_t *last_trigger;
    libera_hw_time_t dd_dummy;
    libera_dma_t *dma = &lgbl.dma;

    DEBUG3_ONLY(static int read_syscall_counter);
    PDEBUG3("Entering read() system call (N=%d).\n", read_syscall_counter++);

    /* Lock the whole device */
    if (mutex_lock_interruptible(&dev->sem))
	return -ERESTARTSYS;

    PDEBUG3("DD read(): Got DD lock.\n");
    /* Sanity check regarding count */
    if (count > (LIBERA_DD_MAX_INTERVAL*sizeof(libera_atom_dd_t))) {
	PDEBUG("DD: read(): Parameter count too big.\n");
	ret = -EINVAL;
	goto out;
    }

    /* Filter out strange, non-atomically-dividable values. */
    if (count % sizeof(libera_atom_dd_t)) {
	PDEBUG("DD: read(): Inapropriate count size.\n");
    	ret = -EINVAL;
	goto out;
    }
    span_atoms = count/sizeof(libera_atom_dd_t);

    /* Zero-atom-length request returns no data */
    if (!span_atoms)
	goto out_zero;

    /* 
     * Check for early/late absolute requests. 
     * In case of early request, wait for data to become available.
     */
    /* What is the time now? Get current Libera time */ 
    ret = libera_get_CTIME(&ctime);
    if (ret) goto out;

    /* Request given in absolute ST */
    if (Qts->request == LIBERA_DD_TIME_EXPLICIT_ST)
    {
        PDEBUG3("DD: read(): LIBERA_DD_TIME_EXPLICIT_ST\n");
	/* Check/Wait in LST units */
	ret = wait_lst(&ctime, &(Qts->L.lst), span_atoms);
	if (ret) {
	    PDEBUG2("DD read(): Error in wait_lst().\n");
	    goto out;
	}
        /* Define the missing parts of Qts (LMT & MT) */
	ret = lst2lmt(&(Qts->L.lst), &(Qts->L.lmt), &ctime);
	if (ret) {
	    PDEBUG2("DD read(): Error in lst2lmt().\n");
	    goto out;
	}
	lmt2mt(&(Qts->L.lmt), &(Qts->U.mt));
    }
    /* Request given in absolute MT */
    if (Qts->request == LIBERA_DD_TIME_EXPLICIT_MT)
    {
        PDEBUG3("DD: read(): LIBERA_DD_TIME_EXPLICIT_MT\n");
	/* Check/Wait in LMT units */
	ret = wait_lmt(&ctime, &(Qts->L.lmt), span_atoms);
	if (ret) {
	    PDEBUG2("DD read(): Error in wait_lmt().\n");
	    goto out;
	}
        /* Define the missing parts of Qts (LST & ST) */
	ret = lmt2lst(&(Qts->L.lmt), &(Qts->L.lst), &ctime);
	if (ret) {
	    PDEBUG2("DD read(): Error in lmt2lst().\n");
	    goto out;
	}
	lst2st(&(Qts->L.lst), &(Qts->U.st));
    }
    /* Trigger defined time (implicitly defined time) */
    if (Qts->request == LIBERA_DD_TIME_IMPLICIT)
    {
	struct libera_event_device  *event  = &libera_event;
	PDEBUG3("DD: read(): LIBERA_DD_TIME_IMPLICIT\n");
	/* Get the last trigger */
	last_trigger = 
	    getFromCircBuf(&event->paired_timestamps[T_TRIG_TRIGGER]);

	PDEBUG2("Got last trigger, lmt: 0x%08lx%08lx, lst: 0x%08lx%08lx\n",
		       	ULL(last_trigger->lmt), ULL(last_trigger->lst));
        /* Last Trigger with _MT_ offset */
        Qts->L.lmt = last_trigger->lmt + (Qts->trigoffset * lgbl.d);
        Qts->L.lst = last_trigger->lst;
        /* Define the missing parts of Qts (LST & ST) */
	ret = lmt2lst(&(Qts->L.lmt), &(Qts->L.lst), last_trigger);
	if (ret) {
	    PDEBUG("DD read(): Error in lmt2lst().\n");
	    goto out;
	}
	lmt2mt(&(Qts->L.lmt), &(Qts->U.mt));
	lst2st(&(Qts->L.lst), &(Qts->U.st));
	/* Check/Wait in LMT units */
	ret = wait_lmt(&ctime, &(Qts->L.lmt), span_atoms);
	if (ret) {
	    PDEBUG("DD read(): Error in wait_lmt().\n");
	    goto out;
	}
	PDEBUG2("store timestamp,  lmt: %lld, lst: %ld.%09ld\n",
		       	Qts->U.mt, Qts->U.st.tv_sec, Qts->U.st.tv_nsec);
    }
    
    /* Store timestamp for later ioctl() retrieval */
    memcpy(&dd_local->tstamp, &(Qts->U), sizeof(libera_timestamp_t));

    /* Write command to CB fifo (initiate data transfer) */
    CB_ret = libera_dd_write_CBfifo(get_circ_offset_lmt(&Qts->L.lmt),
				    span_atoms,
				    dd_local->dec);
    if (CB_ret < 0)
    { 
	ret = CB_ret;
	goto out;
    }

    /* Wait for DD interrupt */
    PDEBUG3("DD read(): Waiting for DD interrupt...\n");
    IRQ_ret = getBlockedFromFIFO(&dev->dd_irqevents,
				 &dev->DD_queue,
				 &dd_dummy);
    if (IRQ_ret) goto out;
    PDEBUG3("DD read(): Awoken by DD interrupt handler.\n");

    /* And now begin the data transfer */
    OB_ret = libera_dd_transfer_OBfifo_DMA(buf, span_atoms, NULL);
    if (OB_ret < 0)
    {
        // reset SDRAM controller
        PDEBUG("SDRAM controller reset.\n");
        writel(0, iobase + DD_CB_FIFO_RESET);
	ret = dma->written * sizeof(libera_atom_dd_t);
	goto out;
    }
    else
	ret += OB_ret * sizeof(libera_atom_dd_t);


    PDEBUG3("DD (%p): Read %lu atoms from MT = %llu\n", 
	   file, (unsigned long)OB_ret, Qts->U.mt);

 out_zero:
    /* MT & LMT increment for future read() requests */
    /* All the other but the 1st request are MT */
    Qts->request = LIBERA_DD_TIME_EXPLICIT_MT;
    Qts->U.mt += OB_ret * dd_local->dec;
    PDEBUG3("Incremented f_pos MT=%llu\n", Qts->U.mt);
    mt2lmt(&(Qts->U.mt), &(Qts->L.lmt));

 out:
    /* Re-enable DD interrupt */
    PDEBUG3("DD read(): Re-enabling DD interrupt.\n");
    set_irqMask((get_irqMask() | LIBERA_INTERRUPT_DD_MASK));

    PDEBUG3("read() returning %d\n", ret);
    mutex_unlock(&dev->sem);
    PDEBUG3("DD read(): Released DD lock.\n");
    return ret;
}


/** Acqires PM data into PM buffer. 
 *
 * Takes care of reading the data from History Buffer to internal
 * buffer for later retrieval using read() on PM device. 
 */
ssize_t
libera_acq_pm(void)
{
    struct libera_pm_device    *pm    = &libera_pm;
    struct libera_event_device *event = &libera_event;
    struct libera_dd_device    *dd    = &libera_dd;
    struct libera_cfg_device   *cfg   = &libera_cfg;
    libera_Ltimestamp_t *pPMevent;
    libera_hw_time_t LMTstart;
    int locked = FALSE;
    int wait = 30;
    ssize_t ret = 0;
    libera_hw_time_t dd_dummy;
    libera_Ltimestamp_t ctime;


    /* NOTE: DD request might be in progress. In that case we 
     *       signal pm_entry conddition so that DD device becomes
     *       available.
     */

    dd->pm_entry = 1;

    do {

        if (waitqueue_active(&dd->DMA_queue))
            wake_up_interruptible(&dd->DMA_queue);

        PDEBUG("Try to obtain DD lock.\n");
        if (mutex_trylock(&dd->sem)) {
	    /* We've locked the DD device! */
	    locked = TRUE;
            PDEBUG("Got DD lock @wait: %d.\n", wait);
	    break;
	}
	libera_delay_jiffies_interruptible(DD_WAIT_STEP);
    } while (wait-- > 0);

    dd->pm_entry = 0;

    if (!locked) {
	PDEBUG("DD device busy. Cannot acquire PM data.\n");
	return -EBUSY;
    }

    pPMevent =
	getFromCircBuf(&event->paired_timestamps[T_TRIG_POST_MORTEM]);
    memcpy(&pm->PMevent, pPMevent, sizeof(libera_Ltimestamp_t));

    /* MT & ST timestamp for later ioctl() retrieval */
    lmt2mt(&pm->PMevent.lmt, &pm->tstamp.mt);
    lst2st(&pm->PMevent.lst, &pm->tstamp.st);
    
    /* PMevent defines the END of interval, but we need the 
       START of interval. */
    LMTstart = pm->PMevent.lmt + (long long)(((long)cfg->param[LIBERA_CFG_PMOFFSET] - (long)pmsize)*(long)lgbl.d);
    PDEBUG3("PM_trigger = 0x%08lx%08lx\n", ULL(pm->PMevent.lmt));
    PDEBUG3("LMTstart   = 0x%08lx%08lx\n", ULL(LMTstart));
    PDEBUG3("PM_offset  = 0x%08lx\n", cfg->param[LIBERA_CFG_PMOFFSET]);
    PDEBUG3("pmsize     = 0x%08lx\n", pmsize);

    /* Is it too late? */
    ret = libera_get_CTIME(&ctime);
    if (ret) goto out;
    ret = wait_lmt(&ctime, &LMTstart, pmsize);
    if (ret) {
	PDEBUG2("PM: Error in wait_lmt(), ret = %d.\n", ret);
	goto out;
    }

    PDEBUG("PM: Acquiring into PM buffer...\n");
    /* Write command to CB fifo (initiate data transfer) */
    ret = libera_dd_write_CBfifo(get_circ_offset_lmt(&LMTstart), 
				 pmsize, 1);
    if (ret < 0) {
	PDEBUG("PM: Error writing to CB FIFO (%d).\n", ret);
	goto out;
    }
    
    /* Wait for DD interrupt */
    PDEBUG("PM: waiting for DD interrupt...\n");
    ret = getBlockedFromFIFO(&dd->dd_irqevents,
			     &dd->DD_queue,
			     &dd_dummy);
    if (ret) goto out;
    
    /* And now begin the data transfer */
    PDEBUG("PM: DD transfer...\n");
    ret = libera_dd_read_OBfifo_DMA((char *)pm->buf, pmsize);
    
    /* Re-enable DD interrupt */
    PDEBUG("PM: Re-enabling DD interrupt...\n");
    set_irqMask((get_irqMask() | LIBERA_INTERRUPT_DD_MASK));
    
    /* Check for errors */
    if (ret < 0) {
	PDEBUG("PM: Error reading OB FIFO (%d).\n", ret);
	goto out;
    }
    if (ret < pmsize) {
	PDEBUG("PM: Incomplete PM data (%d atoms).\n", ret);
	ret = -EIO;
	goto out;
    }

    /* Normal acquisition */
    ret = 0;
    
 out:
    mutex_unlock(&dd->sem);
    return ret;
}
