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
 * Revision 1.4  2000/09/21 18:35:16  eggestad
 * Lots of bug fixes:
 *  - pushQueue didn't work, crashed on first push
 *  - popQueueByHandle had a endless loop
 *  - all IPC messages are now NULL'ed out at every function.
 *  - in pushQueue next was not NULL on push 2 and after.
 *  - issue of handles are now moved to mwclientapi to be common with SRB handles
 *  - deadline handling quite diffrent
 *  - ++
 *
 * Revision 1.3  2000/08/31 21:50:21  eggestad
 * DEBUG level set propper. In acall, propper clearing of message
 *
 * Revision 1.2  2000/07/20 19:22:08  eggestad
 * Changes needed for SRB clients.
 *
 * Revision 1.1.1.1  2000/03/21 21:04:09  eggestad
 * Initial Release
 *
 * Revision 1.1.1.1  2000/01/16 23:20:12  terje
 * MidWay
 *
 */

static char * RCSId = "$Id$";
static char * RCSName = "$Name$"; /* CVS TAG */

#include <stdlib.h>
#include <sys/msg.h>
#include <errno.h>

#include <sys/time.h>
#include <unistd.h>

#include <MidWay.h>
#include <shmalloc.h>
#include <ipcmessages.h>
#include <ipctables.h>
#include <mwclientapi.h>

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

static struct InputQueue * replyQueue = NULL;

/* the queues grow, but never shrinks. */
static int pushQueue(struct InputQueue ** proot, char * message)
{
  struct InputQueue * thiselm, *newelm;

  if (proot == NULL) return -1;
  thiselm = *proot;

  /* special case of an empty list */
  if (thiselm == NULL) {
    *proot = malloc(sizeof( struct InputQueue));
    (*proot)->message = message;
    (*proot)->next = NULL;
    return 0;
  }
  
  /* find an empty element, and use it if avavilable.*/
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
  newelm->next = NULL;
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
    mwlog(MWLOG_DEBUG3, "in  popReplyQueueByHandle tetsing qelm at %p", qelm);
    callmesg = (Call *) qelm->message;    
    if (callmesg->handle == handle) {
      m = qelm->message;
      qelm->message = NULL;
      mwlog(MWLOG_DEBUG1, "popReplyQueueByHandle returning message at %p", m);
      return m;
    };
    qelm = qelm->next;
  };
  mwlog(MWLOG_DEBUG1, "popReplyQueueByHandle no reply with handle %#x", handle);
  return NULL;  
};

/* for debugging purposes: */
void  _mw_dumpmesg(void * mesg)
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
    mwlog(MWLOG_DEBUG1, "ATTACH MESSAGE: %#x\n\
          int         ipcqid             =  %d\n\
          pid_t       pid                =  %d\n\
          int         server             =  %d\n\
          char        srvname            =  %.32s\n\
          SERVERID    srvid              =  %#x\n\
          int         client             =  %d\n\
          char        cltname            =  %.64s\n\
          CLIENTID    cltid              =  %#x\n\
          GATEWAYID   gwid               =  %#x\n\
          int         flags              =  %#x\n\
          int         returncode         =  %d", 
	  am->mtype, am->ipcqid, am->pid, 
	  am->server, am->srvname, am->srvid, 
	  am->client, am->cltname, am->cltid, 
	  am->gwid, am->flags, am->returncode);
    return;
    
  case PROVIDEREQ:
  case PROVIDERPL:
  case UNPROVIDEREQ:
  case UNPROVIDERPL:
    pm = (Provide * ) mesg;
    mwlog(MWLOG_DEBUG1, "PROVIDEMESSAGE: %#x\n\
          SERVERID    srvid              =  %#x\n\
          SERVICEID   svcid              =  %#x\n\
          char        svcname            =  %.32s\n\
          int         flags              =  %#x\n\
          int         returncode         =  %d", 
	  pm->mtype, pm->srvid, pm->svcid, pm->svcname, pm->flags, pm->returncode);
    return;
  case SVCCALL:
  case SVCFORWARD:
  case SVCREPLY:
    rm = (Call * )  mesg;
    mwlog(MWLOG_DEBUG1, "CALL/FRD/REPLY MESSAGE: %#x\n\
          int         handle             = %d\n\
          CLIENTID    cltid              = %#x\n\
          SERVERID    srvid              = %#x\n\
          SERVICEID   svcid              = %#x\n\
          GATEWAYID   gwid               = %#x\n\
          int         forwardcount       = %d\n\
          char        service            = %.32s\n\
          char        origservice        = %.32s\n\
          time_t      issued             = %d\n\
          int         uissued            = %d\n\
          int         timeout            = %d\n\
          int         data               = %d\n\
          int         datalen            = %d\n\
          int         appreturncode      = %d\n\
          int         flags              = %#x\n\
          char        domainname         = %.64s\n\
          int         returncode         = %d", 
	  rm->mtype, rm->handle, 
	  rm->cltid, rm->srvid, rm->svcid, rm->gwid,
	  rm->forwardcount, rm->service, rm->origservice, 
	  rm->issued, rm->uissued, rm->timeout, 
	  rm->data, rm->datalen, 
	  rm->appreturncode, rm->flags, rm->domainname, rm->returncode);
    return;
  default:
    mwlog(MWLOG_DEBUG1, "Unknown message type %#X ", (long *) mesg);
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

  errno = 0;
  rc = msgrcv(_mw_my_mqid(), data, *len, type, flags);
  /* if we got an interrupt this is OK, else not.*/
  if (rc < 0) {
    if (errno == EINTR) {
      mwlog(MWLOG_DEBUG1, "msgrcv in _mw_ipc_getmessage interrupted");
      return -errno;
    };
    mwlog(MWLOG_WARNING, "msgrcv in _mw_ipc_getmessage returned error %d", errno);
    return -errno;
  };
  *len = rc;
  *len += sizeof(long);

  mwlog(MWLOG_DEBUG1, "_mw_ipc_getmessage a msgrcv(type=%d, flags=%d) returned %d and %d bytes of data", 
	type, flags, rc, *len);
  
  _mw_dumpmesg((void *)data);
  
  return 0;
};

