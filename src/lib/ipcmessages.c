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
 * Revision 1.1  2000/03/21 21:04:09  eggestad
 * Initial revision
 *
 * Revision 1.1.1.1  2000/01/16 23:20:12  terje
 * MidWay
 *
 */

#include <stdlib.h>
#include <sys/msg.h>
#include <errno.h>

#include <sys/time.h>
#include <unistd.h>

#include <MidWay.h>
#include <shmalloc.h>
#include <ipcmessages.h>
#include <ipctables.h>

extern int _mw_fastpath;
extern int _mw_attached;

/* 
   really need to get all messages into private memory. 
   Or else we may get full ipc queues on a lot of garbage. 

   however I kinda want to avoid it.
*/
struct InputQueue
{
  char * message;
  struct InputQueue * next;
};

struct InputQueue * replyQueue = NULL;

/* the queues grow, but never shrinks. */
static int pushQueue(struct InputQueue ** proot, char * message)
{
  struct InputQueue * thiselm, *newelm;

  if (proot == NULL) return -1;
  thiselm = *proot;

  /* find an empty element, and use that, is avavilable.*/
  while(thiselm->message != NULL) {
    if (thiselm->next == NULL) break;
    thiselm = thiselm->next;
  };

  /* We always get here, if an available element exists we find you here.*/
  if (thiselm->message == NULL) {
    thiselm->message = message;
    return 0;
  };
  
  /* no available, create a new and append at end.*/
  newelm = malloc(sizeof( struct InputQueue));
  newelm->message = message;

  thiselm->next = newelm;
  return 0;
};

static char * popReplyQueueByHandle(long handle)
{
  struct InputQueue * qelm;
  Call * callmesg;
  char * m; 
  qelm = replyQueue;
  
  while( qelm != NULL) {
    callmesg = (Call *) qelm->message;
    if (callmesg->handle == handle) {
      m = qelm->message;
      qelm->message = NULL;
      return m;
    };
  };

  return NULL;  
};

/* for debugging purposes: */
static void  dumpmesg(void * mesg)
{
  long * mtype;
  Attach *am; 
  Provide * pm;
  Call *rm;
  Converse *cm;
  
  mtype = (long *) mesg;
  switch (*mtype) {
  case ATTACHREQ:
  case ATTACHRPL:
  case DETACHREQ:
  case DETACHRPL:
    am = (Attach * ) mesg;
    mwlog(MWLOG_DEBUG, "ATTACH MESSAGE: %#x\n\
int         ipcqid             =  %d\n\
pid_t       pid                =  %d\n\
int         server             =  %d\n\
char        srvname            =  %s\n\
SERVERID    srvid              =  %#x\n\
int         client             =  %d\n\
char        cltname            =  %s\n\
CLIENTID    cltid              =  %#x\n\
int         flags              =  %#x\n\
int         returncode         =  %d", 
	  am->mtype, am->ipcqid, am->pid, 
	  am->server, am->srvname, am->srvid, 
	  am->client, am->cltname, am->cltid, 
	  am->flags, am->returncode);
    return;
    
  case PROVIDEREQ:
  case PROVIDERPL:
  case UNPROVIDEREQ:
  case UNPROVIDERPL:
    pm = (Provide * ) mesg;
    mwlog(MWLOG_DEBUG, "PROVIDEMESSAGE: %#x\n\
SERVERID    srvid              =  %#x\n\
SERVICEID   svcid              =  %#x\n\
char        svcname            =  %s\n\
int         flags              =  %#x\n\
int         returncode         =  %d", 
	  pm->mtype, pm->srvid, pm->svcid, pm->svcname, pm->flags, pm->returncode);
    return;
  case SVCCALL:
  case SVCFORWARD:
  case SVCREPLY:
    rm = (Call * )  mesg;
    mwlog(MWLOG_DEBUG, "CALL/FRD/REPLY MESSAGE: %#x\n\
int         handle             = %d\n\
CLIENTID    cltid              = %#x\n\
SERVERID    srvid              = %#x\n\
SERVICEID   svcid              = %#x\n\
int         forwardcount       = %d\n\
char        service            = %s\n\
char        origservice        = %s\n\
time_t      issued             = %d\n\
int         uissued            = %d\n\
int         timeout            = %d\n\
int         data               = %d\n\
int         datalen            = %d\n\
int         appreturncode      = %d\n\
int         flags              = %#x\n\
char        domainname         = %s\n\
int         returncode         = %d", 
	  rm->mtype, rm->handle, 
	  rm->cltid, rm->srvid, rm->svcid, 
	  rm->forwardcount, rm->service, rm->origservice, 
	  rm->issued, rm->uissued, rm->timeout, 
	  rm->data, rm->datalen, 
	  rm->appreturncode, rm->flags, rm->domainname, rm->returncode);
    return;
  default:
    mwlog(MWLOG_DEBUG, "Unknown message type %#X ", (long *) mesg);
    return;
  };
  
};

