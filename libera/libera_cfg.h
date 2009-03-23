/* $Id: libera_cfg.h 2192 2008-10-07 09:13:06Z matejk $ */

//! \file libera_cfg.h
//! Libera GNU/Linux driver Configuration (CFG) device interface.

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


#ifndef _LIBERA_CFG_H_
#define _LIBERA_CFG_H_

int 
libera_cfg_cmd(struct libera_cfg_device *dev,
	       unsigned int cmd, unsigned long arg);


#endif /* _LIBERA_CFG_H_ */
