
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

/*
 * $Id$
 * $Name$
 * 
 * $Log$
 * Revision 1.10  2002/09/22 22:49:23  eggestad
 * Added new function that return list of all providers of a service
 *
 * Revision 1.9  2002/09/04 07:13:31  eggestad
 * mwd now sends an event on service (un)provide
 *
 * Revision 1.8  2002/08/09 20:50:15  eggestad
 * A Major update for implemetation of events and Task API
 *
 * Revision 1.7  2002/07/07 22:35:20  eggestad
 * *** empty log message ***
 *
 * Revision 1.6  2002/02/17 14:09:58  eggestad
 * clean compile fix
 *
 * Revision 1.5  2001/09/15 23:59:05  eggestad
 * Proper includes and other clean compile fixes
 *
 * Revision 1.4  2000/09/21 18:36:36  eggestad
 * server crashed if IPC client dissapeared while service call was prosessed.
 *
 * Revision 1.3  2000/08/31 21:50:48  eggestad
 * DEBUG level set propper.
 *
 * Revision 1.2  2000/07/20 19:23:40  eggestad
 * Changes needed for SRB clients and gateways.
 *
 * Revision 1.1.1.1  2000/03/21 21:04:11  eggestad
 * Initial Release
 *
 * Revision 1.1.1.1  2000/01/16 23:20:12  terje
 * MidWay
 *
 */

#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/time.h>
#include <signal.h>

#include <MidWay.h>
#include <ipctables.h>
#include <ipcmessages.h>

static char * RCSId UNUSED = "$Id$";
static char * RCSName UNUSED = "$Name$"; /* CVS TAG */


static ipcmaininfo  * ipcmain = NULL; 
static cliententry  * clttbl  = NULL;
static serverentry  * srvtbl  = NULL;
static serviceentry * svctbl  = NULL;
static gatewayentry * gwtbl   = NULL;
static conv_entry   * convtbl = NULL;

static int my_mqid = -1;

static SERVERID  myserverid  = UNASSIGNED;
static CLIENTID  myclientid  = UNASSIGNED;
static GATEWAYID mygatewayid = UNASSIGNED;


/* I don't want tp make ipcmain a global var, and _mw_attach_ipc are
   never called in mwd, thus this function are for mwd only */

void _mw_set_shmadr (ipcmaininfo * im, cliententry * clt, serverentry * srv, 
		     serviceentry * svc, gatewayentry * gw, conv_entry * conv)
{
  ipcmain = im ;
  clttbl  = clt;  
  srvtbl  = srv;
  svctbl  = svc;
  gwtbl   = gw;
  convtbl = conv;
};