/* 
   Until we support POSIX IPC we go directly to msg queue and fetch
   messages of the right type. later when we support events
   we need internal queues for sorting requests, events and replys.
   
   These calls should have a timeout option.

   len input is including the mtype, sysv msgsnd/rcv do not.

   Anyway these hide the low level IPC calls. POSIX vs SYSV are hidden here.
*/


int _mw_ipc_getmessage(char * data, int *len, int type, int flags)
{
  int rc; 

  if (len == NULL) return -EINVAL;
  *len -= sizeof(long);

  rc = msgrcv(_mw_my_mqid(), data, *len, type, flags);
  *len = rc;
  *len += sizeof(long);
  mwlog(MWLOG_DEBUG, "_mw_ipc_getmessage a msgrcv(type=%d, flags=%d) returned %d and %d bytes of data", 
	type, flags, rc, *len);
  /* if we got an interrupt this is OK, else not.*/
  if (rc < 0) {
    if (errno == EINTR) {
      mwlog(MWLOG_DEBUG, "msgrcv in _mw_ipc_getmessage interrupted");
      return -errno;
    };
    mwlog(MWLOG_WARNING, "msgrcv in _mw_ipc_getmessage returned error %d", errno);
    return -errno;
  };
  dumpmesg((void *)data);

  return 0;
};

int _mw_ipc_putmessage(int dest, char *data, int len,  int flags)
{
  int rc; 
  int qid;
  serverentry * srvent;
  cliententry * cltent;
  gatewayentry * gwent;

  if (len > MWMSGMAX) {
    mwlog(MWLOG_ERROR, "_mw_ipc_putmessage: got a to long message %d > %d", len, MWMSGMAX);
    return -E2BIG;
  };
  /* dest is CLIENTID not mqid*/
  if (dest & MWSERVERMASK) {
    srvent = _mw_get_server_byid(dest);
    if (srvent == NULL) return -ENOENT;
    qid = srvent->mqid;
    mwlog(MWLOG_DEBUG, "_mw_ipc_putmessage: got a request to send a message to server %#x on mqueue %d", dest, qid);
  } else if (dest & MWCLIENTMASK) {
    cltent = _mw_get_client_byid(dest);
    qid = cltent->mqid;
    mwlog(MWLOG_DEBUG, "_mw_ipc_putmessage: got a request to send a message to client %#x on mqueue %d", dest, qid);
  } else if (dest & MWGATEWAYMASK) {
    gwent = _mw_get_gateway_byid(dest);
    qid = gwent->mqid;
    mwlog(MWLOG_DEBUG, "_mw_ipc_putmessage: got a request to send a message to gateway %#x on mqueue %d", dest, qid);
  } else if (dest == 0) {
    /* 0 means mwd. */
    qid = _mw_mwd_mqid();
  } else {
    mwlog(MWLOG_DEBUG, "_mw_ipc_putmessage: got a request to send a message to dest %#x which is not a legal ID", dest);
    return -EBADR;
  };
    
  len -= sizeof(long);
  dumpmesg((void *) data);
  rc = msgsnd(qid, data, len, flags);
  mwlog(MWLOG_DEBUG, "_mw_ipc_putmessage: msgsnd(dest=%d, msglen=%d, flags=%#x) returned %d", 
	dest, len, flags, rc);
  /*
    if we got an interrupt this is OK, else not.
    Hmmm, we should have a timeout here. 
  */
  if (rc < 0) {
    if (errno == EINTR) {
      mwlog(MWLOG_DEBUG, "msgrcv in _mw_ipc_putmessage interrupted");
      return 0;
    };
    mwlog(MWLOG_WARNING, "msgrcv in _mw_ipc_putmessage returned error %d", errno);
    return -errno;
  };

  return 0;
};



