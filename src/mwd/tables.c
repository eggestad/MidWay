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
 * Revision 1.19  2004/04/12 23:02:29  eggestad
 * - added missing server name to server table
 *
 * Revision 1.18  2004/03/20 18:57:47  eggestad
 * - Added events for SRB clients and proppagation via the gateways
 * - added a mwevent client for sending and subscribing/watching events
 * - fix some residial bugs for new mwfetch() api
 *
 * Revision 1.17  2004/02/19 23:44:09  eggestad
 * adding debug messages for msgctl(RM)
 *
 * Revision 1.16  2003/06/12 07:33:15  eggestad
 *  numerous fixes to check_tables()
 *
 * Revision 1.15  2003/04/25 13:03:11  eggestad
 * - fix for new task API
 * - new shutdown procedure, now using a task
 *
 * Revision 1.14  2003/03/16 23:53:53  eggestad
 * bug fixes
 *
 * Revision 1.13  2002/10/22 21:50:50  eggestad
 * addresses in ipctables are now strings and not struct sockaddr_*
 *
 * Revision 1.12  2002/10/03 21:14:04  eggestad
 * - cost field in provide was ignored, now correctly done
 *
 * Revision 1.11  2002/09/29 17:39:50  eggestad
 * improved the _mw_get[client|server|service|gateway]entry functions and removed duplicates in mwd.c
 *
 * Revision 1.10  2002/09/22 23:01:16  eggestad
 * fixup policy on *ID's. All ids has the mask bit set, and purified the consept of index (new macros) that has the mask bit cleared.
 *
 * Revision 1.9  2002/09/05 23:19:45  eggestad
 * smgrTask() shall not try to start servers in unclean system state
 *
 * Revision 1.8  2002/09/04 07:13:31  eggestad
 * mwd now sends an event on service (un)provide
 *
 * Revision 1.7  2002/08/09 20:50:16  eggestad
 * A Major update for implemetation of events and Task API
 *
 * Revision 1.6  2002/07/07 22:45:48  eggestad
 * *** empty log message ***
 *
 * Revision 1.5  2002/02/17 17:56:20  eggestad
 * *** empty log message ***
 *
 * Revision 1.4  2001/10/03 22:41:05  eggestad
 * added a TODO marker
 *
 * Revision 1.3  2001/09/15 23:59:05  eggestad
 * Proper includes and other clean compile fixes
 *
 * Revision 1.2  2000/07/20 19:53:32  eggestad
 * - ID numbers are no longer assigned the first from 0 but in rotation,
 *   like Unix pid.
 * - Major changes to client table to accommodate SRB clients.
 * - prototype fixup.
 *
 * Revision 1.1.1.1  2000/03/21 21:04:29  eggestad
 * Initial Release
 *
 * Revision 1.1.1.1  2000/01/16 23:20:12  terje
 * MidWay
 *
 */

#include <errno.h>
#include <signal.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <string.h>
#include <stdlib.h>

#define _TABLES_C
#include <MidWay.h>
#include <osdep.h>
#include <ipcmessages.h>
#include <internal-events.h>
#include "tables.h"
#include "mwd.h"
#include "events.h"

cliententry  * clttbl  = NULL;
serverentry  * srvtbl  = NULL;
serviceentry * svctbl  = NULL;
gatewayentry * gwtbl   = NULL;
conv_entry   * convtbl = NULL;

static char * RCSId UNUSED = "$Id$";

/* we fill up the necessary fileds in the tables with
 * UNASSIGNED to "empty" them
 * silently returns if ipcmain is not attached with shmat().
 */
