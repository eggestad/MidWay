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
 * Revision 1.1  2002/09/04 07:13:31  eggestad
 * mwd now sends an event on service (un)provide
 *
 * 
 */

/*
 * there are some internal events used within an instance that passes a binary struct.
 */

/*
 * add service  and remove service events are sent by  mwd (to the gw,
 * but  anyone may receive them  ) evy time  a server do a  provide or
 * unprovide.  */

#define NEWSERVICEEVENT ".mwprovide"
#define DELSERVICEEVENT ".mwunprovide"

struct _mwprovideevent {
  char name[MWMAXSVCNAME];
  MWID provider;
  SERVICEID svcid;
};

typedef struct _mwprovideevent mwprovideevent;