/* 
 * our functions here provide the interface to sysvipc message queses
 * as well as implement our format of IPC messages.
 * We provide a function for every message, 
 * since some of the params in the masseges are for internalchecks, 
 * they are caught here, thus the API here is as simple as possible.

 * When we convert to POSIX.4 IPC, only this module must be changed.
 * except for mwd of course.
 */
int _mw_ipcsend_attach(int attachtype, char * name, int flags)
{
  
  Attach mesg;
  int tryserver = FALSE;
  int tryclient = FALSE;
  int rc, error, len;
  
  mwlog(MWLOG_DEBUG,
	"CALL: _mw_ipcsend_attach (%d, \"%s\", 0x%x)",
	attachtype, name, flags);

  mesg.mtype = ATTACHREQ;
  if ( (attachtype & MWIPCSERVER) &&
       (_mw_get_my_serverid() == UNASSIGNED) ) {
    mesg.server = TRUE;
    tryserver = TRUE;
    strncpy(mesg.srvname, name, MWMAXNAMELEN);
  } else {
    mesg.server = FALSE;
  };
  
  if ( (attachtype & MWIPCCLIENT)  &&
       ( _mw_get_my_clientid() == UNASSIGNED) ) {
    mesg.client = TRUE;
    tryclient = TRUE;
    strncpy(mesg.cltname, name, MWMAXNAMELEN);
  } else {
    mesg.client = FALSE;
  };

  if ( (mesg.client == FALSE) && (mesg.server == FALSE)) {
    mwlog(MWLOG_WARNING, "_mw_ipcsend_attach: Attempted to attach as neither client nor server");
    return -EINVAL;
  };
  mesg.pid = getpid();
  mesg.flags = flags;
  mesg.ipcqid = _mw_my_mqid();

  /* THREAD MUTEX BEGIN */
  mwlog(MWLOG_DEBUG,
	"Sending an attach message to mwd name = %s, client = %s server = %s size = %d",
	name, mesg.client?"TRUE":"FALSE", mesg.server?"TRUE":"FALSE", sizeof(mesg));
  mwlog(MWLOG_DEBUG,
	"Previous attachment id client=%d server=%d",
	_mw_get_my_clientid(), _mw_get_my_serverid()); 


  rc = _mw_ipc_putmessage(0, (void *) &mesg, sizeof(mesg) ,0);

  if (rc != 0) {
    mwlog (MWLOG_ERROR, 
	   "_mw_ipc_putmessage (%d,...)=>%d failed with error %d(%s)", 
	   0, rc, errno, strerror(errno));
    rc = errno;
    /* MUTEX END */
    return -rc;
  };

  /* Add timeout */
  len = sizeof(mesg);
  rc = _mw_ipc_getmessage ((void *) &mesg, &len , ATTACHRPL, 0);

  
  error = errno;
  /* MUTEX END */

  if (rc < 0) {
    mwlog (MWLOG_ERROR, 
	   " _mw_ipc_getmessage failed with error %d(%s)", errno, strerror(errno));
    return -error;
  };
  
  
  mwlog(MWLOG_DEBUG,
	"Received an attach message reply from mwd \
name = %s, client = %s server = %s srvid=%#x cltid=%#x flags=0x%x rcode=%d",
	name, mesg.client?"TRUE":"FALSE", mesg.server?"TRUE":"FALSE", 
	mesg.srvid, mesg.cltid, mesg.flags, mesg.returncode );
  
  if (mesg.returncode != 0) {
    mwlog (MWLOG_ERROR, "sysv msg ATTACHREPLY had en error rc=%d", 
	   mesg.returncode);
    return -mesg.returncode;
  };

  /* so far we trust mwd not to fuck up, and return EMW* if 
   * either server or client request was unsuccessful
   */
  if (mesg.srvid != UNASSIGNED) _mw_set_my_serverid(mesg.srvid);
  if (mesg.cltid != UNASSIGNED) _mw_set_my_clientid(mesg.cltid);

  return -mesg.returncode;
};