int _mw_ipc_putmessage(int dest, char *data, int len,  int flags)
{
  int rc; 
  int qid;

  if (len > MWMSGMAX) {
    mwlog(MWLOG_ERROR, "_mw_ipc_putmessage: got a to long message %d > %d", len, MWMSGMAX);
    return -E2BIG;
  };
  qid = _mw_get_mqid_by_mwid(dest);
  if (qid < 0) {
    mwlog(MWLOG_ERROR, "_mw_ipc_putmessage: got a request to send a message to %#x by not such ID exists, reason %d", dest, qid);
    return qid;
  };

  mwlog(MWLOG_DEBUG1, "_mw_ipc_putmessage: got a request to send a message to  %#x on mqueue %d", dest, qid);
  
  len -= sizeof(long);
  _mw_dumpmesg((void *) data);
  rc = msgsnd(qid, data, len, flags);
  mwlog(MWLOG_DEBUG1, "_mw_ipc_putmessage: msgsnd(dest=%d, msglen=%d, flags=%#x) returned %d", 
	dest, len, flags, rc);
  /*
    if we got an interrupt this is OK, else not.
    Hmmm, we should have a timeout here. 
  */
  if (rc < 0) {
    if (errno == EINTR) {
      mwlog(MWLOG_DEBUG1, "msgrcv in _mw_ipc_putmessage interrupted");
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
  int rc, error, len;

  mwlog(MWLOG_DEBUG1,
	"CALL: _mw_ipcsend_attach (%d, \"%s\", 0x%x)",
	attachtype, name, flags);

  memset(&mesg, '\0', sizeof(Attach));
  mesg.mtype = ATTACHREQ;
  mesg.gwid = UNASSIGNED;
  mesg.server = FALSE;
  mesg.client = FALSE;

  if ( (attachtype & MWIPCSERVER) &&
       (_mw_get_my_serverid() == UNASSIGNED) ) {
    mesg.server = TRUE;
    strncpy(mesg.srvname, name, MWMAXNAMELEN);
  } 
  
  if ( (attachtype & MWIPCCLIENT)  &&
       ( _mw_get_my_clientid() == UNASSIGNED) ) {
    mesg.client = TRUE;
    strncpy(mesg.cltname, name, MWMAXNAMELEN);
  } 

  if ( (mesg.client == FALSE) && (mesg.server == FALSE)) {
    mwlog(MWLOG_WARNING, "_mw_ipcsend_attach: Attempted to attach as neither client nor server");
    return -EINVAL;
  };
  mesg.pid = getpid();
  mesg.flags = flags;
  mesg.ipcqid = _mw_my_mqid();

  /* THREAD MUTEX BEGIN */
  mwlog(MWLOG_DEBUG1,
	"Sending an attach message to mwd name = %s, client = %s server = %s size = %d",
	name, mesg.client?"TRUE":"FALSE", mesg.server?"TRUE":"FALSE", sizeof(mesg));

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
  
  
  mwlog(MWLOG_DEBUG1,
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
 * mwd support detaching as either server or client if attached as
 * both. However this is not supported in V1.  an detach disconnects
 * us completly, This function always succeed, but it is not declared
 * void, we really should have some error hendling, but we need rules
 * how to to handle errors.

 * the force flags tell us to set the force flag, and not expect a
 * reply.  It tell the mwd that we're going down, really an ack to a
 * mq removal, mwd uses that to inform us of a shutdown.

 * Maybe a bad design/imp desicion, by I need a seperate call to
 * detach an indirect client, networked, even if _mw_ipcsend_attach()
 * do both. Maybe we should have a separate attach?. Fortunately
 * _mw_ipcsedn_detach() is almost directly mapped on
 * _mw_ipcsend_detach_indirect() */

int _mw_ipcsend_detach_indirect(CLIENTID cid, SERVERID sid, int force) 
{
  Attach mesg;
  int rc, len;

  mwlog(MWLOG_DEBUG1, "CALL: _mw_ipcsend_detach_indirect()");
  memset(&mesg, '\0', sizeof(Attach));
  
  mesg.mtype = DETACHREQ;
  mesg.server = FALSE;
  mesg.client = FALSE;
  
  if (sid != UNASSIGNED) {
    mesg.server = TRUE;
    mesg.srvid = sid;
  };

  if (cid != UNASSIGNED){
    mesg.client = TRUE;
    mesg.cltid = cid;
  };

  
  mesg.pid = getpid();
  mesg.flags = 0;
  mesg.ipcqid = _mw_my_mqid();

  /* this is currently used to not wanting a reply */
  if (force) mesg.flags |= MWFORCE;

  /* THREAD MUTEX BEGIN */
  mwlog(MWLOG_DEBUG1,
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
    
    
    mwlog(MWLOG_DEBUG1,
	  "Received a detach message reply from mwd client = %s server = %s rcode=%d",
	  mesg.client?"TRUE":"FALSE", mesg.server?"TRUE":"FALSE", 
	  mesg.returncode );
    if (mesg.returncode != 0) {
      mwlog (MWLOG_WARNING, "sysv msg ATTACHREPLY had en error rc=%d", 
	     mesg.returncode);
    };
  }; /* !force */

  return 0;
};

int _mw_ipcsend_detach(int force)
{
  CLIENTID cid;
  SERVERID sid;
  int rc;

  sid = _mw_get_my_serverid();
  cid = _mw_get_my_clientid();

  rc = _mw_ipcsend_detach_indirect(cid, sid, force);

  _mw_set_my_serverid(UNASSIGNED);
  _mw_set_my_clientid(UNASSIGNED);
  return rc;
};


SERVICEID _mw_ipc_provide(char * servicename, int flags)
{
  Provide providemesg, * pmesgptr;
  int rc, error, len, svcid;

  memset(&providemesg, '\0', sizeof(Provide));
  
  providemesg.mtype = PROVIDEREQ;
  providemesg.srvid = _mw_get_my_serverid();
  strncpy(providemesg.svcname, servicename, MWMAXSVCNAME);
  providemesg.flags = flags;

  mwlog(MWLOG_DEBUG1, "Sending a provide message for service %s",  providemesg.svcname);
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
  mwlog(MWLOG_DEBUG1,"Received a provide reply with service id %d and rcode %d",
	providemesg.srvid, providemesg.returncode);

  if (providemesg.svcid < 0) return providemesg.returncode;
  return providemesg.svcid;
};

int _mw_ipc_unprovide(char * servicename,  SERVICEID svcid)
{
  Provide unprovidemesg;
  int len, rc;

  memset(&unprovidemesg, '\0', sizeof(Provide));
  
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
  mwlog(MWLOG_DEBUG1, "mwunprovide() sent request for unprovide %s with serviceid=%d to mwd", 
	servicename, svcid);
  len = MWMSGMAX;
  rc = _mw_ipc_getmessage((char *) &unprovidemesg, &len, UNPROVIDERPL, 0);
  mwlog(MWLOG_DEBUG1, "mwunprovide() got reply for unprovide %s with rcode=%d serviceid = %d(rc=%d)", 
	servicename, unprovidemesg.returncode, svcid, rc);
  
  return unprovidemesg.returncode;
};

/* the IPC implementation of mwacall() */
int _mwacallipc (char * svcname, char * data, int datalen, int flags)
{
  int svcid, dest; 
  int rc;
  int hdl, dataoffset;
  char * dbuf;
  Call calldata;
  struct timeval tm;
  float timeleft;

  memset (&calldata, '\0', sizeof(Call));
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
  hdl = _mw_nexthandle();

  mwlog(MWLOG_DEBUG1,
	"Doing mwacall() to service %s service id %#x with handle %d",
	svcname, svcid, hdl);
  
  gettimeofday(&tm, NULL);
  calldata.issued = tm.tv_sec;
  calldata.uissued = tm.tv_usec;
  /* mwbegin() is the tuxedo way of set timeout. We use it too
     The nice feature is to set exactly the same deadline on a set of mwacall()s */
    
  if ( _mw_deadline(NULL, &timeleft)) {
    /* we should maybe say that a few ms before a deadline, we call it expired? */
    if (timeleft <= 0.0) {
      /* we've already timed out */
      mwlog(MWLOG_DEBUG1, "call to %s was made %d ms after deadline had expired", 
	    timeleft, svcname);
      return -ETIME;
    };
    /* timeout is here in ms */
    calldata.timeout = (int) (timeleft * 1000);
  } else {
    calldata.timeout = 0; 
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
  calldata.gwid = _mw_get_my_gatewayid();

  if (calldata.cltid == -1) {
    _mwfree(dbuf);
    mwlog(MWLOG_ERROR, "Failed to get my own clientid! This can't happen.");
    return -EFAULT;
  };
  calldata.srvid = UNASSIGNED;
  calldata.svcid = svcid;
  strncpy (calldata.service, svcname, MWMAXSVCNAME);
  strncpy (calldata.origservice, svcname, MWMAXSVCNAME);
  calldata.forwardcount = 0;

  /* when properly implemented some flags will be used in the IPC call */
  calldata.handle = hdl;
  calldata.flags = flags;
  calldata.appreturncode = 0;
  calldata.returncode = 0;
  
  mwlog(MWLOG_DEBUG1, "Sending a ipcmessage to serviceid %#x service %s on server %#x my clientid %#x buffer at offset%d len %d ", svcid, calldata.service, dest, calldata.cltid, calldata.data, calldata.datalen);

  rc = _mw_ipc_putmessage(dest, (char *) &calldata, sizeof (Call), 0);
  
  mwlog(MWLOG_DEBUG1, "_mw_ipc_putmessage returned %d handle is %d", rc, hdl);
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
  
  mwlog(MWLOG_DEBUG1, "_mwfetch: called for handle %d", handle);

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
      mwlog(MWLOG_DEBUG1, "_mwfetch: returned with error code %d", rc);
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
	mwlog(MWLOG_DEBUG1, "Got a reply with handle %d, That had expired, junking it.",
	      callmesg->handle);
      };
      
      mwlog(MWLOG_DEBUG1, "Got a reply I was not awaiting with handle %d, enqueuing it internally", callmesg->handle);
      if (pushQueue(&replyQueue, buffer) != 0) {
	mwlog(MWLOG_ERROR, "ABORT: failed in enqueue on internal buffer, this can't happen");
	exit(8);
      };
      buffer = malloc(MWMSGMAX);
      continue;
    };
    
  };
  
  /* we now have the requested reply */
  mwlog(MWLOG_DEBUG1,"_mwfetchipc: Got a message of type %#x handle %d ", 
	callmesg->mtype, callmesg->handle);
  /* Retriving info from the message 
     If fastpath we return pointers to the shm area, 
     else we copy to private heap.
  */
  if (! _mw_fastpath_enabled()) {
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

  mwlog(MWLOG_DEBUG1, "_mwfetch: returned with rc=%d and with %d bytes of data", 
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

  if (_mw_isattached() == 0) return -ENOTCONN;
  ipcmain = _mw_ipcmaininfo();
  if (ipcmain == NULL) return -EADDRNOTAVAIL;

  mwlog(MWLOG_DEBUG1, "CALL: _mw_shutdown:mwd(%d)", delay);

  mesg.mtype = ADMREQ;
  mesg.opcode = ADMSHUTDOWN;
  mesg.cltid = _mw_get_my_clientid();

  /* THREAD MUTEX BEGIN */
  mwlog(MWLOG_DEBUG1,
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
