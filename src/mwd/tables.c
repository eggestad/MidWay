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
static char * RCSName UNUSED = "$Name$"; /* CVS TAG */

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

  DEBUG(	"addserver(name=\"%s\", mqid=%d, pid=%d",
	name, mqid, pid);

  ipcmain = getipcmaintable();
  if (ipcmain == NULL) {
    Error("No ipcmain table available");
    return -EUCLEAN;
  };
  srvtbl = getserverentry(0);
  DEBUG("address of server table is %#X %#x", srvtbl, srvtbl);

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
  
  DEBUG(	"addserver: added server %s id = %d adr %#x mqid = %d pid = %d", 
	name, srvidx, (long)&srvtbl[srvidx], mqid, pid);
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

  event_clear_id(CLTID2MWID(sid));

  srvtbl = getserverentry(0);
  srvidx = sid&MWINDEXMASK; //MWID2SRVID(sid);

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

  DEBUG(	"addclient(type=%d, name=\"%s\", mqid=%d, pid=%d, gwid=%d",
	type, name, mqid, pid, gwid);
  
  ipcmain = getipcmaintable();
  if (ipcmain == NULL) {
    Error("No ipcmain table available");
    return -EUCLEAN;
  };
  clttbl = getcliententry(0);    
  DEBUG("address of client table is %#X %#x", clttbl, clttbl);

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
    DEBUG(	  "addclient: added client %s to index %#x adr %#x mqid = %d pid = %d", 
	  name, cltidx, (long) &clttbl[cltidx], mqid, pid);
    
    break;
  case MWNETCLIENT:
    clttbl[cltidx].location = GWCLIENT ;
    clttbl[cltidx].gwid = gwid ;
    DEBUG(	  "addclient: added client %s to index %#x  gatewayid = %d pid = %d", 
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

  event_clear_id(CLTID2MWID(cid));

  clttbl = getcliententry(0);
  cltidx = cid & MWINDEXMASK;
  
  clttbl[cltidx].type = UNASSIGNED;
  clttbl[cltidx].mqid = UNASSIGNED;
  clttbl[cltidx].pid  = UNASSIGNED;
  clttbl[cltidx].status = UNASSIGNED;
  clttbl[cltidx].clientname[0] = '\0';
  clttbl[cltidx].username[0] = '\0';
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
  svctbl = getserviceentry(0);
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

  srvent = getserverentry(srvid & MWINDEXMASK);
  if (srvent->status == UNASSIGNED) {
    Error("got a request to assign a service to server %#x which is nott attached", srvid);
    return -EINVAL;
  };

  svctbl = getserviceentry(0);
  svcidx = addservice(name, type);
  if (svcidx < 0) return svcidx;

  svctbl[svcidx].server = srvid;
  svctbl[svcidx].location = GWLOCAL;

  strncpy(evdata.name, name, MWMAXSVCNAME);
  evdata.provider = SRVID2MWID(srvid);
  evdata.svcid = svcidx;
  internal_event_enqueue(NEWSERVICEEVENT, &evdata, sizeof(mwprovideevent), NULL, NULL);

  DEBUG2("service index = %d srvid = %x type = %d location = %d", 
	svcidx,  svctbl[svcidx].server, svctbl[svcidx].type, svctbl[svcidx].location);

  svcidx = svcidx | MWSERVICEMASK;
  return svcidx;
};

SERVICEID addremoteservice(GATEWAYID gwid, char * name, int type)
{
  int svcidx;
  mwprovideevent evdata;
  serviceentry * svctbl;
  gatewayentry * gwent;

  /* We skip test on ipcmain eq to NULL, that really can't happen */

  gwent = getgatewayentry(gwid & MWINDEXMASK);
  if (gwent->status == UNASSIGNED) {
    Error("got a request to assign a service to gateway %#x which is nott attached", gwid);
    return -EINVAL;
  };
  
  svctbl = getserviceentry(0);
  svcidx = addservice(name, type);
  if (svcidx < 0) return svcidx;

  svctbl[svcidx].gateway = gwid;
  svctbl[svcidx].location = GWREMOTE;

  strncpy(evdata.name, name, MWMAXSVCNAME);
  evdata.provider = GWID2MWID(gwid);
  evdata.svcid = svcidx;
  internal_event_enqueue(NEWSERVICEEVENT, &evdata, sizeof(mwprovideevent), NULL, NULL);

  DEBUG2("service index = %d gwid = %x type = %d location = %d", 
	svcidx,  svctbl[svcidx].gateway, svctbl[svcidx].type, svctbl[svcidx].location);
  
  svcidx = svcidx | MWSERVICEMASK;
  return svcidx;
};

