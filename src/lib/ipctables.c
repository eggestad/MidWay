
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
 * Revision 1.1  2000/03/21 21:04:11  eggestad
 * Initial revision
 *
 * Revision 1.1.1.1  2000/01/16 23:20:12  terje
 * MidWay
 *
 */

#include <errno.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/time.h>

#include <MidWay.h>
#include <ipctables.h>
#include <ipcmessages.h>


static ipcmaininfo  * ipcmain = NULL; 
static cliententry  * clttbl  = NULL;
static serverentry  * srvtbl  = NULL;
static serviceentry * svctbl  = NULL;
static gatewayentry * gwtbl   = NULL;
static conv_entry   * convtbl = NULL;

static my_mqid = -1;

static SERVERID myserverid = UNASSIGNED;
static CLIENTID myclientid = UNASSIGNED;


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
    mwlog(MWLOG_WARNING, 
	  "Attempted to attach shm segments while already attached");
    return -EISCONN; 
  };

  /* if we're a clients we shall attach all the shared memory tables readonly */
  if (type & MWIPCSERVER)  readonly = 0;
  else readonly = SHM_RDONLY;

  /* THREAD MUTEX BEGIN */
  /* NOTE: below the returns in the if(error) do not release the MUTEX */


  mainid = shmget(key, 0, 0);
  if (mainid == -1) {
    mwlog(MWLOG_ERROR,"There are no shared memory for key = 0x%x reason %s",
	  key, strerror(errno));
    return -errno;
  };
  mwlog(MWLOG_DEBUG,"main shm info table has id %d",mainid);

  ipcmain = (ipcmaininfo *) shmat(mainid, (void *) 0, readonly);
  if ( (ipcmain == NULL) || (ipcmain == (void *) -1) ) {
    mwlog(MWLOG_ERROR,"Failed to attach shared memory with id = %d reason %s",
	  mainid, strerror(errno));
    ipcmain = NULL;
    /* MUTEX END */
    return -errno;
  };
  mwlog(MWLOG_DEBUG,"main shm info table attached at 0x%x",ipcmain);


  /* attaching all the other tables */

  clttbl = shmat(ipcmain->clttbl_ipcid, NULL, readonly);
  if (clttbl == (void *) -1) {
    mwlog(MWLOG_ERROR,"Failed to attach client table with id = %d reason %s",
	  ipcmain->clttbl_ipcid, strerror(errno));
    return -errno;
  };
  mwlog(MWLOG_DEBUG,"client table attached at 0x%x 0x%x ",clttbl, rc);
  
  srvtbl = shmat(ipcmain->srvtbl_ipcid, NULL, readonly);
  if (srvtbl == (void *) -1) {
    mwlog(MWLOG_ERROR,"Failed to attach Server table with id = %d reason %s",
	  ipcmain->srvtbl_ipcid, strerror(errno));
    return -errno;
  };
  mwlog(MWLOG_DEBUG,"Server table attached at 0x%x",srvtbl);
  
  svctbl = shmat(ipcmain->svctbl_ipcid, NULL, readonly);
  if (svctbl == (void *) -1) {
    mwlog(MWLOG_ERROR,"Failed to attach service table with id = %d reason %s",
	  ipcmain->svctbl_ipcid, strerror(errno));
    return -errno;
  };
  mwlog(MWLOG_DEBUG,"service table attached at 0x%x",svctbl);
  
  gwtbl = shmat(ipcmain->gwtbl_ipcid, NULL, readonly);
  if (gwtbl == (void *) -1) {
    mwlog(MWLOG_ERROR,"Failed to attach gateway table with id = %d reason %s",
	  ipcmain->gwtbl_ipcid, strerror(errno));
    return -errno;
  };
  mwlog(MWLOG_DEBUG,"gateway table attached at 0x%x",gwtbl);
  
  convtbl = shmat(ipcmain->convtbl_ipcid, NULL, readonly);
  if (convtbl == (void *) -1) {
    mwlog(MWLOG_ERROR,"Failed to attach convserver table with id = %d reason %s",
	  ipcmain->convtbl_ipcid, strerror(errno));
    return -errno;
  };
  mwlog(MWLOG_DEBUG,"convserver table attached at 0x%x",convtbl);

  my_mqid = msgget(IPC_PRIVATE, 0622); /* perm = rw--w--w- */ 
  if (my_mqid == -1) {
    mwlog(MWLOG_ERROR,"Failed to create an ipc queue for this process reason=%d",
	  errno);
    return -errno;
  };
  mwlog(MWLOG_DEBUG,"My request/reply queue has IPC id %d", my_mqid );

  /* Note flag, the heap is never readonly. */
  _mwHeapInfo = shmat (ipcmain->heap_ipcid, NULL, 0);
  if (_mwHeapInfo != NULL) {
    mwlog(MWLOG_DEBUG, "heap attached");
  };
  /* THREAD MUTEX ENDS */
  return 0;
};

#include <stdio.h>