void
init_tables()
{
  int i;

  if (ipcmain == NULL) {
    Error("No ipcmain table available");
    return ;
  };
  for (i=0; i < ipcmain->clttbl_length; i++) {
    clttbl[i].type = UNASSIGNED;
    clttbl[i].mqid = UNASSIGNED;
    clttbl[i].pid = UNASSIGNED;
    clttbl[i].status = UNASSIGNED;
    memset(clttbl[i].clientname, '\0', MWMAXNAMELEN);
    memset(clttbl[i].username, '\0', MWMAXNAMELEN);
    memset(clttbl[i].addr_string, '\0', MWMAXNAMELEN);

  }
  for (i=0; i < ipcmain->srvtbl_length; i++) {
    srvtbl[i].mqid = UNASSIGNED;
    srvtbl[i].pid = UNASSIGNED;
    srvtbl[i].status = UNASSIGNED;
    memset(srvtbl[i].servername, '\0', MWMAXNAMELEN);
  }
  for (i=0; i < ipcmain->svctbl_length; i++) {
    svctbl[i].type = UNASSIGNED;
    svctbl[i].server = UNASSIGNED;
    svctbl[i].gateway = UNASSIGNED;
    memset(svctbl[i].servicename, '\0', MWMAXSVCNAME);
    memset(svctbl[i].mwname, '\0', MWMAXNAMELEN);
  }
  for (i=0; i < ipcmain->gwtbl_length; i++) {
    gwtbl[i].status = UNASSIGNED;
    gwtbl[i].mqid = UNASSIGNED;
    gwtbl[i].pid = UNASSIGNED;
    memset(gwtbl[i].instancename, '\0', MWMAXNAMELEN);
    memset(gwtbl[i].domainname, '\0', MWMAXNAMELEN);
    memset(gwtbl[i].addr_string, '\0', MWMAXNAMELEN);
  }
  ipcmain->gwtbl_nextidx = 0;
  for (i=0; i < ipcmain->convtbl_length; i++) {
    convtbl[i].srvid = UNASSIGNED;
    convtbl[i].cltid = UNASSIGNED;
    convtbl[i].gwid = UNASSIGNED;
  }
};

void term_tables()
{
  shmdt(clttbl);
  clttbl = NULL;

  shmdt(srvtbl);
  srvtbl = NULL;

  shmdt(svctbl);
  svctbl = NULL;

  shmdt(gwtbl);
  gwtbl = NULL;

  shmdt(convtbl);
  convtbl = NULL;

  shmctl(ipcmain->clttbl_ipcid, IPC_RMID, NULL);
  shmctl(ipcmain->srvtbl_ipcid, IPC_RMID, NULL);
  shmctl(ipcmain->svctbl_ipcid, IPC_RMID, NULL);
  shmctl(ipcmain->gwtbl_ipcid, IPC_RMID, NULL);
  shmctl(ipcmain->convtbl_ipcid, IPC_RMID, NULL);
};


/**********************************************************************
 *
 * Functions dealing with servers.
 *
 **********************************************************************/

SERVERID addserver(char * name, int mqid, pid_t pid)
{
  ipcmaininfo * ipcmain;
  serverentry * srvtbl;
  int srvidx = UNASSIGNED, i;
  static int nextidx = 0;

  DEBUG("addserver(name=\"%s\", mqid=%d, pid=%d",
	name, mqid, pid);

  ipcmain = getipcmaintable();
  if (ipcmain == NULL) {
    Error("No ipcmain table available");
    return -EUCLEAN;
  };
  srvtbl = _mw_getserverentry(0);
  DEBUG("address of server table is %p %p", srvtbl, srvtbl);

  for (i = nextidx; i < ipcmain->srvtbl_length; i++) {
    DEBUG("addserver: testing %d status=%d", 
	  i, srvtbl[i].status);
    
    if (srvtbl[i].status == UNASSIGNED) {
      srvidx = i;
      nextidx = i+1;
      break;
    }
  };
  if (srvidx == UNASSIGNED) {
    for (i = 0; i < nextidx; i++) {
      DEBUG("addserver: testing %d status=%d", 
	    i, srvtbl[i].status);
      
      if (srvtbl[i].status == UNASSIGNED) {
	srvidx = i;
	nextidx = i+1;
	break;
      }
    };
  };
  if (srvidx == UNASSIGNED) {
    Error("Server table full");
    return -ENOSPC;
  };
  
  strncpy(srvtbl[srvidx].servername, name, MWMAXNAMELEN);
  srvtbl[srvidx].mqid = mqid;
  srvtbl[srvidx].status = 0;
  srvtbl[srvidx].booted = time(NULL);
  srvtbl[srvidx].lastsvccall = -1;
  srvtbl[srvidx].mwdblock = FALSE;
  srvtbl[srvidx].pid = pid;
  srvtbl[srvidx].nowserving = UNASSIGNED;
  
  srvtbl[srvidx].percbusy1 = 0;
  srvtbl[srvidx].percbusy5 = 0;
  srvtbl[srvidx].percbusy15 = 0;
  srvtbl[srvidx].count1 = 0;
  srvtbl[srvidx].count5 = 0;
  srvtbl[srvidx].count15 = 0;
  srvtbl[srvidx].qlenbusy1 = 0;
  srvtbl[srvidx].qlenbusy5 = 0;
  srvtbl[srvidx].qlenbusy15 = 0;
  srvtbl[srvidx].avserv1 = 0;
  srvtbl[srvidx].avserv5 = 0;
  srvtbl[srvidx].avserv15 = 0;
  srvtbl[srvidx].avwait1 = 0;
  srvtbl[srvidx].avwait5 = 0;
  srvtbl[srvidx].avwait15 = 0;
  
  DEBUG("addserver: added server %s id = %d adr %p mqid = %d pid = %d", 
	name, srvidx, &srvtbl[srvidx], mqid, pid);
  srvidx |= MWSERVERMASK;
  
  return srvidx;
};