int _mw_attach_ipc(key_t key, int type)
{
  int mainid, readonly;
  int rc;

  extern struct segmenthdr * _mwHeapInfo;
  /* already connected,  */
  if (ipcmain != NULL) {
    Warning(	  "Attempted to attach shm segments while already attached");
    return -EISCONN; 
  };

  /* if we're a clients we shall attach all the shared memory tables readonly */
  if (type & MWIPCSERVER)  readonly = 0;
  else readonly = SHM_RDONLY;

  DEBUG1("_mw_attachipc(key=%d, type=%s(%d)) readonly=%d", 
	key, type==MWIPCSERVER?"SERVER":"CLIENT", type, readonly);

  /* THREAD MUTEX BEGIN */
  /* NOTE: below the returns in the if(error) do not release the MUTEX */


  mainid = shmget(key, 0, 0);
  if (mainid == -1) {
    Error("There are no shared memory for key = 0x%x reason %s",
	  key, strerror(errno));
    return -errno;
  };
  DEBUG1("main shm info table has id %d",mainid);

  ipcmain = (ipcmaininfo *) shmat(mainid, (void *) 0, readonly);
  if ( (ipcmain == NULL) || (ipcmain == (void *) -1) ) {
    Error("Failed to attach shared memory with id = %d reason %s",
	  mainid, strerror(errno));
    ipcmain = NULL;
    /* MUTEX END */
    return -errno;
  };
  DEBUG1("main shm info table attached at 0x%x",ipcmain);


  /* attaching all the other tables */

  clttbl = shmat(ipcmain->clttbl_ipcid, NULL, readonly);
  if (clttbl == (void *) -1) {
    Error("Failed to attach client table with id = %d reason %s",
	  ipcmain->clttbl_ipcid, strerror(errno));
    return -errno;
  };
  DEBUG1("client table attached at 0x%x 0x%x ",clttbl, rc);
  
  srvtbl = shmat(ipcmain->srvtbl_ipcid, NULL, readonly);
  if (srvtbl == (void *) -1) {
    Error("Failed to attach Server table with id = %d reason %s",
	  ipcmain->srvtbl_ipcid, strerror(errno));
    return -errno;
  };
  DEBUG1("Server table attached at 0x%x",srvtbl);
  
  svctbl = shmat(ipcmain->svctbl_ipcid, NULL, readonly);
  if (svctbl == (void *) -1) {
    Error("Failed to attach service table with id = %d reason %s",
	  ipcmain->svctbl_ipcid, strerror(errno));
    return -errno;
  };
  DEBUG1("service table attached at 0x%x",svctbl);
  
  gwtbl = shmat(ipcmain->gwtbl_ipcid, NULL, readonly);
  if (gwtbl == (void *) -1) {
    Error("Failed to attach gateway table with id = %d reason %s",
	  ipcmain->gwtbl_ipcid, strerror(errno));
    return -errno;
  };
  DEBUG1("gateway table attached at 0x%x",gwtbl);
  
  convtbl = shmat(ipcmain->convtbl_ipcid, NULL, readonly);
  if (convtbl == (void *) -1) {
    Error("Failed to attach convserver table with id = %d reason %s",
	  ipcmain->convtbl_ipcid, strerror(errno));
    return -errno;
  };
  DEBUG1("convserver table attached at 0x%x",convtbl);

  my_mqid = msgget(IPC_PRIVATE, 0622); /* perm = rw--w--w- */ 
  if (my_mqid == -1) {
    Error("Failed to create an ipc queue for this process reason=%d",
	  errno);
    return -errno;
  };
  DEBUG1("My request/reply queue has IPC id %d", my_mqid );

  /* Note flag, the heap is never readonly. */
  _mwHeapInfo = shmat (ipcmain->heap_ipcid, NULL, 0);
  if (_mwHeapInfo != NULL) {
    DEBUG1("heap attached");
  };
  /* THREAD MUTEX ENDS */
  return 0;
};

#include <stdio.h>

void
_mw_detach_ipc(void)
{
  extern struct segmenthdr * _mwHeapInfo;
  
  shmdt(clttbl);
  shmdt(srvtbl);
  shmdt(svctbl);
  shmdt(gwtbl);
  shmdt(convtbl);

  shmdt(_mwHeapInfo);
  _mwHeapInfo = NULL;
  
  shmdt(ipcmain);
  ipcmain = NULL;
  myserverid = UNASSIGNED;
  myclientid= UNASSIGNED;

  msgctl(my_mqid, IPC_RMID, NULL);
  my_mqid = UNASSIGNED;
};
  
int _mw_my_mqid()
{
  return my_mqid;
};

int _mw_mwd_mqid()
{
  if (ipcmain == NULL) return -EILSEQ;
  DEBUG1("lookup of mwd msgqid gave %d from ipcmain at 0x%x", 
	ipcmain->mwd_mqid, ipcmain);
  return ipcmain->mwd_mqid;
};

/* functions dealing with ID's */

void _mw_set_my_serverid(SERVERID sid)
{
  myserverid = MWSERVERMASK | sid;
  return ;
};

void _mw_set_my_clientid(CLIENTID cid)
{
  myclientid = MWCLIENTMASK | cid;
  return ;
};

void _mw_set_my_gatewayid(GATEWAYID gwid)
{
  mygatewayid = MWGATEWAYMASK | gwid;
  return;
};