/*
 * mwd support detaching as either server or client if attached
 * as both. However this is not supported in V1.
 * an detach disconnects us completly, 
 * This function always succeed, but it is not declared void, 
 * we really should have some error hendling, but we need rules how to 
 * to handle errors.

 the force flags tell us to set the force flag, and not expect a reply.
 It tell the mwd that we're going down, really an ack to a mq removal, 
 mwd uses that to inform us of a shutdown.
 */
int _mw_ipcsend_detach(int force)
{
  
  Attach mesg;
  CLIENTID cid;
  SERVERID sid;
  int rc, len;

  mwlog(MWLOG_DEBUG, "CALL: _mw_ipcsend_detach()");

  mesg.mtype = DETACHREQ;
  
  sid = _mw_get_my_serverid();
  cid = _mw_get_my_clientid();

  if (sid != UNASSIGNED) {
    mesg.server = TRUE;
    mesg.srvid = sid;
  } else {
    mesg.server = FALSE;
  };
  if (cid != UNASSIGNED){
    mesg.client = TRUE;
    mesg.cltid = cid;
  } else {
    mesg.client = FALSE;
  };
  
  mesg.pid = getpid();
  mesg.flags = 0;
  mesg.ipcqid = _mw_my_mqid();

  if (force) mesg.flags |= MWFORCE;
  /* THREAD MUTEX BEGIN */
  mwlog(MWLOG_DEBUG,
	"Sending a detach message client=%s server=%s", 
	mesg.client?"TRUE":"FALSE", mesg.server?"TRUE":"FALSE");
  rc =  _mw_ipc_putmessage(0, (void *) &mesg, sizeof(mesg),0);
  
  if (rc != 0) {
    mwlog (MWLOG_WARNING, 
	   "_mw_ipc_putmessage(%d,...)=>%d failed with error %d(%s)", 
	   _mw_mwd_mqid(), rc, errno, strerror(errno));
    /* THREAD MUTEX END */
    return 0;
  };

  if (!force) {
    len = sizeof(mesg);
    rc = _mw_ipc_getmessage ( (void *) &mesg, &len , DETACHRPL, 0);
    
    /* THREAD MUTEX END */
    
    if (rc == -1) {
      mwlog (MWLOG_WARNING, 
	     "_mw_ipc_getmessage failed with error %d(%s)", errno, strerror(errno));
      return 0;
    };
    
    
    mwlog(MWLOG_DEBUG,
	  "Received a detach message reply from mwd client = %s server = %s rcode=%d",
	  mesg.client?"TRUE":"FALSE", mesg.server?"TRUE":"FALSE", 
	  mesg.returncode );
    if (mesg.returncode != 0) {
      mwlog (MWLOG_WARNING, "sysv msg ATTACHREPLY had en error rc=%d", 
	     mesg.returncode);
    };
  }; /* !force */
  _mw_set_my_serverid(UNASSIGNED);
  _mw_set_my_clientid(UNASSIGNED);
  return 0;
};


