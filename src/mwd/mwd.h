/*
  MidWay
  Copyright (C) 2000 Terje Eggestad

  MidWay is free software; you can redistribute it and/or
  modify it under the terms of the GNU  General Public License as
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
 * $Id$
 * $Name$
 * 
 * $Log$
 * Revision 1.2  2000/07/20 19:44:00  eggestad
 * prototype fixup.
 *
 * Revision 1.1.1.1  2000/03/21 21:04:24  eggestad
 * Initial Release
 *
 * Revision 1.1.1.1  2000/01/16 23:20:12  terje
 * MidWay
 *
 */

typedef struct {
  /* signal flags */
  int terminate ;
  int childdied;
  int alarm;
  int user1;
  int user2;
} Flags;

#ifndef _MWD_C
extern Flags flags;
extern ipcmaininfo * ipcmain;
#endif

#include <MidWay.h>

void inst_sighandlers(void);

ipcmaininfo * getipcmaintable(void); 
cliententry * getcliententry(int i);
serverentry * getserverentry(int i);
serviceentry * getserviceentry(int i);
gatewayentry * getgatewayentry(int i);
conv_entry * getconv_entry(int i);

void usage(void);