SERVERID _mw_get_my_serverid()
{
  return   myserverid ;
};

CLIENTID _mw_get_my_clientid()
{
  return myclientid;
};

GATEWAYID _mw_get_my_gatewayid()
{
  return mygatewayid;
};

MWID _mw_get_my_mwid(void)
{
  if (myclientid != UNASSIGNED) return myclientid;
  if (myserverid != UNASSIGNED) return myserverid;
  if (mygatewayid != UNASSIGNED) return mygatewayid;
  return UNASSIGNED;
}

/* functions dealing with shm info */

ipcmaininfo * _mw_ipcmaininfo()
{
  return ipcmain;
};
cliententry * _mw_getcliententry(int i)
{
  if (clttbl == NULL) return NULL;
  i &= MWINDEXMASK;
  if ( (i >= 0) && (i <= ipcmain->clttbl_length) )
    return & clttbl[i];
  else 
    return NULL;
};
serverentry * _mw_getserverentry(int i)
{
  if (srvtbl == NULL) return NULL;
  i &= MWINDEXMASK;
  if ( (i >= 0) && (i <= ipcmain->srvtbl_length) )
    return & srvtbl[i];
  else 
    return NULL;
};
serviceentry * _mw_getserviceentry(int i)
{
  if (svctbl == NULL) return NULL;

  i = SVCID(i);

  if ( (i >= 0) && (i <= ipcmain->svctbl_length) )
    return & svctbl[i];
  else 
    return NULL;
};
gatewayentry * _mw_getgatewayentry(int i)
{
  if (gwtbl == NULL) return NULL;
  i &= MWINDEXMASK;
  if ( (i >= 0) && (i <= ipcmain->gwtbl_ipcid) )
    return & gwtbl[i];
  else 
    return NULL;
};
conv_entry * _mw_getconv_entry(int i)
{
  if (convtbl == NULL) return NULL;
  i &= MWINDEXMASK;
  if ( (i >= 0) && (i <= ipcmain->convtbl_ipcid) )
    return & convtbl[i];
  else 
    return NULL;
};

/*
  Lookup functions to shmtables for info about other members 
  in the MidWay system.
*/
cliententry  * _mw_get_client_byid (CLIENTID cltid)
{
  int index;
  cliententry * tblent; 

  if (ipcmain == NULL) { 
    return NULL;
  };
  if (cltid & MWCLIENTMASK != MWCLIENTMASK) {
    return NULL;
  };

  index = cltid & MWINDEXMASK;
  if (index >= ipcmain->clttbl_length) 
    return NULL;
  
  tblent = & clttbl[index];
  if (tblent->status == UNASSIGNED) {
    return 0;
  };
  return tblent;
};

serverentry  * _mw_get_server_byid (SERVERID srvid)
{
  int index;
  serverentry * srvent; 

  if (ipcmain == NULL) { 
    DEBUG1("_mw_get_server_byid called while ipcmain == NULL");
    return NULL;
  };
  if (srvid & MWSERVERMASK != MWSERVERMASK) {
    DEBUG1("_mw_get_server_byid srvid & MWSERVERMASK %d != %d", srvid & MWSERVERMASK, MWSERVERMASK);
    return NULL;
  };
  index = srvid & MWINDEXMASK;
  if (index >= ipcmain->srvtbl_length) {
    DEBUG1("_mw_get_server_byid index to big %d > %d", index, ipcmain->srvtbl_length);
    return NULL;
  }
  
  srvent = & srvtbl[index];
  if (srvent->status == UNASSIGNED) {
    DEBUG1("_mw_get_server_byid serverid is not in use");
    return NULL;
  };
  return srvent;
};

