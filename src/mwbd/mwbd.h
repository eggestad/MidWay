/*
  MidWay
  Copyright (C) 2001 Terje Eggestad

  MidWay is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License as
  published by the Free Software Foundation; either version 2 of the
  License, or (at your option) any later version.
  
  MidWay is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.
  
  You should have received a copy of the GNU General Public License 
  along with the MidWay distribution; see the file COPYING.  If not,
  write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA. 
*/



/* 
 * $Log$
 * Revision 1.3  2003/03/16 23:53:53  eggestad
 * bug fixes
 *
 * Revision 1.2  2002/10/17 22:08:10  eggestad
 * - we're now using the mwlog() API
 *
 * Revision 1.1  2001/09/15 23:40:09  eggestad
 * added the broker daemon
 *
 */

#ifndef _MWBD_C
extern int debugging; 
#endif 

#ifndef _MWBD_H
#define _MWBD_H

#include <stdio.h>
#include <syslog.h>

#ifndef MWMAXNAMELEN
#define MWMAXNAMELEN (64 + 1)
#endif

#ifdef DEBUG
#define UNIXSOCKETPATH "/tmp/mwbd" 
#else 
#define UNIXSOCKETPATH "/var/run/mwbd.socket"
#endif

#define TRACEFILE1     "/var/log/mwbd.trace"
#define TRACEFILE2     "/tmp/mwbd.trace"

#endif

void Exit(char *, int);
