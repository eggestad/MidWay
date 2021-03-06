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
  License along with the MidWay distribution; see the file COPYING.  If not,
  write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA. 
*/


#ifndef _ADDRESS_H
#define _ADDRESS_H

#include <mwclientapi.h>

int _mwdecode_url(const char * url, mwaddress_t * mwaddr);
const char * _mw_sprintsa(struct sockaddr * sa, char * buffer);
int _mw_mkdir_asneeded(char * path);
char * _mw_makeMidWayHomePath(char * mwhome) ;
char * _mw_makeInstanceHomePath(char * mwhome, char * instancename) ;
key_t _mw_make_instance_home_and_ipckey(char *);

#endif


