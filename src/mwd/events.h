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
 * Revision 1.3  2003/04/25 13:03:07  eggestad
 * - fix for new task API
 * - new shutdown procedure, now using a task
 *
 * Revision 1.2  2002/09/04 07:13:31  eggestad
 * mwd now sends an event on service (un)provide
 *
 * Revision 1.1  2002/08/09 20:50:16  eggestad
 * A Major update for implemetation of events and Task API
 *
 */

#ifndef _EVENTS_C
#define _EVENTS_C

#include <MidWay.h>
#include <ipcmessages.h>

int event_subscribe(char * pattern, MWID id, int subid, int flags);
int event_unsubscribe(int subid, MWID id);
void event_clear_id(MWID id);

int event_ack(Event * evmsg) ;
int internal_event_enqueue(char * event, void * data, int datalen, char * user, char * client);

int event_enqueue(Event * evmsg);
int do_events(PTask pt);

#endif // _EVENTS_C
