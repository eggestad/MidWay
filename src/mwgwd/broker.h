
/*
  MidWay
  Copyright (C) 2001,2005 Terje Eggestad

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

/* $Id$ */

/* 
 * $Log$
 * Revision 1.2  2005/10/12 22:46:27  eggestad
 * Initial large data patch
 *
 * Revision 1.1  2001/09/15 23:40:09  eggestad
 * added the broker daemon
 *
 */

#ifndef _BROKER_H
#define _BROKER_H

int connectbroker(char * domain, char * instance);
int read_with_fd(int s, char * message, int len, int * fd);

#endif