int delserver(SERVERID sid)
{
  ipcmaininfo * ipcmain = NULL;
  serverentry * srvtbl = NULL;
  int srvidx;

  ipcmain = getipcmaintable();
  if (ipcmain == NULL) {
    Error("No ipcmain table available");
    return -EUCLEAN;
  };

  event_clear_id(sid);

  srvtbl = _mw_getserverentry(0);
  srvidx = SRVID2IDX(sid); 

  srvtbl[srvidx].status = UNASSIGNED;
  srvtbl[srvidx].mqid = UNASSIGNED;
  srvtbl[srvidx].pid = UNASSIGNED;
  srvtbl[srvidx].servername[0] = '\0';
  return 0;
};



/**********************************************************************
 *
 * Functions dealing with clients.
 *
 **********************************************************************/

CLIENTID addclient(int type, char * name, int mqid, pid_t pid, int gwid)
{
  ipcmaininfo * ipcmain = NULL;
  cliententry * clttbl = NULL;
  int cltidx = UNASSIGNED, i;
  static int nextidx = 0;

  DEBUG("(type=%d, name=\"%s\", mqid=%d, pid=%d, gwid=%d",
	type, name, mqid, pid, gwid);
  
  ipcmain = getipcmaintable();
  if (ipcmain == NULL) {
    Error("No ipcmain table available");
    return -EUCLEAN;
  };
  clttbl = _mw_getcliententry(0);    
  DEBUG("address of client table is %p %p", clttbl, clttbl);

  /* 
   * foreach entry in the client table, find the first available,
   * starting with the last id issued.  
   */
  for (i = nextidx; i < ipcmain->clttbl_length; i++) {
    DEBUG("addclient: testing %d if status=%d==%d type=%d==%d", 
	  i, clttbl[i].status, UNASSIGNED, clttbl[i].type, UNASSIGNED);

    if (clttbl[i].status == UNASSIGNED) {
      cltidx = i;
      nextidx = i+1;
      break;
    };
  };
  if (cltidx == UNASSIGNED) {
    for (i = 0; i < nextidx; i++) {
      DEBUG("addclient: testing %d if status=%d==%d type=%d==%d", 
	    i, clttbl[i].status, UNASSIGNED, clttbl[i].type, UNASSIGNED);
      
      if (clttbl[i].status == UNASSIGNED) {
	cltidx = i;
	nextidx = i+1;
	break;
      };
    };
  }
  if (cltidx == UNASSIGNED) {  
    /* if for loop completed, we have a full table.*/
    Error("Client table full");
    return -ENOSPC;
  }
  
  /* 
   * We have an available, set all the params.
   */
  switch (type) {
  case MWIPCCLIENT:
  case MWIPCSERVER:
    clttbl[cltidx].location = GWLOCAL ;
    clttbl[cltidx].gwid = UNASSIGNED ;
    DEBUG("added client %s to index %#x adr %p mqid = %d pid = %d", 
	  name, cltidx, &clttbl[cltidx], mqid, pid);
    
    break;
  case MWNETCLIENT:
    clttbl[cltidx].location = GWCLIENT ;
    clttbl[cltidx].gwid = gwid ;
    DEBUG("added client %s to index %#x  gatewayid = %d pid = %d", 
	  name, cltidx, gwid, pid);
    
    break;
  default:
    Warning("Got a request to add a client of unknown type (%#x)", type);
    return -ENOSYS;
  }
  clttbl[cltidx].type     = type;      
  clttbl[cltidx].mqid     = mqid;
  clttbl[cltidx].pid      = pid;
  
  strncpy(clttbl[cltidx].clientname, name, MWMAXNAMELEN) ;
  clttbl[cltidx].authtype = UNASSIGNED;
  
  clttbl[cltidx].status = 0;
  clttbl[cltidx].connected = time(NULL);
  clttbl[cltidx].requests = 0;
  
  cltidx |= MWCLIENTMASK;
  return cltidx;
};

  
/*
 * used when deleting a server to ensure that all service entries 
 * belonging to this server are deleted.
 * Returns NULL when there are no more services.
 */

