/*
  The MidWay API
  Copyright (C) 2005 Terje Eggestad

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

/* $Id$ */

/*
 * $Log$
 * Revision 1.1  2005/10/12 22:46:27  eggestad
 * Initial large data patch
 *
 */

#ifndef _CALL_PENDING_DATA_H
#define _CALL_PENDING_DATA_H

#include <ipcmessages.h>

#define CALLFLAG 1
#define REPLYFLAG 0
#define CALLREPLYSTR(f) (f==CALLFLAG?"call":"reply")

int add_pending_call_data(char * svcname, int totaldatalen, int flags,  
		      MWID mwid, char * instance, char * domain, MWID callerid, 
		      SRBhandle_t handle, int hops);

void del_pending_call_data(MWID mwid, SRBhandle_t handle);


int add_pending_reply_data(MWID mwid, SRBhandle_t handle, Call * cmsg, int totallen); 

void del_pending_reply_data(MWID mwid, SRBhandle_t handle);

int append_pending_call_data(MWID mwid, SRBhandle_t handle, char * data, int datalen);
int append_pending_reply_data(MWID mwid, SRBhandle_t handle, char * data, int datalen);

void clear_pending_data(MWID mwid);

#endif // _CALL_PENDING_DATA_H