int delservice(SERVICEID svcid)
{
  int idx;
  mwprovideevent evdata;
  serviceentry * svctbl = NULL;
  ipcmaininfo * ipcmain = NULL;
  ipcmain = getipcmaintable();


  svctbl = getserviceentry(0);
  idx = MWID2SVCID(svcid);

  evdata.svcid  = idx;
  strncpy(evdata.name, svctbl[idx].mwname, MWMAXSVCNAME);
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
  svctbl = getserviceentry(0); 


  for (idx = 0; idx < ipcmain->svctbl_length; idx++) {
    if (svctbl[idx].server ==  srvid) {
      delservice(idx | MWSERVICEMASK);
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
    for (i = 0; i< ipcmain->srvtbl_length; i++) 
      if (srvtbl[i].pid > 1) 
	pidlist[n++] = srvtbl[i].pid;
  };
  if (gwtbl != NULL) {
    for (i = 0; i< ipcmain->gwtbl_length; i++) 
      if (gwtbl[i].pid > 1) 
	pidlist[n++] = gwtbl[i].pid;
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
    for (i = 0; i< ipcmain->srvtbl_length; i++) 
      if ( (srvtbl[i].mqid > 1) && ( all || (sid == i) )) {
	Info("Commanding server %d to stop.", i);
	srvtbl[i].mwdblock = TRUE;
	/* if not all, redistribute the remainding requests already in the queue.*/
	msgctl(srvtbl[i].mqid, IPC_RMID, NULL);
	count ++;
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

static int signallist[5] = { SIGTERM, SIGHUP, SIGQUIT, SIGKILL, 0 };

int kill_all_servers(void)
{
  int n, i, j, rc; 
  pid_t * pidlist;
  if (ipcmain == NULL) return -ENOTCONN;

  /* 1 - create kill list (array). */
  /* 2, and 3 collect pids */
  
  n = stop_server(-1); /* give the command to every server */
  sleep(1);

  /* 4 kills */
  for (j = 0; j < 5; j++) {
    /* get the currect list of server to kill */
    rc = get_pids(&n, & pidlist);
    DEBUG("There are %d servers to kill", n);

    if (rc==0) break;

    for (i = 0; i < n; i++) {
      if (pidlist[i]> 0)
	rc = kill (pidlist[i], signallist[j]);
      else 
	Error("Was about to do kill(%d, %d) illegal PID", pidlist[i], signallist[j]);
      Info("Did a kill(pid=%d, sig=%d)", pidlist[i], signallist[j]);
      if (rc == 0) 
	if (signallist[j] == 0)
	  Info("The process %d refused to die!!!", pidlist[i]);
	else
	  Info("Server pid=%d is slow to die, sending the signal %d", 
		pidlist[i], signallist[j]);
      else 
	pidlist[i] = -1;
    }
    sleep(2*j);
  };

  /* 5 deleting msg queues */
  /* I'm not even sure I *should* try to do this to the clients. */
  if (clttbl != NULL) {
    for (i = 0; i< ipcmain->clttbl_length; i++) 
      if (clttbl[i].mqid > 1) 
	msgctl(clttbl[i].mqid, IPC_RMID, NULL);
  };

  /* hmmmm, well we're just  have to see about this when we make gateways.*/
  if (gwtbl != NULL) {
    for (i = 0; i< ipcmain->gwtbl_length; i++) 
      if (gwtbl[i].mqid > 1) 
	msgctl(gwtbl[i].mqid, IPC_RMID, NULL);
  };

  free( pidlist);
  return n;
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
	  msgctl(srvtbl[i].mqid, IPC_RMID, NULL);
	  delserver(MWSERVERMASK | i); 
	  delallservices(SRVID2MWID(i));
	};
      }
  };
  if (svctbl != NULL) {
    for (i = 0; i< ipcmain->svctbl_length; i++) 
      if (svctbl[i].server > 0) {
	int si;
	si = MWINDEXMASK&svctbl[i].server;
	if (srvtbl[si].pid == UNASSIGNED) {
	  Info("Service %d without server, cleaned up", i);
	  delservice(si);
	};
      }
  };

  /* conv table missing */
  if (gwtbl != NULL) {
    for (i = 0; i< ipcmain->gwtbl_length; i++) 
      if (gwtbl[i].pid > 1) {
	if (rc == -1) {
	  Info("gateway %d pid=%d has died, cleaning up", i, gwtbl[i].pid);
	  msgctl(srvtbl[i].mqid, IPC_RMID, NULL);
	  /* clean up all the network clients handled by this gw */
	  for (j = 0; j < ipcmain->clttbl_length; j++) {
	    if (clttbl[j].gwid = i)
	      delclient(MWCLIENTMASK | j);
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

  return 0;
};