/*
static serviceentry * get_service_bysrv(SERVERID srvid)
{
  ipcmaininfo * ipcmain;
  serviceentry * svctbl;
  int i;

  ipcmain = getipcmaintable();
  if (ipcmain == NULL) {
    Error("No ipcmain table available");
    return NULL;
  };
  svctbl = getserviceentry(0);
  for (i = 0; i < ipcmain->svctbl_length; i++) {
    if ( (svctbl[i].type != UNASSIGNED) &&
	 (svctbl[i].server == srvid) ) {
      return & svctbl[i];
    };
  }
  return NULL;
}
*/

int delclient(CLIENTID cid)
{
  cliententry * clttbl = NULL;
  int cltidx;

  if (ipcmain == NULL) {
    Error("No ipcmain table available");
    return -EUCLEAN;
  };

  event_clear_id(cid);
  event_unsubscribe(UNASSIGNED, cid);

  clttbl = _mw_getcliententry(0);
  cltidx = CLTID2IDX(cid);
  
  clttbl[cltidx].type = UNASSIGNED;
  clttbl[cltidx].mqid = UNASSIGNED;
  clttbl[cltidx].pid  = UNASSIGNED;
  clttbl[cltidx].status = UNASSIGNED;
  clttbl[cltidx].clientname[0] = '\0';
  clttbl[cltidx].username[0] = '\0';
  clttbl[cltidx].addr_string[0] = '\0';
  return 0;
};


/**********************************************************************
 *
 * Functions dealing with services.
 *
 **********************************************************************/
static SERVICEID addservice(char * name, int type)
{
  serviceentry * svctbl;
  int i, svcidx;
  static int nextidx = 0;

  /* we go thru the service table starting with the nest after the
     last one we returned to find the first available entry.*/
  svctbl = _mw_getserviceentry(0);
  for (i = 0; i < ipcmain->svctbl_length; i++) {
    svcidx = (nextidx + i) % ipcmain->svctbl_length;
    if (svctbl[svcidx].type == UNASSIGNED) {
      nextidx = svcidx+1;
      break;
    };
  };
  
  if (svctbl[svcidx].type != UNASSIGNED) {  
    /* if for loop completed, we have a full table.*/
    Error("Service table full");
    return -ENOSPC;
  };
  strncpy(svctbl[svcidx].servicename, name, MWMAXSVCNAME);
  svctbl[svcidx].type = type;
  DEBUG("Added service %s as %d for nextid=%d", 
	name, svcidx, nextidx);
 
  return svcidx;
};
  
SERVICEID addlocalservice(SERVERID srvid, char * name, int type)
{
  int svcidx;
  serviceentry * svctbl;
  serverentry * srvent;
  mwprovideevent evdata;

  /* We skip test on ipcmain eq to NULL, that really can't happen */

  if (SRVID(srvid) == UNASSIGNED) return UNASSIGNED;

  srvent = _mw_getserverentry(srvid);
  if (srvent == NULL) return UNASSIGNED;

  if (srvent->status == UNASSIGNED) {
    Error("got a request to assign a service to server %#x which is nott attached", srvid);
    return -EINVAL;
  };

  svctbl = _mw_getserviceentry(0);
  svcidx = addservice(name, type);
  if (svcidx < 0) return svcidx;

  svctbl[svcidx].server = srvid;
  svctbl[svcidx].location = GWLOCAL;

  strncpy(evdata.name, name, MWMAXSVCNAME);
  evdata.provider = srvid;
  evdata.svcid = IDX2SVCID(svcidx);
  internal_event_enqueue(NEWSERVICEEVENT, &evdata, sizeof(mwprovideevent), NULL, NULL);

  DEBUG2("service index = %d srvid = %x type = %d location = %d", 
	svcidx,  svctbl[svcidx].server, svctbl[svcidx].type, svctbl[svcidx].location);

  svcidx = IDX2SVCID(svcidx);
  return svcidx;
};