SERVICEID _mw_ipc_provide(char * servicename, int flags)
{
  Provide providemesg, * pmesgptr;
  int rc, error, len, svcid;

  providemesg.mtype = PROVIDEREQ;
  providemesg.srvid = _mw_get_my_serverid();
  strncpy(providemesg.svcname, servicename, MWMAXSVCNAME);
  providemesg.flags = flags;

  mwlog(MWLOG_DEBUG, "Sending a provide message for service %s",  providemesg.svcname);
  /* THREAD MUTEX BEGIN */
  rc = _mw_ipc_putmessage(0, (char *) &providemesg, 
			  sizeof(Provide),0);
  if (rc < 0) {
    mwlog(MWLOG_ERROR, "mwprovide() failed with rc=%d",rc);
    /* MUTEX END */
    return rc;
  };

  /* Add timeout */
  len = sizeof(Provide);
  rc = _mw_ipc_getmessage((char *) &providemesg, &len, PROVIDERPL, 0);
  /* MUTEX END */
  if (rc == -1) {
    mwlog (MWLOG_ERROR, 
	   "Failed to get a reply to a provide request error %d(%s)", 
	   errno, strerror(errno));
    return -error;
  };
  mwlog(MWLOG_DEBUG,"Received a provide reply with service id %d and rcode %d",
	providemesg.srvid, providemesg.returncode);

  if (providemesg.svcid < 0) return providemesg.returncode;
  return providemesg.svcid;
};

int _mw_ipc_unprovide(char * servicename,  SERVICEID svcid)
{
  Provide unprovidemesg;
  int len, rc;
  
  unprovidemesg.mtype = UNPROVIDEREQ;
  unprovidemesg.srvid = _mw_get_my_serverid();



  unprovidemesg.svcid = svcid;
  unprovidemesg.flags = 0;

  rc = _mw_ipc_putmessage(0, (char *) &unprovidemesg, 
			  sizeof(Provide),0);
  if (rc < 0) {
    mwlog(MWLOG_ERROR, "mwunprovide() failed with rc=%d",rc);
    return rc;
  };
  mwlog(MWLOG_DEBUG, "mwunprovide() sent request for unprovide %s with serviceid=%d to mwd", 
	servicename, svcid);
  len = MWMSGMAX;
  rc = _mw_ipc_getmessage((char *) &unprovidemesg, &len, UNPROVIDERPL, 0);
  mwlog(MWLOG_DEBUG, "mwunprovide() got reply for unprovide %s with rcode=%d serviceid = %d(rc=%d)", 
	servicename, unprovidemesg.returncode, svcid, rc);
  
  return unprovidemesg.returncode;
};

/* used to generate handles for mwacall. We use the TCP approach for sequence number.
   the first time called we get a random number, the next time we just bump it one.
*/
static int getnexthandle()
{
  static int hdl = 0; 

  if (hdl == 0) {
    hdl = rand();
    /* just in case it got to be 0 */
    if (hdl == 0) hdl = 12345;
  } else hdl++;
  return hdl;
};

/* the IPC implementation of mwacall() */
int _mwacallipc (char * svcname, char * data, int datalen, 
		 struct timeval * deadline, int flags)
{
  int svcid, dest; 
  int rc;
  int hdl, dataoffset;
  char * dbuf;
  Call calldata;
  struct timeval tm;

  errno = 0;
  svcid = _mw_get_service_byname(svcname,flags&MWCONV);
  if (svcid < 0) {
    mwlog(MWLOG_WARNING,"mwacall() to service %s failed reason %d",
	  svcname,svcid);
    return svcid;
  };
  dest = _mw_get_server_by_serviceid (svcid);
  if (dest < 0) {
    mwlog(MWLOG_WARNING,"mw(a)call(): getting the serverid for serviceid %d(%s) failed reason %d",
	  svcid, svcname, dest);
    return dest;
  };

  /* should this be a sequential number for each server(thread) */
  hdl = getnexthandle();

  mwlog(MWLOG_DEBUG,
	"Doing mwacall() to service %s service id %#x with handle %d",
	svcname, svcid, hdl);
  