serviceentry * _mw_get_service_byid (SERVICEID svcid)
{
  int index;
  serviceentry * svcent; 

  if (ipcmain == NULL) { 
    return NULL;
  };
  if (SVCID(svcid) == UNASSIGNED) {
    return NULL;
  };
  index = SVCID2IDX(svcid);
  if (index >= ipcmain->svctbl_length) 
    return NULL;
  
  svcent = & svctbl[index];
  if (svcent->server == UNASSIGNED) {
    return 0;
  };
  return svcent;
};

SERVERID _mw_get_server_by_serviceid (SERVICEID svcid)
{
  int index;

  if (ipcmain == NULL) { 
    return UNASSIGNED;
  };
  if (svcid & MWSERVICEMASK != MWSERVICEMASK) {
    return UNASSIGNED;
  };
  index = svcid & MWINDEXMASK;
  if (index >= ipcmain->svctbl_length) 
    return UNASSIGNED;
  
  if (svctbl[index].server >= 0) return svctbl[index].server;
  if (svctbl[index].gateway >= 0) return svctbl[index].gateway;
  return 0;
};

/* return a list of all MWID's (only gateways and servers that provide
   the given service */
MWID * _mw_get_service_providers(char * svcname, int convflag)
{
  SERVICEID * slist;
  int index = 0, n;
  MWID * rlist;
  
  if (ipcmain == NULL) { 
    return NULL;
  };

  slist = _mw_get_services_byname(svcname, convflag);
  if (slist == NULL) return NULL;

  rlist = malloc(sizeof(MWID) * (ipcmain->svctbl_length+1)); 

  for (index = 0; index < ipcmain->svctbl_length+1; index++) 
    rlist[index] = UNASSIGNED;

  while(slist[index] != UNASSIGNED) {
    
    if (svctbl[index].location == GWLOCAL) {
      rlist[n] = IDX2SRVID(svctbl[index].server);
      n++;
    } else if (svctbl[index].location == GWPEER) {
      rlist[n] = IDX2GWID(svctbl[index].gateway);
      n++;
    } else if (svctbl[index].location == GWREMOTE) {
      rlist[n] = IDX2GWID(svctbl[index].gateway);
      n++;
    };
  };
  
  if (n != 0) return rlist;
  
  Error("we got a list of %d serviceids but no server or gateway that provide any of these services!", index);

  free(rlist);
  return NULL;  
};

/* return the list of sericeid's of the given service */
SERVICEID * _mw_get_services_byname (char * svcname, int convflag)
{
  SERVICEID * slist;
  int type, i, index, n = 0, x;

  if (ipcmain == NULL) { 
    return NULL;
  };

  slist = malloc(sizeof(MWID) * (ipcmain->svctbl_length+1)); 

  if (convflag)
    type = MWCONVSVC;
  else 
    type = MWCALLSVC;

  for (index = 0; index < ipcmain->svctbl_length+1; index++) 
    slist[index] = UNASSIGNED;
  
  for (i = 0; i < ipcmain->svctbl_length; i++) {

    /* we begin in  a random place in the table, in  order not to give
       the first service first in the list every time */
    x = 1+(rand() % ipcmain->svctbl_length);
    index = (x + i) % ipcmain->svctbl_length;

    if (svctbl[index].type == UNASSIGNED) continue;
    if (svctbl[index].type != type) continue;

    if (strncmp(svctbl[index].servicename, svcname, MWMAXSVCNAME) 
	== 0) {
      slist[n++] = index;
    };
  };
    
  if (n != 0) return slist;
  
  free(slist);
  return NULL; 
};

  /* depreciated */
