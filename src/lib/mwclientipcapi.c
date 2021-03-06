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

#include <stdio.h>
#include <errno.h>

/*#include <sys/types.h>
#include <sys/ipc.h>
*/
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>
#include <string.h>

#include <math.h>

#include <MidWay.h>
#include <ipctables.h>
#include <ipcmessages.h>
#include <mwclientapi.h>
#include <shmalloc.h>
#include <address.h>

/*
  Here we provide the "visible" library API, in the IPC only form.
  This is however limited to the client API, since the server API are 
  available in IPC only.

*/

int _mwattachipc(int type,  mwaddress_t * mwadr, const char * name, mwcred_t * cred, int flags)
{
  int rc;
  int key;

  if (!mwadr) return -EINVAL;

  key = mwadr->sysvipckey;

  DEBUG3("_mwattachipc: Attaching to IPC with key %d", key);
  rc = _mw_attach_ipc(key, type);
  if (rc != 0) return rc;
  
  DEBUG3("_mwattachipc: Sending attach request to mwd type=%d name = %s", type, name);
  return _mw_ipcsend_attach(type, name, 0);
};

int _mwdetachipc(void)
{
  ipcmaininfo * ipcmain;
  int rc;

  ipcmain = _mw_ipcmaininfo(); 
  if (ipcmain == NULL) return 0;
  
  rc = _mwsystemstate();
  DEBUG3("_mwdetachipc: system is %s in shutdown", rc?"":"not");

  if (!rc) {
    errno = 0;
    rc = _mw_ipcsend_detach(0);
  } else {
    rc = _mw_ipcsend_detach(1);
  };
  _mw_detach_ipc();
  return rc;
};
/* This conclude the administrative calls.*/




void _mw_do_ipcevent(Event * ev)
{
  int doack;
  char * dbuf = NULL;
  
  DEBUG1("We got an event %s subscription id %d", 
	 ev->event, ev->subscriptionid);
  
  doack = 0;
  if (ev->data != 0) {
    doack = 1;
    dbuf = _mwoffset2adr(ev->data, _mw_getsegment_byid(ev->datasegmentid));
    DEBUG3("ev->data %lu, addr = %p data = %s", ev->data, dbuf, dbuf); 
  }	
    
  _mw_doevent(ev->subscriptionid, ev->event, dbuf, ev->datalen);
  
  if (doack) {
    ev->mtype = EVENTACK;
    ev->senderid = _mw_get_my_mwid();
    DEBUG1("Sending event ack");
    _mw_ipc_putmessage(0, (char *)ev, sizeof(Event), 0);
  };
};

void _mw_doipcevents(void)
{
  Event ev;
  int rc;

  do {
    rc = _mw_ipc_getevent(&ev);
    if (rc == -ENOMSG) return;
    if (rc == -EMSGSIZE) continue;
    
    if (rc != 0) {
      Error ("in %s, reason %s", __FUNCTION__, strerror(-rc));
      return;
    };
    _mw_do_ipcevent(&ev);
    
  } while (rc >= 0);
};
  
/*****************************************************************
 * 
 * Here We have the API that makes a client able to talk to a server
 * Note that some of these are essetial for making a server talk to 
 * a calling client.
 * 
 * Thus a valid client can be cmplied without mwserver.c but you can't 
 * legally compile a server with out this module, even if it we 
 * attach as a server only.
 *
 *****************************************************************/

/* the usuall IPC only API, except that mwcall() is implemeted
   entierly here.
*/
int _mwacall_ipc(const char * svcname, const char * data, size_t datalen, int flags) 
{
  int rc;
  
  rc = _mwsystemstate();
  if (rc) return rc;

  return _mwacallipc (svcname, data, datalen,  flags| MWMULTIPLE, UNASSIGNED, NULL, NULL, UNASSIGNED, 0);
}

////////////////////////////////////////////////////////////////////////

void _mwipcprotosetup(mwaddress_t * mwadr)
{
   mwadr->proto.attach = _mwattachipc;
   mwadr->proto.detach = _mwdetachipc;

   mwadr->proto.acall = _mwacall_ipc;
   mwadr->proto.fetch = _mwfetchipc;
   mwadr->proto.listsvc = _mw_list_services_byglob;

   mwadr->proto.event = _mw_ipcsend_event;
   mwadr->proto.recvevents = _mw_doipcevents;
   mwadr->proto.subscribe = _mw_ipcsend_subscribe;
   mwadr->proto.unsubscribe = _mw_ipcsend_unsubscribe;
};