SERVICEID addremoteservice(GATEWAYID gwid, char * name, int cost, int type)
{
  int svcidx;
  mwprovideevent evdata;
  serviceentry * svctbl;
  gatewayentry * gwent;


  if (GWID(gwid) == UNASSIGNED) return UNASSIGNED;

  /* We skip test on ipcmain eq to NULL, that really can't happen */

  gwent = _mw_getgatewayentry(gwid);
  if (gwent->status == UNASSIGNED) {
    Error("got a request to assign a service to gateway %#x which is not attached", gwid);
    return -EINVAL;
  };
  
  svctbl = _mw_getserviceentry(0);
  svcidx = addservice(name, type);
  if (svcidx < 0) return svcidx;

  svctbl[svcidx].gateway = gwid;
  svctbl[svcidx].location = gwent->location;
  svctbl[svcidx].cost = cost;

  strncpy(evdata.name, name, MWMAXSVCNAME);
  evdata.provider = gwid;
  evdata.svcid = IDX2SVCID(svcidx);
  internal_event_enqueue(NEWSERVICEEVENT, &evdata, sizeof(mwprovideevent), NULL, NULL);

  DEBUG2("service index = %d gwid = %x type = %d location = %d", 
	svcidx,  svctbl[svcidx].gateway, svctbl[svcidx].type, svctbl[svcidx].location);
  
  svcidx = IDX2SVCID(svcidx);
  return svcidx;
};

int delservice(SERVICEID svcid)
{
  int idx;
  mwprovideevent evdata;
  serviceentry * svctbl = NULL;
  ipcmaininfo * ipcmain = NULL;
  ipcmain = getipcmaintable();

  if (SVCID(svcid) == UNASSIGNED) return UNASSIGNED;

  svctbl = _mw_getserviceentry(0);
  idx = SVCID2IDX(svcid);

  evdata.svcid  = idx;
  strncpy(evdata.name, svctbl[idx].servicename, MWMAXSVCNAME);
  if (svctbl[idx].server != UNASSIGNED)
    evdata.provider = svctbl[idx].server;
  else if (svctbl[idx].gateway) 
    evdata.provider = svctbl[idx].gateway;
  else {
    Error("delservice on a service with both serverid and gatewayid == UNASSIGNED");
    evdata.provider = UNASSIGNED;
  };
  internal_event_enqueue(DELSERVICEEVENT, &evdata, sizeof(mwprovideevent), NULL, NULL);

  DEBUG("Deleting service %#x, name = %s, on server %#x or gateway %#x",
	svcid, svctbl[idx].servicename, svctbl[idx].server, svctbl[idx].gateway);
  svctbl[idx].type = UNASSIGNED;
  svctbl[idx].server = UNASSIGNED;
  svctbl[idx].gateway = UNASSIGNED;
  memset(svctbl[idx].servicename, '\0', MWMAXSVCNAME);
  memset(svctbl[idx].mwname, '\0', MWMAXNAMELEN);

  return 0;
};

/* general clean up used by do_detach() all services belonging to a 
   given server are to be deleted. */

int delallservices(SERVERID srvid)
{
  ipcmaininfo * ipcmain = NULL;
  serviceentry * svctbl = NULL;
  int idx, n = 0;

  ipcmain = getipcmaintable();
  if (ipcmain == NULL) return -1;
  svctbl = _mw_getserviceentry(0); 


  for (idx = 0; idx < ipcmain->svctbl_length; idx++) {
    if (svctbl[idx].server ==  srvid) {
      delservice(IDX2SVCID(idx));
      n++;
    };
  }

  return n;
};