SERVICEID _mw_get_service_byname (char * svcname, int convflag)
{
  int index, type, selectedid = UNASSIGNED;
#ifdef MSGCTLSTATFIX
  int rc, qlen;
  struct msqid_ds this_mq_stat, last_mq_stat;
#endif

  serviceentry * lasttblent = NULL; 
  serverentry * lastsrv = NULL, * thissrv = NULL;

  if (ipcmain == NULL) { 
    return UNASSIGNED;
  };

  if (convflag)
    type = MWCONVSVC;
  else 
    type = MWCALLSVC;

  for (index = 0; index < ipcmain->svctbl_length; index++) {
    /* if the table entry is not in use, skip */
    if (svctbl[index].server == UNASSIGNED) 
      continue;
    /* conversational or not test */
    if (svctbl[index].type != type) 
      continue;
    /* if the name fits, then so does the shoe. */
    if (strncmp(svctbl[index].servicename, svcname, MWMAXSVCNAME) 
	== 0) {
      /* now we must select which serviceid to return
	 THere may be many servers providing this service.
	 First of all local servers are preferred 
	 (in Ver 1.X there are only locals. but)
	 We what the server that are likely to service fastet*/ 
      thissrv = _mw_get_server_byid(svctbl[index].server);
      if (thissrv == NULL) continue;
      if (thissrv->nowserving < 0) {
	/* If the server is idle there is no one else that can do it
	   faster */
	return index | MWSERVICEMASK;
      }

      /* MSGCTLSTATFIX shall be set iff msgctl(..., IPC_STAT, ...) becomes
	 legal if queue not readable, but writeable. 
	 Needs kernel fix, this only a problem with msgctl(), not shmctl(), or semctl()

	 A consequence of the lack of this fix is that we migth return from here, 
	 with reference to a server how has non writable queue, or has its queue removed.
	 I really should never happen, but I want to 
      */
      if (lasttblent == NULL) {
	/* first find */
#ifdef MSGCTLSTATFIX
	rc = msgctl(thissrv->mqid, IPC_STAT, &last_mq_stat);
	if (rc == 0) {
#endif 
	  lasttblent = & svctbl[index];
	  selectedid = index;
	  lastsrv = thissrv;
#ifdef MSGCTLSTATFIX
	}
	/* short cut to exit if found server is idle*/
	if ( (last_mq_stat.msg_qnum == 0) 
	     && (thissrv->nowserving == UNASSIGNED) ) 
	  break;
	else 
	  continue;
#endif 
      };

      /* the call to this function is done before the message is constucted, 
	 thus the time delay  *may*  cause this decision out to be dated.
	 see _mwacallipc()
      */

#ifdef MSGCTLSTATFIX
      rc = msgctl(thissrv->mqid, IPC_STAT, &this_mq_stat);
      if (rc != 0) continue;
      if (this_mq_stat.msg_qnum < last_mq_stat.msg_qnum) {
	memcpy(&last_mq_stat, &this_mq_stat, sizeof(struct msqid_ds));
#endif 
	lasttblent = & svctbl[index];
	selectedid = index;
	lastsrv = thissrv;
#ifdef MSGCTLSTATFIX
      };
#endif 
    }
  }
  if (selectedid == UNASSIGNED) return -ENOENT;
  return selectedid | MWSERVICEMASK;
};


gatewayentry * _mw_get_gateway_table (void)
{  
  return gwtbl;
};

gatewayentry * _mw_get_gateway_byid (GATEWAYID gwid)
{
  int index;
  gatewayentry * gwent; 

  if (ipcmain == NULL) { 
    DEBUG1("_mw_get_gateway_byid called while ipcmain == NULL");
    return NULL;
  };
  if (gwid & MWSERVERMASK != MWSERVERMASK) {
    DEBUG1("_mw_get_gateway_byid gwid & MWGATEWAYMASK %d != %d", gwid & MWSERVERMASK, MWSERVERMASK);
    return NULL;
  };
  index = gwid & MWINDEXMASK;
  if (index >= ipcmain->gwtbl_length) {
    DEBUG1("_mw_get_gateway_byid index to big %d > %d", index, ipcmain->gwtbl_length);
    return NULL;
  }
  
  gwent = & gwtbl[index];
  if (gwent->status == UNASSIGNED) {
    DEBUG1("_mw_get_gateway_byid gatewayid is not in use");
    return NULL;
  };
  return gwent;
};

/* given a MW id, return the messaeg queue id for the id, or -ENOENT if
   No agent with that ID, or -EBADR if mwid is out of range. */