void
_mw_detach_ipc(int cleantables)
{
  extern struct segmenthdr * _mwHeapInfo;
  serverentry * srvent;
  cliententry * cltent;
  
  /* this is really wrong too mess with eth ipctables directly!!!
  srvent = _mw_getserverentry(myserverid);
  printf ("srvent of %d is %#X", myserverid, srvent);
  if (srvent != NULL) {
    srvent->status = UNASSIGNED;
    srvent->pid = UNASSIGNED;
    srvent->mqid = UNASSIGNED;
    strncpy(srvent->statustext, "(DEAD)", MWMAXNAMELEN);
  };
  cltent = _mw_getcliententry(myclientid);
  if (cltent != NULL) {
    cltent->status = UNASSIGNED;
    cltent->type = UNASSIGNED;
    cltent->location = UNASSIGNED;
    cltent->pid = UNASSIGNED;
    cltent->mqid = UNASSIGNED;
  };
  */
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
  mwlog(MWLOG_DEBUG,"lookup of mwd msgqid gave %d from ipcmain at 0x%x", 
	ipcmain->mwd_mqid, ipcmain);
  return ipcmain->mwd_mqid;
};


void _mw_set_my_serverid(SERVERID sid)
{
  myserverid = sid;
  return ;
};

void _mw_set_my_clientid(CLIENTID cid)
{
  myclientid = cid;
  return ;
};

SERVERID _mw_get_my_serverid()
{
  return   myserverid ;
};

CLIENTID _mw_get_my_clientid()
{
  return myclientid;
};

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
  i &= MWINDEXMASK;
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
    mwlog(MWLOG_DEBUG,"_mw_get_server_byid called while ipcmain == NULL");
    return NULL;
  };
  if (srvid & MWSERVERMASK != MWSERVERMASK) {
    mwlog(MWLOG_DEBUG,"_mw_get_server_byid srvid & MWSERVERMASK %d != %d", srvid & MWSERVERMASK, MWSERVERMASK);
    return NULL;
  };
  index = srvid & MWINDEXMASK;
  if (index >= ipcmain->srvtbl_length) {
    mwlog(MWLOG_DEBUG,"_mw_get_server_byid index to big %d > %d", index, ipcmain->srvtbl_length);
    return NULL;
  }
  
  srvent = & srvtbl[index];
  if (srvent->status == UNASSIGNED) {
    mwlog(MWLOG_DEBUG,"_mw_get_server_byid serverid is not in use");
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
  if (svcid & MWSERVICEMASK != MWSERVICEMASK) {
    return NULL;
  };
  index = svcid & MWINDEXMASK;
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
  serviceentry * svcent; 

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

SERVICEID _mw_get_service_byname (char * svcname, int convflag)
{
  int rc, index, type, selectedid = UNASSIGNED, qlen;
  struct msqid_ds this_mq_stat, last_mq_stat;
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

gatewayentry * _mw_get_gateway_byid (GATEWAYID srvid)
{
  return NULL;
};

/* overall health of the MW instance. 
   returns: 
   1 NOT CONNECTED;
   0 OK
   -ESHUTDOWN if mwd has marked ipcmain shutdown or dead.
   -ECONNREFUSED if mwd is in the progress or booting up.
   -EUCLEAN local error (a can't happen)
   -ECONNABORTED if mwd requester has died abruptly.

   * should we clean up on error?
*/
int _mwsystemstate()
{
  int rc; 

  if (ipcmain  == NULL) return 1;
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
    mwlog(MWLOG_WARNING, "Failed to set my status = \"%s\", probaly not attached",
	  status == NULL ? "(idle)": status);
    return;
  };

  /* prevent accidental overwrite with too long statuses. */
  if ((status != NULL) && (strlen(status) >= MWMAXNAMELEN) )
    status[MWMAXNAMELEN-1] = '\0';

  if (status == NULL) {
    myentry->status = MWNORMAL;
    strncpy(myentry->statustext, "(idle)", MWMAXNAMELEN);
    mwlog(MWLOG_DEBUG, "Returning to idle state");
  } else if (strcmp(status, SHUTDOWN) == 0) {
    myentry->status = MWSHUTDOWN;
    strncpy(myentry->statustext, SHUTDOWN, MWMAXNAMELEN);
    mwlog(MWLOG_DEBUG, "Status is now SHUTDOWN");
  } else {
    myentry->status = MWBUSY;
    /* status is a service name which is always legal length. */
    strncpy(myentry->statustext, status, MWMAXNAMELEN);
    mwlog(MWLOG_DEBUG, "Starting service %s", status);
  }
  return;
};

void _mw_update_stats(int qlen, int waitmsec, int servmsec)
{
  /* As above, making decaying stats is a challenge. mwd must make the decay, 
     and thus the update..
     I haven't yet figured out how to pass the updae data to mwd.
  */
  
  mwlog(MWLOG_DEBUG, "Last service request waited %d millisec to be processes and was  completed in %d millisecs. Message queue has now %d pending requests.", waitmsec, servmsec, qlen);
  return;
};