/* help procedure to find all teh pids to all the members. */
int get_pids(int * npids, int ** plist)
{
  int n, * pidlist, i;
  if ( (npids == NULL) || (plist == NULL) ) return -EINVAL;

  if (ipcmain == NULL) return -ENOTCONN;
  n = ipcmain->clttbl_length + ipcmain->srvtbl_length + ipcmain->gwtbl_length;
  * plist = (int *) malloc(sizeof(pid_t) * n);
  
  pidlist = * plist;
  n = 0;

  /* How should we handle clients? We are quite simply removing their message queues below, but 
   * that only works for clients I have the right to delete for. 
   It is definitely wrong to kill them, their being clients may be a side show and may
   try a auto reattach.

   if (clttbl != NULL) {
   for (i = 0; i< ipcmain->clttbl_length; i++) 
   if (clttbl[i].pid > 1) 
   pidlist[n++] = clttbl[i].pid;
   };
  */
  if (srvtbl != NULL) {
     DEBUG2("getting pids from the srvtbl");
     for (i = 0; i< ipcmain->srvtbl_length; i++) 
	if ( (srvtbl[i].pid > 1)  && (srvtbl[i].pid != ipcmain->mwdpid) ) {
	   DEBUG2(" pid %d = %d", n, srvtbl[i].pid);
	   pidlist[n++] = srvtbl[i].pid;
	};
  };
  if (gwtbl != NULL) {
     DEBUG2("getting pids from the gwtbl");
     for (i = 0; i< ipcmain->gwtbl_length; i++) 
	if (gwtbl[i].pid > 1) {
	   DEBUG2(" pid %d = %d", n, srvtbl[i].pid);
	   pidlist[n++] = gwtbl[i].pid;
	};
  };

  *npids = n;

  return 0;
};

/* this functions are meant to give a shutdown signal to a specific server, but can do all
   Currently we give the shutdown command by removing the servers message queue. 

   Not matter what, this function do not get ack from the server that the shutdown is OK.
*/
int stop_server(SERVERID sid)
{
   int all = 0;
   int count = 0;
   int i;
   /* can't happen but saves us core dumps. */
   if (ipcmain == NULL) return -ENOTCONN;
   
   /* if sid = -1, then do everybody */
   if (sid == -1) all = 1;
   sid &= MWINDEXMASK;
   if (srvtbl != NULL) {
      for (i = 0; i< ipcmain->srvtbl_length; i++) {
	 if (srvtbl[i].mqid <= 0) continue; // if mqid is 0 or neg, its down
	 if (srvtbl[i].mqid == ipcmain->mwd_mqid) continue; // mwd's servers are left out
	 if ( all || (sid == i) ) {
	    Info("Commanding server %d to stop.", i);
	    srvtbl[i].mwdblock = TRUE;
	    /* if not all, redistribute the remainding requests already in the queue.*/
	    msgctl(srvtbl[i].mqid, IPC_RMID, NULL);
	    count ++;
	 };
      };
   };
   return count;
};
/* procedure for killing all members are:
   1 - create an array, pidlist, long enough to hold all passible pids, sum(ipcmain->???tbl_length)
   2 - if the client table exists, go thru it and add them to pidlist
   3 - ditto for the other tables.
   4 - go thru and send kill -15, kill -1 kill -3 kill -9 with 10 sec interval.
   5 - go thru tables one again, and check to see id their message queues still existsint 
*/


int kill_all_servers(int signal)
{
  int n, i, rc; 
  pid_t * pidlist;
  if (ipcmain == NULL) return -ENOTCONN;

  /* 1 - create kill list (array). */
  /* 2, and 3 collect pids */
  
  /* 4 kills */

  /* get the currect list of server to kill */
  rc = get_pids(&n, & pidlist);
  DEBUG("There are %d servers to kill", n);

  if (rc < 0) return rc;
  
  for (i = 0; i < n; i++) {
     char cmd[1024];
     if (pidlist[i]> 0) {
	if (*_mwgetloglevel() > MWLOG_INFO) {
	   sprintf (cmd, "ps -f -p %d",  pidlist[i]);
	   system(cmd);
	};
	rc = kill (pidlist[i], signal);
     } else {
	Error("Was about to do kill(%d, %d) illegal PID", pidlist[i], signal);
     };

     Info("Did a kill(pid=%d, sig=%d)", pidlist[i], signal);
     if (rc == 0) 
	Info("The process %d refused to die!!!", pidlist[i]);
     else 
	pidlist[i] = -1;
  };

  free( pidlist);
  return n;
};

