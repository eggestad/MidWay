/*
  The MidWay API
  Copyright (C) 2000 Terje Eggestad

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

#ifndef _MWSERVERAPI_H
#define _MWSERVERAPI_H

#include <MidWay.h>
#include <ipcmessages.h>

struct ServiceFuncEntry {
  SERVICEID svcid;
  int (*func)  (mwsvcinfo*);
  struct ServiceFuncEntry * next;
};

Call * _mw_server_get_callbuffer(void);
void _mw_server_set_callbuffer(Call *);

void _mw_incprovided(void);
void _mw_decprovided(void);

struct ServiceFuncEntry * _mw_pushservice(SERVICEID sid, int (*svcfunc) (mwsvcinfo*)) ;
void _mw_popservice(SERVICEID sid);
int _mw_set_deadline(Call * callmesg, mwsvcinfo * svcreqinfo);
mwsvcinfo *  _mwGetServiceRequest (int flags);
int _mwCallCServiceFunction(mwsvcinfo * svcinfo);

#endif /* _IPCTABLES_H */
