/*
  MidWay
  Copyright (C) 2000 Terje Eggestad

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

#ifndef _SRBEVENT_H
#define _SRBEVENT_H

#include <urlencode.h>

#include <MidWay.h>
#include <connection.h>
#include <ipcmessages.h>

#endif

int srb_subscribe(Connection * conn, char * pattern, int id, int flags);
int srb_unsubscribe(Connection * conn, int id);
void srb_unsubscribe_all(Connection * conn);
void do_srb_event_dispatch(Event * ev);

