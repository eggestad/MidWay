/*
  The MidWay API
  Copyright (C) 2000, 2002 Terje Eggestad

  The MidWay API is free software; you can redistribute it and/or
  modify it under the terms of the GNU Library General Public License as
  published by the Free Software Foundation; either version 2 of the
  License, or (at your option) any later version.
  
  The MidWay API is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Library General Public License for more details.
  
  You should have received a copy of the GNU Library General Public
  License along with the MidWay distribution; see the file COPYING. If not,
  write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA. 
*/

/* this is the MidWay.h for internal MidWay use. We add on config.h ++ */

#ifndef _MIDWAY_INTERNAL_H
#define _MIDWAY_INTERNAL_H

#include "../../include/MidWay.h"

#include "../../config.h"

/* gcc hack in order to avoid unused warnings (-Wunused) on cvstags */
#ifdef GNU_C
#define UNUSED __attribute__ ((unused))
#else
#define UNUSED 
#endif

#endif