void hard_disconnect_ipc(void)
{
   int i;
  /* 5 deleting msg queues */
  /* I'm not even sure I *should* try to do this to the clients. */
  if (clttbl != NULL) {
    for (i = 0; i< ipcmain->clttbl_length; i++) 
       if (clttbl[i].mqid > 1) {
	  DEBUG("removing mqueue for CLT %x", IDX2CLTID(i));
	  msgctl(clttbl[i].mqid, IPC_RMID, NULL);
       };
  };

  /* hmmmm, well we're just  have to see about this when we make gateways.*/
  if (gwtbl != NULL) {
    for (i = 0; i< ipcmain->gwtbl_length; i++) 
       if (gwtbl[i].mqid > 1) {
	  DEBUG("removing mqueue for GW %x", IDX2GWID(i));
	  msgctl(gwtbl[i].mqid, IPC_RMID, NULL);
       };
  };
};


/* go thru all the tables and clean up after those that  died without telling us. 
   used by the watchdog
*/
int check_tables()
{ 
  int i,j, rc;

  if (clttbl != NULL) {
    for (i = 0; i< ipcmain->clttbl_length; i++) 
      if (clttbl[i].pid > 1) {
	rc = _mw_procowner (clttbl[i].pid, NULL);
	if (rc == 0) {
	  Info("Client %d pid=%d has died, cleaning up", i, clttbl[i].pid);
	  DEBUG("removing clients msgqueue id=%d", clttbl[i].mqid);
	  msgctl(clttbl[i].mqid, IPC_RMID, NULL);
	  rc = delclient(MWCLIENTMASK | i);
	};
      }
  };
  if (srvtbl != NULL) {
    for (i = 0; i< ipcmain->srvtbl_length; i++) 
      if (srvtbl[i].pid > 1) {
	rc = kill (srvtbl[i].pid, 0);
	if (rc == -1) {
	  Info("Server %d pid=%d has died, cleaning up", i, srvtbl[i].pid);
	  DEBUG("removing servers msgqueue id=%d", srvtbl[i].mqid);
	  msgctl(srvtbl[i].mqid, IPC_RMID, NULL);
	  delserver(MWSERVERMASK | i); 
	  delallservices(IDX2SRVID(i));
	};
      }
  };

  if (gwtbl != NULL) {
    for (i = 0; i< ipcmain->gwtbl_length; i++) 
      if (gwtbl[i].pid > 1) {
	rc = kill (gwtbl[i].pid, 0);
	if (rc == -1) {
	  Info("gateway %d pid=%d has died, cleaning up", i, gwtbl[i].pid);
	  DEBUG("removing gateways msgqueue id=%d", gwtbl[i].mqid);
	  msgctl(gwtbl[i].mqid, IPC_RMID, NULL);
	  /* clean up all the network clients handled by this gw */
	  for (j = 0; j < ipcmain->clttbl_length; j++) {
	    if (clttbl[j].gwid == IDX2GWID(i))
	      delclient(IDX2CLTID(j));
	  };

	  /* TODO clean up all imported services */

	  gwtbl[i].pid = UNASSIGNED;
	  gwtbl[i].srbrole = UNASSIGNED;
	  gwtbl[i].location = UNASSIGNED;
	  gwtbl[i].mqid = UNASSIGNED;
	  gwtbl[i].status = UNASSIGNED;
	};
      }
  };

  if (svctbl != NULL) {
     for (i = 0; i< ipcmain->svctbl_length; i++) {
	if (svctbl[i].type == UNASSIGNED) continue;
	if ( (svctbl[i].type == GWLOCAL) && (svctbl[i].server > 0) ) {
	   int si;
	   si = SRVID2IDX(svctbl[i].server);
	   if (srvtbl[si].pid == UNASSIGNED) {
	      Info("Service %d without server, cleaned up", i);
	      rc = delservice(IDX2SVCID(i));
	      if (rc) Warning ("Failed to delete service %d", i);
	   };
	} else if (svctbl[i].gateway > 0) {
	   int gi;
	   gi = GWID2IDX(svctbl[i].gateway);
	   if (gwtbl[gi].pid == UNASSIGNED) {
	      Info("Service %d without gateway, cleaned up", i);
	      rc = delservice(IDX2SVCID(i));
	      if (rc) Warning ("Failed to delete service %d", i);
	   }
	}
     }
  };

  /* conv table missing */


  return 0;
};