int _mw_get_mqid_by_mwid(int dest)
{
  int qid;
  serverentry * srvent;
  cliententry * cltent;
  gatewayentry * gwent;

  if (dest & MWSERVERMASK) {
    srvent = _mw_get_server_byid(dest);
    if (srvent) 
      qid = srvent->mqid;
    else 
      qid = -1;
  } else if (dest & MWCLIENTMASK) {
    cltent = _mw_get_client_byid(dest);
    if (cltent)
      qid = cltent->mqid;
    else 
      qid = -1;
  } else if (dest & MWGATEWAYMASK) {
    gwent = _mw_get_gateway_byid(dest);
    if (gwent)
      qid = gwent->mqid;
    else 
      qid = -1;
  } else if (dest == 0) {
    /* 0 means mwd. */
    qid = _mw_mwd_mqid();
  } else {
    return -EBADR;
  };
  if (qid == -1) return -ENOENT;
  return qid;
};

/* overall health of the MW instance. 
   returns: 
   0 OK
   -ENAVAIL not connected;
   -ESHUTDOWN if mwd has marked ipcmain shutdown or dead.
   -ECONNREFUSED if mwd is in the progress or booting up.
   -EUCLEAN local error (a can't happen)
   -ECONNABORTED if mwd requester has died abruptly.

   * should we clean up on error?
*/

int _mwsystemstate()
{
  int rc; 

  if (ipcmain  == NULL) return -ENAVAIL;
  if ( (ipcmain->status == MWSHUTDOWN) || (ipcmain->status == MWSHUTDOWN) )
    return -ESHUTDOWN;
  if (ipcmain->status == MWBOOTING) return -ECONNREFUSED;
  
  /* this really should never come true */
  if ( (clttbl == NULL) || (gwtbl == NULL) ||
       (srvtbl == NULL) || (svctbl == NULL) ) return -EUCLEAN;
  
  /* test of if mwd requester is running */
  rc = kill (ipcmain->mwdpid, 0);  
  if  ( ! ( (rc == 0) || (errno == EPERM) )) return -ECONNABORTED;

  return 0;
};
  
/*
  Statistics: NB: this applies only to servers.
  THis is done by calling _mw_stat_begin() when something to 
  are fetched of the input queue, and _mw_stat_end() else.

  We also have an implicit task for servers that issue an _mw_stat_update()
  every 60 seconds.

  */

void _mw_set_my_status(char * status)
{
  serverentry * myentry = NULL;

  myentry = _mw_get_server_byid (myserverid);

  if (myentry == NULL) {
    Warning("Failed to set my status = \"%s\", probably not attached",
	  status == NULL ? "(idle)": status);
    return;
  };

  /* prevent accidental overwrite with too long statuses. */
  if ((status != NULL) && (strlen(status) >= MWMAXNAMELEN) )
    status[MWMAXNAMELEN-1] = '\0';

  if (status == NULL) {
    myentry->status = MWREADY;
    strncpy(myentry->statustext, "(idle)", MWMAXNAMELEN);
    DEBUG1("Returning to idle state");
  } else if (strcmp(status, SHUTDOWN) == 0) {
    myentry->status = MWSHUTDOWN;
    strncpy(myentry->statustext, SHUTDOWN, MWMAXNAMELEN);
    DEBUG1("Status is now SHUTDOWN");
  } else {
    myentry->status = MWBUSY;
    /* status is a service name which is always legal length. */
    strncpy(myentry->statustext, status, MWMAXNAMELEN);
    DEBUG1("Starting service %s", status);
  }
  return;
};

void _mw_update_stats(int qlen, int waitmsec, int servmsec)
{
  /* As above, making decaying stats is a challenge. mwd must make the decay, 
     and thus the update..
     I haven't yet figured out how to pass the updae data to mwd.
  */
  
  DEBUG1("Last service request waited %d millisec to be processes and was  completed in %d millisecs. Message queue has now %d pending requests.", waitmsec, servmsec, qlen);
  return;
};