  gettimeofday(&tm, NULL);
  calldata.issued = tm.tv_sec;
  calldata.uissued = tm.tv_usec;
  /* mwbegin() is the tuxedo way of set timeout. We use it too
     The nice feature is to set exactly the same deadline on a set of mwacall()s */
  if ( (deadline == NULL) || (deadline->tv_sec == 0) )
    calldata.timeout = 0; 
  else {
    int tout;
    /* we should maybe say that a few ms before a deadline, we call it expired? */
    if ((deadline->tv_sec <= tm.tv_sec) && (deadline->tv_usec <= tm.tv_usec)) {
      /* we've already timed out */
      mwlog(MWLOG_DEBUG1, "call to %s was made after deadline had expired", svcname);
      return -ETIME;
    };
    calldata.timeout = (deadline->tv_sec -  tm.tv_sec) * 1000
      + (deadline->tv_usec - tm.tv_usec) / 1000;
  };

  /* now the data string are passed in shm, we only pass the address (offset) , and 
     the other neccessary data.
     since shmat on Linux do not always reurn tha same address for a given shm segment
     we must operate on offset into the segment (se shmalloc.c) */

  dataoffset = _mwshmcheck(data);
  if (dataoffset == -1) {
    dbuf = _mwalloc(datalen);
    if (dbuf == NULL) {
      mwlog(MWLOG_ERROR, "mwalloc(%d) failed reason %d", datalen, (int) errno);
      return -errno;
    };
    memcpy(dbuf, data, datalen);
    dataoffset = _mwshmcheck(dbuf);
  };
  /* else 
     we really should check to see if datalen is longe that buffer 
  */
  
  calldata.mtype = SVCCALL;
  calldata.data = dataoffset;
  calldata.datalen = datalen;
  calldata.cltid = _mw_get_my_clientid();
  if (calldata.cltid == -1) {
    _mwfree(dbuf);
    mwlog(MWLOG_ERROR, "Failed to get my own clientid! This can't happen.");
    return -EFAULT;
  };
  calldata.srvid = UNASSIGNED;
  calldata.svcid = svcid;
  memcpy (calldata.service, svcname, MWMAXSVCNAME);
  memcpy (calldata.origservice, svcname, MWMAXSVCNAME);
  memset(calldata.domainname, '\0', MWMAXNAMELEN);
  calldata.forwardcount = 0;

  /* when properly implemented some flags will be used in the IPC call */
  calldata.handle = hdl;
  calldata.flags = flags;
  calldata.appreturncode = 0;
  calldata.returncode = 0;
  
  mwlog(MWLOG_DEBUG, "Sending a ipcmessage to serviceid %#x service %s on server %#x my clientid %#x buffer at offset%d len %d ", svcid, calldata.service, dest, calldata.cltid, calldata.data, calldata.datalen);

  rc = _mw_ipc_putmessage(dest, (char *) &calldata, sizeof (Call), 0);
  
  mwlog(MWLOG_DEBUG, "_mw_ipc_putmessage returned %d handle is %d", rc, hdl);
  if (rc >= 0) return hdl;
  return rc;
};
    


/* mwfetch primary purpose is to get the reply to a mwacall(). 
   In that case the handle is what mwacall() returned. 
   However if handle is 0 we can use mwfetch() to get service calls
   in servers. Thus this IPC implementation are a bit diffrent from 
   the TCP one.
*/

/* callmesg is stored globally so that _mwreply can retived info that 
   may not survive the service handle functon 
   NB!!!!! NOT THREAD SAFE!!!!
*/

