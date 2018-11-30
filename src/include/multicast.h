/*
  The MidWay API
  Copyright (C) 2001 Terje Eggestad

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



#ifndef _MULTICAST_H

#include <MidWay.h>
#include <sys/socket.h>
#include <sys/types.h>

#define _MULTICAST_H


#define MCASTADDRESS_V4 "225.0.0.100" 


int _mw_setmcastaddr(void);
int _mw_initmcast(int s);
int _mw_sendmcast (int s, char * payload);
int _mw_getmcastreply(int s, instanceinfo * reply, float timeout);
int _mw_sendmcastquery(int s, const char * domain, const char * instance);

#endif /* _MULTICAST_H */