int _mwfetchipc (int handle, char ** data, int * len, int * appreturncode, int flags)
{
  int rc,l;
  char * buffer;
  struct timeval tv;
  Call * callmesg;

  /*  if (handle == 0) return -NMWNYI;*/
  
  mwlog(MWLOG_DEBUG, "_mwfetch: called for handle %d", handle);

  /* first we check in another call to _mwfetch() retived it and placed 
     in the internal queue.*/
  callmesg = (Call *) popReplyQueueByHandle(handle);

  /* prepare for while loop */
  if (callmesg == NULL) {
    *len = sizeof(Call);
    buffer = malloc(MWMSGMAX);
  };

  while ( callmesg == NULL) {
    /* get the next message of type SVCREPLY of teh IPC queue. */
    rc = _mw_ipc_getmessage(buffer, len , SVCREPLY, flags);
    if (rc != 0) {
      mwlog(MWLOG_DEBUG, "_mwfetch: returned with error code %d", rc);
      free (buffer);
      return rc;
    };
    
    /* we now got the first available reply of the IPC message queue.
       In it wasn't the one we was waiting for push it to the internal queue. */
    callmesg = (Call *) buffer ;
    if (callmesg->handle != handle) {
      
      gettimeofday(&tv, NULL);
      /* test to see if message missed deadline. NOTE if the message
         we were waiting for, we do not test deadline. */
      if ( ((callmesg->issued + callmesg->timeout/1000) < tv.tv_sec) &&  
	   ((callmesg->uissued + callmesg->timeout%1000) < tv.tv_usec) ) {
	mwlog(MWLOG_DEBUG, "Got a reply with handle %d, That had expired, junking it.",
	      callmesg->handle);
      };
      
      mwlog(MWLOG_DEBUG, "Got a reply I was not awaiting with handle %d, enqueuing it internally", callmesg->handle);
      if (pushQueue(&replyQueue, buffer) != 0) {
	mwlog(MWLOG_ERROR, "ABORT: failed in enqueue on internal buffer, this can't happen");
	exit(8);
      };
      buffer = malloc(MWMSGMAX);
      continue;
    };
    
  };
  
  /* we now have the requested reply */
  mwlog(MWLOG_DEBUG,"_mwfetchipc: Got a message of type %#x handle %d ", 
	callmesg->mtype, callmesg->handle);
  /* Retriving info from the message 
     If fastpath we return pointers to the shm area, 
     else we copy to private heap.
  */
  if (! _mw_fastpath) {
    if (*data != NULL) free (*data);
    *data = malloc(callmesg->datalen);
    memcpy(*data, _mwoffset2adr(callmesg->data), callmesg->datalen);
    _mwfree(_mwoffset2adr(callmesg->data));
  } else {
    *data = _mwoffset2adr(callmesg->data);
  }; 

  *len = callmesg->datalen;
  *appreturncode = callmesg->appreturncode;

  /* deadline info is invalid even though I can provide it */

  mwlog(MWLOG_DEBUG, "_mwfetch: returned with rc=%d and with %d bytes of data", 
	rc, callmesg->datalen);
  return callmesg->returncode;
};

int _mwCurrentMessageQueueLength()
{
  struct msqid_ds mqstat;
  int rc;

  rc = msgctl(_mw_my_mqid(), IPC_STAT, &mqstat);
  if (rc != 0) return -errno;

  return mqstat.msg_qnum;
};

int _mw_shutdown_mwd(int delay)
{
  
  Administrative mesg;
  int rc;
  ipcmaininfo * ipcmain = NULL;

  if (_mw_attached == 0) return -ENOTCONN;
  ipcmain = _mw_ipcmaininfo();
  if (ipcmain == NULL) return -EADDRNOTAVAIL;

  mwlog(MWLOG_DEBUG, "CALL: _mw_shutdown:mwd(%d)", delay);

  mesg.mtype = ADMREQ;
  mesg.opcode = ADMSHUTDOWN;
  mesg.cltid = _mw_get_my_clientid();

  /* THREAD MUTEX BEGIN */
  mwlog(MWLOG_DEBUG,
	"Sending a shutdown message to mwd");

  rc =  _mw_ipc_putmessage(0, (void *) &mesg, sizeof(mesg),0);
  
  if (rc != 0) {
    mwlog (MWLOG_WARNING, 
	   "_mw_ipc_putmessage(%d,...)=>%d failed with error %d(%s)", 
	   0, rc, errno, strerror(errno));
    /* THREAD MUTEX END */
    return 0;
  };

  /* THREAD MUTEX END */
  
  
  return 0;
};
