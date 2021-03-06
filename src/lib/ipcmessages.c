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

#include <stdlib.h>
#include <sys/msg.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>

#include <sys/time.h>
#include <unistd.h>

#include <MidWay.h>
#include <shmalloc.h>
#include <ipcmessages.h>
#include <ipctables.h>
#include <mwclientapi.h>

/** @file
 We here have all the function that deal with sending and receiving IPC messages. 

*/
   
struct CallReplyQueue
{
  Call * callmessage;
  struct CallReplyQueue * next;
};

static struct CallReplyQueue * callReplyQueue = NULL;
static struct CallReplyQueue * callReplyFreeList = NULL;

static void debugReplyQueue(void) 
{
   struct CallReplyQueue * elm;
   int q = 0, f = 0;

   for (elm = callReplyQueue; elm != NULL; elm = elm->next) q++;
   for (elm = callReplyFreeList; elm != NULL; elm = elm->next) f++;
   
   DEBUG1(" Callreply queue length = %d free %d", q, f);
};
 
// push  at end, pop from the start
static int pushCallReply(Call * callmsg)
{
   struct CallReplyQueue * newelm, ** pplast;

   DEBUG1("pushing call with handle %d", callmsg->handle);
   debugReplyQueue();

   for (pplast = &callReplyQueue; (*pplast) != NULL; pplast = &(*pplast)->next);

   DEBUG1("got the last entry pplast=%p (*pplast) = %p", pplast, *pplast);

   if (callReplyFreeList == NULL) {
      DEBUG1("new elm");
      newelm = malloc(sizeof(struct CallReplyQueue));
   } else {
      DEBUG1("new elm poping from freelist ");
      newelm = callReplyFreeList;
      callReplyFreeList = callReplyFreeList->next;
   };
   DEBUG1("newelm %p callReplyQueue %p newelm->next %p", newelm, callReplyQueue, newelm->next);
   newelm->callmessage = callmsg;
   newelm->next = *pplast;
   *pplast = newelm;
   DEBUG1("newelm %p callReplyQueue %p newelm->next %p", newelm, callReplyQueue, newelm->next);
   debugReplyQueue();
   return 0;
};

/**
 * We need to expose this in the casde a call reply comes while in mwservicerequest
 */
int _mw_ipc_pushCallReply(Call * callmsg) {
   return pushCallReply(callmsg);
}


static Call * popCallReplyByHandle(int handle)
{
   struct CallReplyQueue ** pelm, *elm;
   Call * callptr = NULL;

   DEBUG1("poping call with handle %d", handle);

   debugReplyQueue();

   if (callReplyQueue == NULL) goto out;

   if (handle == 0) {
      DEBUG1("pop any queue = %p", callReplyQueue);
      if (callReplyQueue) {
	 elm = callReplyQueue;
	 callReplyQueue = callReplyQueue->next;

	 callptr =  elm->callmessage;
	 elm->callmessage = NULL;
	 elm->next = callReplyFreeList;
	 callReplyFreeList = elm;
      };
      goto out;
   };
      
   pelm = &callReplyQueue;
      
   do {
      elm = *pelm;
      if (elm == NULL) goto out;
      if (elm->callmessage->handle == handle) {
	 callptr = elm->callmessage;
	 elm->callmessage = NULL;
	 *pelm = elm->next;
	 elm->next = callReplyFreeList;
	 callReplyFreeList = elm;
	 goto out;
      };
      if (elm->next == NULL) goto out;
      pelm = &elm->next;
   } while(pelm);
   return NULL;

 out:
   debugReplyQueue();
   if (callptr)
      DEBUG1("pop returned call with handle %d", callptr->handle);
   else 
      DEBUG1("pop returned NULL");
   return callptr;
};

   
#if 0
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

  if (!qelm)
     DEBUG3("queue empty");

  while( qelm != NULL) {
    DEBUG3("in  popReplyQueueByHandle testing qelm at %p", qelm);
    callmesg = (Call *) qelm->message;    
    if (callmesg && (callmesg->handle == handle)) {
      m = qelm->message;
      qelm->message = NULL;
      DEBUG1("popReplyQueueByHandle returning message at %p", m);
      return m;
    };
    qelm = qelm->next;
  };
  DEBUG1("popReplyQueueByHandle no reply with handle %#x", handle);
  return NULL;  
};
#endif

static char * strdata(seginfo_t * si, int dataoff, int len)
{
   static char data[80];
   char * rdata;
   int i, l;
   
   rdata = _mwoffset2adr(dataoff, si);
   
   if (!rdata) {
      if (si) {
	 sprintf(data, "(null) ERROR: could not lookup buffer in segment %d size %ld offset %d len %d", 
		 si->segmentid, (long) si->end - (long) si->start, dataoff, len);
      } else {
	 sprintf(data, "(null) ERROR: could not lookup buffer in NULL segment  offset %d len %d", 
		 dataoff, len);
      };
      return data;
   };
   if (len > 72) l = 72;
   else  l = len;

   for (i = 0; i < l; i++) {
      if (isprint(rdata[i])) data[i] = rdata[i];
      else data[i] = '?';

   };
   if (l < len) {
      data[i++] = '.'; 
      data[i++] = '.'; 
      data[i++] = '.'; 
   };
   data[i] = '\0'; 
   return data;
};

////////////////////////////////////////////////////////////////////////

	  
/* for debugging purposes: */

static const char * messagetype(int mtype)
{
   
   switch (mtype) {

   case ATTACHREQ:
      return "Attach Request";
   case ATTACHRPL:
      return "Attach Reply";
   case DETACHREQ:
      return "Detach Request";
   case DETACHRPL:
      return "Detach Reply";
      
   case PROVIDEREQ:
      return "Provide Request";
   case PROVIDERPL:
      return "Provide Reply";
   case UNPROVIDEREQ:
      return "Unprovide Request";
   case UNPROVIDERPL:
      return "Unprovide Reply";
      
   case SVCCALL:
      return "Service Call";
   case SVCFORWARD:
      return "Call Forward";
   case SVCREPLY:
      return "Call Reply";
      
   case EVENT:
      return "Event";
   case EVENTACK:
      return "Event Ack";
      
   case EVENTSUBSCRIBEREQ:
      return "Subscribe Request";
   case EVENTSUBSCRIBERPL:
      return "Subscribe Reply";
   case EVENTUNSUBSCRIBEREQ:
      return "Unsubscribe  Request";
   case EVENTUNSUBSCRIBERPL:
      return "Unsubscribe Reply";

   case ALLOCREQ:
      return "Alloc Request";
   case ALLOCRPL:
      return "Alloc Reply";
   case FREEREQ:
      return "Free Request";
   case FREERPL:
      return "Free Reply";

  default:
      return "????";
   };
};

/** for debugging, it do a DEBUG1() with the decoded IPC message. 
 */
void  _mw_dumpmesg(void * mesg)
{
  long * mtype;
  Attach *am; 
  Provide * pm;
  Call *rm;
  Event *ev;
  Alloc * alloc;
  //  Converse *cm;
  
  mtype = (long *) mesg;
  switch (*mtype) {
  case ATTACHREQ:
  case ATTACHRPL:
  case DETACHREQ:
  case DETACHRPL:
    am = (Attach * ) mesg;
    DEBUG1("%s MESSAGE: %#lx\n\
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
	  messagetype(am->mtype), am->mtype, am->ipcqid, am->pid, 
	  am->server, am->srvname, am->srvid, 
	  am->client, am->cltname, am->cltid, 
	  am->gwid, am->flags, am->returncode);
    return;
    
  case PROVIDEREQ:
  case PROVIDERPL:
  case UNPROVIDEREQ:
  case UNPROVIDERPL:
    pm = (Provide * ) mesg;
    DEBUG1("%s MESSAGE: %#lx\n\
          SERVERID    srvid              =  %#x\n\
          SERVICEID   svcid              =  %#x\n\
          GATEWAYID    gwid              =  %#x\n\
          char        svcname            =  %.32s\n\
          int         cost               =  %d\n\
          int         flags              =  %#x\n\
          int         returncode         =  %d", 
	   messagetype(pm->mtype), pm->mtype, pm->srvid, pm->svcid, pm->gwid, 
	   pm->svcname, pm->cost, pm->flags, pm->returncode);
    return;
  case SVCCALL:
  case SVCFORWARD:
  case SVCREPLY:
    rm = (Call * )  mesg;
    DEBUG1("%s MESSAGE: %#lx\n\
          int         handle             = %d\n\
          CLIENTID    cltid              = %#x\n\
          SERVERID    srvid              = %#x\n\
          SERVICEID   svcid              = %#x\n\
          GATEWAYID   gwid               = %#x\n\
          MWID        callerid           = %#x\n\
          int         forwardcount       = %d\n\
          char        service            = %.32s\n\
          char        origservice        = %.32s\n\
          time_t      issued             = %lxsd\n\
          int         uissued            = %d\n\
          int         timeout            = %d\n\
	  int32_t     datasegmentid      = %d\n\
          int64_t     data               = %lu \"%s\"\n\
          int64_t     datalen            = %zu\n\
          int         appreturncode      = %d\n\
          int         flags              = %#x\n\
          char        domainname         = %.64s\n\
          int         returncode         = %d", 
	   messagetype(rm->mtype), rm->mtype, rm->handle, 
	   rm->cltid, rm->srvid, rm->svcid, rm->gwid, 
	   rm->callerid,
	   rm->forwardcount, rm->service, rm->origservice, 
	   rm->issued, rm->uissued, rm->timeout, 
	   rm->datasegmentid,
	   rm->data, 
	   strdata(_mw_getsegment_byid(rm->datasegmentid), rm->data,  rm->datalen), 
	   rm->datalen, 
	   rm->appreturncode, rm->flags, rm->domainname, rm->returncode);
    return;

  case EVENT:
  case EVENTACK:
    ev = (Event *) mesg;
    DEBUG1("%s MESSAGE: %#lx\n"
	   "          event                          = %.64s\n"
	   "          MWID        eventid            = %#x\n"
	   "          MWID        subscriptionid     = %d\n"
	   "          MWID        senderid           = %#x\n"
	   "            CLIENTID    cltid            = %d\n"
	   "            SERVERID    srvid            = %d\n"
	   "            SERVICEID   svcid            = %d\n"
	   "            GATEWAYID   gwid             = %d\n"
	   "          int32_t     datasegmentid      = %d\n"
	   "          int64_t     data               = %lu \"%s\"\n"
	   "          int64_t     datalen            = %zu\n"
	   "          char        username           = %.64s\n"
	   "          char        clientname         = %.64s\n"
	   "          int         flags              = %#x\n",
	   messagetype(ev->mtype), ev->mtype, ev->event, 
	   ev->eventid,
	   ev->subscriptionid,
	   ev->senderid,
	   CLTID2IDX(ev->senderid),
	   SRVID2IDX(ev->senderid),
	   SVCID2IDX(ev->senderid),
	   GWID2IDX(ev->senderid),
	   ev->datasegmentid,
	   ev->data, 
	   strdata(_mw_getsegment_byid(ev->datasegmentid), ev->data,  ev->datalen),
	   ev->datalen, 
	   ev->username, 
	   ev->clientname, 
	   ev->flags);
    return;

  case EVENTSUBSCRIBEREQ:
  case EVENTSUBSCRIBERPL:
  case EVENTUNSUBSCRIBEREQ:
  case EVENTUNSUBSCRIBERPL:
    ev = (Event *) mesg;
    DEBUG1("%s MESSAGE: %#lx\n"
	   " event                          = %.64s\n"
	   " MWID        subscriptionid     = %d\n"
	   " MWID        senderid           = %#x\n"
	   "   CLIENTID    cltid            = %d\n"
	   "   SERVERID    srvid            = %d\n"
	   "   SERVICEID   svcid            = %d\n"
	   "   GATEWAYID   gwid             = %d\n"
	   " int32_t     datasegmentid      = %d\n"
	   " int64_t     data               = %lu \"%s\"\n"
	   " int64_t     datalen            = %zu\n"
	   " int32_t     flags              = %#x\n"
	   " int32_t     returncode         = %d\n",
	   messagetype(ev->mtype), ev->mtype,  
	   ev->event,
	   ev->subscriptionid,
	   ev->senderid,
	   CLTID2IDX(ev->senderid),
	   SRVID2IDX(ev->senderid),
	   SVCID2IDX(ev->senderid),
	   GWID2IDX(ev->senderid),
	   ev->datasegmentid,
	   ev->data,
	   strdata(_mw_getsegment_byid(ev->datasegmentid), ev->data,  ev->datalen),
	   ev->datalen, 
	   ev->flags, ev->returncode);
    return;

  case ALLOCREQ:
  case ALLOCRPL:
  case FREEREQ:
  case FREERPL:
     alloc = (Alloc *) mesg;
     DEBUG1("%s MESSAGE: %#lx\n"
	    " MWID        senderid           = %#x\n"
	    "   CLIENTID    cltid            = %d\n"
	    "   SERVERID    srvid            = %d\n"
	    "   SERVICEID   svcid            = %d\n"
	    "   GATEWAYID   gwid             = %d\n"
	    " int64_t     buffersize         = %zu\n"
	    " int64_t     pages              = %ld\n"
	    " int         bufferid           = %d\n", 
	    messagetype(alloc->mtype), alloc->mtype,  
	    alloc->mwid,
	    CLTID2IDX(alloc->mwid),
	    SRVID2IDX(alloc->mwid),
	    SVCID2IDX(alloc->mwid),
	    GWID2IDX(alloc->mwid),
	    alloc->size,
	    alloc->pages,
	    alloc->bufferid);
     return;

  default:
    DEBUG1("Unknown message type %#lx ", * (long *) mesg);
    return;
  };
  
};

/**
   Get a message from my IPC message queue. 

   Until we support POSIX IPC we go directly to msg queue and fetch
   messages of the right type. later when we support events
   we need internal queues for sorting requests, events and replys.
   
   This call should have a timeout option.

   @param data the ipc message buffer
   @param *len The len of the data, the actuall bytes is
   returned. This is including the mtype, as opposed to sysv
   msgsnd/msgrcv were length is without the mtype.
   @param type Get a IPC message of a specific type, or any od 0
   Anyway these hide the low level IPC calls. POSIX vs SYSV are hidden here.
   @param flags #MWNOBLOCK

   @return 0 on success or -errno 
*/
int _mw_ipc_getmessage(char * data, size_t *len, int type, int flags)
{
  int rc; 

  if (len == NULL) return -EINVAL;
  *len -= sizeof(long);

  // TODO need to handle propper translation to flags
  DEBUG1("msgrcv type %d flags %x", type, flags);
  if (flags & MWNOBLOCK) flags |= IPC_NOWAIT;

  errno = 0;
  rc = msgrcv(_mw_my_mqid(), data, *len, type, flags);
  /* if we got an interrupt this is OK, of if there was no message in noblock. Else not.*/
  if (rc < 0) {
    if (errno == EINTR) {
      DEBUG1("msgrcv in _mw_ipc_getmessage interrupted");
      return -errno;
    } else if ((errno == ENOMSG) && (flags & IPC_NOWAIT)) {
      DEBUG1("msgrcv in _mw_ipc_getmessage nowait an no message");
      return -errno;
    } else 
    Warning("msgrcv in _mw_ipc_getmessage returned error %d", errno);
    return -errno;
  };
  *len = rc;
  *len += sizeof(long);

  DEBUG1("_mw_ipc_getmessage a msgrcv(type=%d, flags=%d) returned %d and %zu bytes of data", 
	type, flags, rc, *len);
  
  _mw_dumpmesg((void *)data);
  
  return 0;
};

/** 
  Put a message  on an IPC message queue.  
  
  @param dest the #MWID recipient. 
  @param data the pointer to the message
  @param len the length of themessage including the mtype header. 
  @param flags #MWNOBLOCK
  @return 0 on success or -errno */
int _mw_ipc_putmessage(int dest, const char *data, size_t len,  int flags)
{
  int rc; 
  int qid;

  TIMEPEG();

  if (len > MWMSGMAX) {
    Error("_mw_ipc_putmessage: got a to long message %zu > %lu", len, MWMSGMAX);
    return -E2BIG;
  };
  qid = _mw_get_mqid_by_mwid(dest);
  if (qid < 0) {
    Error("_mw_ipc_putmessage: got a request to send a message to %#x by not such ID exists, reason %d", 
	  dest, qid);
    return qid;
  };

  TIMEPEG();
  DEBUG1("_mw_ipc_putmessage: got a request to send a message to  %#x on mqueue %d", dest, qid);
  
  len -= sizeof(long);
  TIMEPEG();
  _mw_dumpmesg((void *) data);
  TIMEPEG();
  rc = msgsnd(qid, data, len, flags);
  TIMEPEG();

  DEBUG1("_mw_ipc_putmessage: msgsnd(dest=%d, msglen=%zu, flags=%#x) returned %d", 
	qid, len, flags, rc);
  /*
    if we got an interrupt this is OK, else not.
    Hmmm, we should have a timeout here. (?)

    TODO: We also should have a gracefull handling of full dest queues 
  */
  if (rc < 0) {
    if (errno == EINTR) {
      DEBUG1("msgsnd in _mw_ipc_putmessage interrupted");
      return 0;
    };
    Warning("msgsnd in _mw_ipc_putmessage returned error %d", errno);
    rc = -errno;
    errno = 0;
    return rc;
  };

  TIMEPEG();

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

/**
   Do an attach on IPC. Used my mwattach() if address is in IPC domain. 

   @todo This function is not used by mwgwd()
   
   @param attachtype an or'ed combinatuon of #MWIPCSERVER and #MWIPCCLIENT. 
   @param name the client name
   @param flags sent to mwd()
   
   @return 0 on success or -errno
 */
int _mw_ipcsend_attach(int attachtype, const char * name, int flags)
{
  
  Attach mesg;
  int rc, error;
  size_t len;

  DEBUG1(	"CALL: _mw_ipcsend_attach (%d, \"%s\", 0x%x)",
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
    Warning("_mw_ipcsend_attach: Attempted to attach as neither client nor server");
    return -EINVAL;
  };
  mesg.pid = getpid();
  mesg.flags = flags;
  mesg.ipcqid = _mw_my_mqid();

  /* THREAD MUTEX BEGIN */
  DEBUG1(	"Sending an attach message to mwd name = %s, client = %s server = %s size = %zu",
	name, mesg.client?"TRUE":"FALSE", mesg.server?"TRUE":"FALSE", sizeof(mesg));

  rc = _mw_ipc_putmessage(0, (void *) &mesg, sizeof(mesg) ,0);

  if (rc != 0) {
    Error(	   "_mw_ipc_putmessage (%d,...)=>%d failed with error %d(%s)", 
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
    Error(	   " _mw_ipc_getmessage failed with error %d(%s)", errno, strerror(errno));
    return -error;
  };
  
  
  DEBUG1(	"Received an attach message reply from mwd \
name = %s, client = %s server = %s srvid=%#x cltid=%#x flags=0x%x rcode=%d",
	name, mesg.client?"TRUE":"FALSE", mesg.server?"TRUE":"FALSE", 
	mesg.srvid, mesg.cltid, mesg.flags, mesg.returncode );
  
  if (mesg.returncode != 0) {
    Error("sysv msg ATTACHREPLY had en error rc=%d", 
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

/**
   Do an attach on IPC. Used my mwattach() if address is in IPC domain. 

   mwd support detaching as either server or client if attached as
   both. However this is not supported in V1.  an detach disconnects
   us completly, This function always succeed, but it is not declared
   void, we really should have some error hendling, but we need rules
   how to to handle errors.

   the force flags tell us to set the force flag, and not expect a
   reply.  It tell the mwd that we're going down, really an ack to a
   message queue removal, mwd uses that to inform us of a shutdown.

   Maybe a bad design/imp desicion, but I need a seperate call to
   detach an indirect client, networked, even if _mw_ipcsend_attach()
   do both. Maybe we should have a separate attach?. Fortunately
   _mw_ipcsend_detach() is almost directly mapped on
   _mw_ipcsend_detach_indirect() 
   
   @param cid detach the client with cid
   @param sid detach the server with sid
   @param force mwd() will not reply to the request, mwd() will just do it immediately
   
   @return 0 on success or -errno
*/
int _mw_ipcsend_detach_indirect(CLIENTID cid, SERVERID sid, int force) 
{
  Attach mesg;
  int rc;
  size_t len;

  DEBUG1("CALL: _mw_ipcsend_detach_indirect()");
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
  DEBUG1(	"Sending a detach message client=%s server=%s", 
	mesg.client?"TRUE":"FALSE", mesg.server?"TRUE":"FALSE");
  rc =  _mw_ipc_putmessage(0, (void *) &mesg, sizeof(mesg),0);
  
  if (rc != 0) {
    Warning(	   "_mw_ipc_putmessage(%d,...)=>%d failed with error %d(%s)", 
	   _mw_mwd_mqid(), rc, errno, strerror(errno));
    /* THREAD MUTEX END */
    return 0;
  };

  if (!force) {
    len = sizeof(mesg);
    rc = _mw_ipc_getmessage ( (void *) &mesg, &len , DETACHRPL, 0);
    
    /* THREAD MUTEX END */
    
    if (rc == -1) {
      Warning(	     "_mw_ipc_getmessage failed with error %d(%s)", errno, strerror(errno));
      return 0;
    };
    
    
    DEBUG1(	  "Received a detach message reply from mwd client = %s server = %s rcode=%d",
	  mesg.client?"TRUE":"FALSE", mesg.server?"TRUE":"FALSE", 
	  mesg.returncode );
    if (mesg.returncode != 0) {
      Warning("sysv msg ATTACHREPLY had en error rc=%d", 
	     mesg.returncode);
    };
  }; /* !force */

  return 0;
};

/** see _mw_ipcsend_detach_indirect() */
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


/**
   A provide for imported services. 
   We need this for gateways where a gw has entries in the gwtbl for
   foreign/peer gateways, and the gwid shall be the gwid for the
   foreign, and not the local gw.  We made however a more general
   function here so that in the future we can conduct such madness as
   having a single process be more than one server.

   @param mwid the MWID for the provider (server or gateway)
   @param servicename The service name
   @param cost The routing cost for imported services, 1 if local. 
   @param flags sent on in Provide. 

   @return the actual bytes in the sent message, or -errno
*/
int _mw_ipcsend_provide_for_id(MWID mwid, const char * servicename, int cost, int flags)
{
  Provide providemesg;
  int rc;
  
  memset(&providemesg, '\0', sizeof(Provide));
  
  providemesg.mtype = PROVIDEREQ;
  providemesg.srvid = SRVID(mwid);
  providemesg.gwid = GWID(mwid);
  providemesg.cost = cost;
  providemesg.svcid = UNASSIGNED;
  strncpy(providemesg.svcname, servicename, MWMAXSVCNAME);
  providemesg.flags = flags;

  if ( (providemesg.srvid == UNASSIGNED) && 
       (providemesg.gwid == UNASSIGNED) ) {
    Error("both srvid and gwid is unassigned mwid = %#x", mwid);
    return -EINVAL;
  };
  
  DEBUG1("Sending a provide message for service %s",  providemesg.svcname);
  /* THREAD MUTEX BEGIN */
  rc = _mw_ipc_putmessage(0, (char *) &providemesg, 
			  sizeof(Provide),0);
  if (rc < 0) {
    Error("mwprovide() failed with rc=%d",rc);
    /* MUTEX END */
  };
  return rc;
};

/** Send a Provide message. 

    @param service name the service name
    @param cost The routing cost for imported services, 1 if local. 
    @param flags sent on in Provide. 
    
    @return the actual bytes in the sent message, or -errno
 */
int _mw_ipcsend_provide(const char * servicename, int cost, int flags)
{
  SERVERID srvid;
  GATEWAYID gwid;
  
  srvid =  _mw_get_my_serverid();
  if (srvid != UNASSIGNED) 
    return _mw_ipcsend_provide_for_id(srvid, servicename, cost, flags);
 
  gwid = _mw_get_my_gatewayid();
  return _mw_ipcsend_provide_for_id(srvid, servicename, cost, flags);
};

/** The IPC version of mwprovide(). 
    @param service name the service name
    @param flags sent on in Provide. 
    @return the SERVICEID or -errno
 */
SERVICEID _mw_ipc_provide(const char * servicename, int flags)
{
  Provide providemesg;
  int rc, error;
  size_t len;


  rc = _mw_ipcsend_provide(servicename, /* cost= */ 0, flags);
  if (rc < 0) return rc;

  /* Add timeout */
  len = sizeof(Provide);
  rc = _mw_ipc_getmessage((char *) &providemesg, &len, PROVIDERPL, 0);
  /* MUTEX END */
  if (rc == -1) {
    Error("Failed to get a reply to a provide request error %d(%s)", 
	   errno, strerror(errno));
    return -error;
  };
  DEBUG1("Received a provide reply with service id %x and rcode %d",
	providemesg.srvid, providemesg.returncode);

  if (providemesg.svcid < 0) return providemesg.returncode;
  return providemesg.svcid;
};

/** Send an unprovide request. 
    @param service name the service name
    @param svcid the SERVICEID of the provided service)
    @return 0 or -errno
*/
int _mw_ipcsend_unprovide(const char * servicename,  SERVICEID svcid)
{
  SERVERID srvid;
  GATEWAYID gwid;
  
  srvid =  _mw_get_my_serverid();
  if (srvid != UNASSIGNED) 
    return _mw_ipcsend_unprovide_for_id(srvid, servicename, svcid);
 
  gwid = _mw_get_my_gatewayid();
  return _mw_ipcsend_unprovide_for_id(srvid, servicename, svcid);
};

/** Send an unprovide for imported services. 
    @param mwid the gateway id for the importing gateway.
    @param service name the service name
    @param svcid the SERVICEID of the provided service)
    @return 0 or -errno
*/
int _mw_ipcsend_unprovide_for_id(MWID mwid, const char * servicename,  SERVICEID svcid)
{
  Provide unprovidemesg;
  int rc;

  memset(&unprovidemesg, '\0', sizeof(Provide));
  
  unprovidemesg.mtype = UNPROVIDEREQ;
  unprovidemesg.srvid = SRVID(mwid);
  unprovidemesg.gwid = GWID(mwid);
  unprovidemesg.svcid = svcid;
  unprovidemesg.flags = 0;

  if ( (unprovidemesg.srvid == UNASSIGNED) && 
       (unprovidemesg.gwid == UNASSIGNED) ) {
    Error("both srvid and gwid is unassigned mwid = %#x", mwid);
    return -EINVAL;
  };

  rc = _mw_ipc_putmessage(0, (char *) &unprovidemesg, 
			  sizeof(Provide),0);
  if (rc < 0) {
    Error("mwunprovide() failed with rc=%d",rc);
    return rc;
  };
  DEBUG1("sent request for unprovide %s with serviceid=%x to mwd", 
	servicename, SVCID2IDX(svcid));

  return rc;
};

/** The IPC version of mwunprovide(). 
    @param service name the service name
    @param svcid the SERVICEID of the provided service)
    @return 0 or -errno
*/
int _mw_ipc_unprovide(const char * servicename,  SERVICEID svcid)
{
  Provide unprovidemesg;
  int rc;
  size_t len;

  rc = _mw_ipcsend_unprovide(servicename, svcid);

  len = MWMSGMAX;
  rc = _mw_ipc_getmessage((char *) &unprovidemesg, &len, UNPROVIDERPL, 0);
  DEBUG1("got reply for unprovide %s with rcode=%d serviceid = %x (rc=%d)", 
	servicename, unprovidemesg.returncode, svcid, rc);
  
  return unprovidemesg.returncode;
};

/**
   Get the MWID of the client (caller). Which is the cid if cid !=
   UNASSIGNED, gwid else.
*/

MWID _mw_get_caller_mwid(Call * cmsg)
{
   if (cmsg->cltid != UNASSIGNED) 
      return cmsg->cltid;
   else 
      return cmsg->gwid;
};

/**
  The IPC implementation of mwacall(). This is used by mwgwd() as well as IPC clients. 

  @todo caller is is obsolete. 

  @param svcname the service name
  @param data The passed data in the call, NULL if no data
  @param datalen the number of bytes in data, 0 if data is NULL. 
  @param flags non blocking if or'ed with #MWNOBLOCK, rest is passed on in call request. 
  @param mwid mwgwd sets mwid for the srb client or gwid for peer gateway.  The mwid shall be set to
  #UNASSIGNED for IPC clients. This function will user the current clientid or gatewayid. 

  @param domain the remote domain, our own domain is used if NULL
  @param instance the remote instance, our own instace is used if NULL

  @param callerid is the id on the peer gateways side. It's must be kept here
  since we must passit back to the peer gateway. It's done this way
  because the peer gateway don't need to remember the call, and hence
  reduses the time the peer gateway uses to process the reply (no
  lookups) 

  @param hops the number if gateways this request has route thru. 
  
  @return see mwacall() man page. 
*/
int _mwacallipc (const char * svcname, const char * data, size_t datalen, int flags, 
		 MWID mwid, const char * instance, const char * domain, MWID callerid, int hops)
{
  int dest; 
  int rc;
  int hdl, n, idx;
  Call callmesg;
  struct timeval tm;
  float timeleft;
  SERVICEID * svclist;

  TIMEPEG();

  DEBUG1("BEGIN: (svcname=%s, data=%p, datalen=%zu, flags=%#x, mwid=%#x, instance=%s, domain=%s, "
	 "callerid=%d, hops=%d", svcname, data, datalen, flags, mwid, 
	 instance?instance:"(NULL)", domain?domain:"(NULL)", callerid, hops);

  memset (&callmesg, '\0', sizeof(Call));
  errno = 0;

  /* set up the message */  
  gettimeofday(&tm, NULL);
  callmesg.issued = tm.tv_sec;
  callmesg.uissued = tm.tv_usec;
  /* mwbegin() is the tuxedo way of set timeout. We use it too
     The nice feature is to set exactly the same deadline on a set of mwacall()s */
    
  if ( _mw_deadline(NULL, &timeleft)) {
    /* we should maybe say that a few ms before a deadline, we call it expired? */
    if (timeleft <= 0.0) {
      /* we've already timed out */
      DEBUG1("call to %s was made %f ms after deadline had expired", 
	    svcname, timeleft);
      return -ETIME;
    };
    /* timeout is here in ms */
    callmesg.timeout = (int) (timeleft * 1000);
  } else {
    callmesg.timeout = 0; 
  };

  TIMEPEG();
  DEBUG1("doing mwid's");
  /* now the data string are passed in shm, we only pass the address (offset) , and 
     the other neccessary data.
     since shmat on Linux do not always return the same address for a given shm segment
     we must operate on offset into the segment (se shmalloc.c) */
  
  callmesg.mtype = SVCCALL;

  if (mwid == UNASSIGNED) {
    callmesg.cltid = _mw_get_my_clientid();
    callmesg.gwid = _mw_get_my_gatewayid();
  } else {
    callmesg.cltid = CLTID(mwid);
    callmesg.gwid = GWID(mwid);
  } 
  callmesg.srvid = UNASSIGNED;
  strncpy (callmesg.service, svcname, MWMAXSVCNAME);
  strncpy (callmesg.origservice, svcname, MWMAXSVCNAME);
  callmesg.forwardcount = 0;

  TIMEPEG();

  /* should this be a sequential number for each server(thread) */
  hdl = _mw_nexthandle();

  /* when properly implemented some flags will be used in the IPC call */
  callmesg.handle = hdl;
  callmesg.flags = flags;
  callmesg.appreturncode = 0;
  callmesg.returncode = 0;

  TIMEPEG();

  if (instance) {
    strncpy(callmesg.instance, instance, MWMAXNAMELEN);
  } else {
    callmesg.instance[0] = '\0';
  };

  if (domain) {
    strncpy(callmesg.domainname, domain, MWMAXNAMELEN);
  } else {
    callmesg.domainname[0] = '\0';
  };

  callmesg.callerid = callerid;
  callmesg.hops = hops;

  TIMEPEG();
  DEBUG1("doing data");

 // if there is not available buffers,  we try for a while
  do {
     int n; 
     n = 0;
     
     rc = _mw_putbuffer_to_call(&callmesg, data, datalen);

     if (rc) {
	if (n >= 100) return rc;
	if (n == 0) {
	   Warning("Reply: failed to place reply buffer rc=%d", rc);
	};
	usleep(20);
	n++;
     }
  } while (rc != 0);
  
  DEBUG1("dataoffset %lu length %zu",   callmesg.data, callmesg.datalen);

  TIMEPEG();
  DEBUG1("getting available servers");
  svclist = _mw_get_services_byname(svcname, &n, flags);
  if (svclist == NULL) return -ENOENT;

  TIMEPEG();

  while(n > 0) {
    
    idx = (rand() % n);
    DEBUG1("selecting %d of %d = %d", idx, n, SVCID2IDX(svclist[idx]));
    callmesg.svcid = svclist[idx];

    TIMEPEG();
    
    dest = _mw_get_provider_by_serviceid (callmesg.svcid);
    if (dest < 0) {
      Warning("mw(a)call(): getting the serverid for serviceid %#x(%s) failed reason %d",
	      callmesg.svcid, svcname, dest);
      continue;
    };
 
    DEBUG1("Sending a ipcmessage to serviceid %#x service %s on server %#x my clientid %#x "
	   "buffer at offset %lu len %zu ", 
	   callmesg.svcid, callmesg.service, dest, callmesg.cltid, callmesg.data, callmesg.datalen);

    TIMEPEG();

    rc = _mw_ipc_putmessage(dest, (char *) &callmesg, sizeof (Call), 0);
    
    DEBUG1("_mw_ipc_putmessage returned %d handle is %d", rc, hdl);
    if (rc >= 0) {
      rc = hdl;
      goto out;
    };

    TIMEPEG();  

    /* if we got here, there was a failure, remove the svcid we just tried from the svclist, end we retry */
    n--;
    svclist[idx] = svclist[n];
  };
 out:
  TIMEPEG();

  free(svclist);
  
  TIMEPEG();
  DEBUG1("done");
  return rc;
};
    


/** The IPC version of mwfethc(). The mwfetch() primary purpose is to
   get the reply to a specific mwacall().  In that case the handle is
   what mwacall() returned.  However if handle is 0 we can use
   mwfetch() to get service calls in servers. Thus this IPC
   implementation are a bit diffrent from the TCP one.

   callmesg is stored globally so that _mwreply can retived info that 
   may not survive the service handle functon 

   \e NB!!!!! NOT THREAD SAFE!!!!

   @param *hdl The IPC call handle, if the value if hdl is 0, the actual handle is returned here. 
   @param *data a pointer to a pointer which will point to a data buffer or have the value NULL if no data was returned in this reply. 
   @param *len a point to an integer that will be set to the number of bytes in the returned buffer, or 0 if no data was returned in this reply. 
   @param *appreturncode The application return code. The returned value has special meaning to MidWay, this value is private to the application. 
   @param flags may be #MWNOBLOCK
   @return the call return code one of either #MWSUCCESS, #MWFAIL, or #MWMORE
*/
int _mwfetchipc (mwhandle_t * hdl, char ** data, size_t * len, int * appreturncode, int flags)
{
  int rc;
  char * buffer;
  struct timeval tv;
  Call * callmesg;
  MWID id;
  mwhandle_t handle = *hdl;
  seginfo_t * si;

  /*  if (handle == 0) return -NMWNYI;*/
  
  DEBUG1(" called for handle %d", handle);

  /* first we check in another call to _mwfetch() retived it and placed 
     in the internal queue.*/
  callmesg = popCallReplyByHandle(handle);

  /* prepare for while loop */
  if (callmesg == NULL) {
    *len = sizeof(Call);
    buffer = malloc(MWMSGMAX);
  };

  while (callmesg == NULL) {
    /* get the next message of type SVCREPLY of teh IPC queue. */
    rc = _mw_ipc_getmessage(buffer, len, SVCREPLY, flags);
    if (rc != 0) {
      DEBUG1("returned with error code %d", rc);
      free (buffer);
      return rc;
    };
    
    /* we now got the first available reply of the IPC message queue.
       In it wasn't the one we was waiting for push it to the internal queue. */
    callmesg = (Call *) buffer ;
    if (handle && (callmesg->handle != handle)) {
      
      gettimeofday(&tv, NULL);
      /* test to see if message missed deadline. NOTE if the message
         we were waiting for, we do not test deadline. */
      if ( ((callmesg->issued + callmesg->timeout/1000) < tv.tv_sec) &&  
	   ((callmesg->uissued + callmesg->timeout%1000) < tv.tv_usec) ) {
	DEBUG1("Got a reply with handle %d, That had expired, junking it.",
	      callmesg->handle);
      };
      
      DEBUG1("Got a reply I was not awaiting with handle %d, enqueuing it internally", callmesg->handle);
      if (pushCallReply(callmesg) != 0) {
	Error("ABORT: failed in enqueue on internal buffer, this can't happen");
	exit(8);
      };
      callmesg = NULL;
      buffer = malloc(MWMSGMAX);
      continue;
    };
    
  };
  
  /* we now have the requested reply */
  DEBUG1("Got a message of type %#lx handle %d ", 
	callmesg->mtype, callmesg->handle);
  /* Retriving info from the message 
     If fastpath we return pointers to the shm area, 
     else we copy to private heap.
  */

  if (*data != NULL) free (*data);
  
  if (callmesg->data) {     
     id = 0;
     si = _mw_getsegment_byid(callmesg->datasegmentid);
     
     if (si == NULL) {
	Error ("failed to get the buffer accompanying the call");
	return -EFAULT;
     };

     
     if (! _mw_fastpath_enabled()) {
	void * shmbuf;
	DEBUG3("copying data to local byffer, freeing shm buffer");
	*data = malloc(callmesg->datalen+1);
	shmbuf = _mwoffset2adr(callmesg->data, si);
	memcpy(*data, shmbuf, callmesg->datalen);
	(*data)[callmesg->datalen] = '\0';

	_mwshmgetowner(shmbuf, &id);	
	DEBUG1("owner id of data is %s", _mwid2str(id, NULL));
	_mwfree(shmbuf);
     } else {
	*data = _mwoffset2adr(callmesg->data, si);
	_mwshmgetowner(*data, &id);	
     }; 
     DEBUG1("owner id of data is %s", _mwid2str(id, NULL));
     *len = callmesg->datalen;
  } else {
     *data = NULL;
     *len = 0;
  };

  *appreturncode = callmesg->appreturncode;
  *hdl = callmesg->handle;

  /* deadline info is invalid even though I can provide it */

  DEBUG1("returned with returncode=%d and with %zu bytes of data", 
	callmesg->returncode, callmesg->datalen);

  if (callmesg->returncode > MWMORE) 
    return -callmesg->returncode;
  return callmesg->returncode;
};

/************************************************************************/

/* EVENTS */

/**
   The IPC version of mwsubscribe().
   
   @param pattern The pattern of event name to subscribe on. 
   @param subid the subscription id of this subscription. 
   @param flags either #MWEVSTRING, #MWEVGLOB, #MWEVREGEXP, or #MWEVEREGEXP

   @return 0 on success or -errno
*/
int _mw_ipc_subscribe(const char * pattern, int subid, int flags)
{
  Event ev;
  int rc;
  size_t len;

  rc = _mw_ipcsend_subscribe (pattern, subid, flags);

  if (rc != 0) {
    Error("_mw_ipcsend_subscribe failed with %d", rc);
    return rc;
  };

  len = MWMSGMAX;
  rc = _mw_ipc_getmessage((char *) &ev, &len, EVENTSUBSCRIBERPL, 0);

  if (rc != 0) {
    Error("_mw_ipc_getmessage failed with %d", rc);
    return rc;
  };

  DEBUG1("got reply for subscribe %s id %d with rcode=%d (rc=%d)", 
	pattern, subid, ev.returncode, rc);
  
  return ev.returncode;
};

/**
   The IPC version of mwsubscribe().
   
   @param subid the subscription id of the subscription. 
   @return 0 on success or -errno
*/
int _mw_ipc_unsubscribe(int subid)
{
  Event ev;
  int rc;
  size_t len;

  rc = _mw_ipcsend_unsubscribe (subid);

  len = MWMSGMAX;
  rc = _mw_ipc_getmessage((char *) &ev, &len, EVENTUNSUBSCRIBERPL, 0);
  DEBUG1("got reply for unsubscribe %d with rcode=%d (rc=%d)", 
	 subid, ev.returncode,  rc);
  
  return ev.returncode;
};

/**
   send an event subscription on IPC (to mwd()). Do now wait for reply

   @param pattern The pattern of event name to subscribe on. 
   @param subid the subscription id of this subscription. 
   @param flags either #MWEVSTRING, #MWEVGLOB, #MWEVREGEXP, or #MWEVEREGEXP

   @return 0 on success or -errno  
*/
int _mw_ipcsend_subscribe (const char * pattern, int subid, int flags)
{
  MWID id;
  int  dest; 
  int rc;
  int pattlen;
  char * dbuf;
  Event ev;

  memset (&ev, '\0', sizeof(Event));
  errno = 0;

  dest = MWD_ID; 

  if  (pattern == NULL) return -EINVAL;
  if  (subid < 0) return -EINVAL;

  DEBUG1("Sending a ipcmessage to mwd subscribe %s subid %d  flags %#x ", 
	 pattern, subid, flags);

  ev.mtype = EVENTSUBSCRIBEREQ;
  ev.subscriptionid = subid;
  ev.flags = flags;

  id = _mw_get_my_mwid();
  if (id == UNASSIGNED) {	
	return -ENOENT;
  }
  ev.senderid = id;
  
  /* the pattern may be longer than MWMAXNAMELEN for globs and
     regex. if so we pass the pattern in a buffer, and not the event
     field. */
  pattlen = strlen(pattern);
  if (pattlen > MWMAXNAMELEN) {     
    
    DEBUG1("pattern longer than MWMAXLEN (%d > %d) passing pattern in a buffer", 
	   pattlen, MWMAXNAMELEN);

    dbuf = _mwalloc(pattlen+1);
    if (dbuf == NULL) {
      Error("Subscribe: mwalloc(%d) failed reason %d", pattlen, (int) errno);
      return -errno;
    };
    memcpy(dbuf, pattern, pattlen);
    dbuf[pattlen] = '\0';
    ev.data = _mwshmcheck(dbuf);      
    ev.datalen = pattlen;
  } else {
    ev.data = 0;
    ev.datalen = 0;
    memcpy(ev.event, pattern, pattlen);
  };
  
  rc = _mw_ipc_putmessage(dest, (char *) &ev, sizeof (Event), 0);
  
  DEBUG1("_mw_ipc_putmessage returned %d", rc);

  return rc;
};
    
/**
   send an event unsubscription on IPC (to mwd()). Do now wait for reply

   @param subid the subscription id of the subscription. 
   @return 0 on success or -errno
*/
int _mw_ipcsend_unsubscribe (int subid)
{
  MWID id;
  int  dest; 
  int rc;
  Event ev;

  memset (&ev, '\0', sizeof(Event));
  errno = 0;

  dest = MWD_ID; 

  if  (subid < 0) return -EINVAL;

  DEBUG1("Sending a ipcmessage to mwd unsubscribe subid %d  ", 
	 subid);

  ev.mtype = EVENTUNSUBSCRIBEREQ;
  ev.subscriptionid = subid;
  ev.flags = 0;

  id = _mw_get_my_mwid();
  if (id == UNASSIGNED) {	
	return -ENOENT;
  }
  ev.senderid = id;

  rc = _mw_ipc_putmessage(dest, (char *) &ev, sizeof (Event), 0);
  
  DEBUG1("_mw_ipc_putmessage returned %d", rc);

  return rc;
};

/**
   send an event on IPC (to mwd()). 

   @param event The event name, may not be NULL
   @param data Associated data, may be NULL if datalen == 0
   @param datalen the number of bytes in data
   @param username if not NULL, the username we're sending an event to
   @param clientname if not NULL, the clientname we're sending an event to
   @param fromid if #UNASSIGNED we use our own clientid, intened for mwgwd() to set clientid of the network cliennt sending the event, or the gateway for imported events.  
   @param remoteflag if the event came from a peer gateway.
   @return 0 on success or -errno
*/

int _mw_ipcsend_event (const char * event, const char * data, size_t datalen,
		       const char * username, const char * clientname, 
		       MWID fromid, int remoteflag)
{
  MWID id;
  int  dest; 
  int rc;
  int dataoffset;
  char * dbuf;
  Event ev;

  memset (&ev, '\0', sizeof(Event));
  errno = 0;

  dest = MWD_ID;
  
  
  /* now the data string are passed in shm, we only pass the address (offset) , and 
     the other neccessary data.
     since shmat on Linux do not always reurn tha same address for a given shm segment
     we must operate on offset into the segment (se shmalloc.c) */

  if (data != NULL) {
     dataoffset = _mwshmcheck((void*)data);
    if (dataoffset == -1) {
      dbuf = _mwalloc(datalen);
      if (dbuf == NULL) {
	Error("Send Event: mwalloc(%zu) failed reason %d", datalen, (int) errno);
	return -errno;
      };
      memcpy(dbuf, data, datalen);
      dataoffset = _mwshmcheck(dbuf);
    };
  } else {
    dataoffset = 0;
    datalen = 0;
  };
  /* else 
     we really should check to see if datalen is longer than buffer 
  */
  
  ev.mtype = EVENT;
  ev.data = dataoffset;
  ev.datalen = datalen;

  if (remoteflag)
     ev.flags = MWEVENTPEERGENERATED;

  if (fromid == UNASSIGNED) 
     id = _mw_get_my_mwid();
  else 
     id = fromid;

  if (id == UNASSIGNED) {	
    return -ENOENT;
  }
  ev.senderid = id;
  ev.subscriptionid = UNASSIGNED;

  strncpy (ev.event, event, MWMAXNAMELEN);
  if (username != NULL) 
    strncpy (ev.username, username, MWMAXNAMELEN);
  else 
    ev.username[0] = '\0';
  if (clientname != NULL) 
    strncpy (ev.clientname, clientname, MWMAXNAMELEN);
  else 
    ev.clientname[0] = '\0';
  
  if (remoteflag) ev.flags |= MWEVENTPEERGENERATED;

  DEBUG1("Sending a ipcmessage to mwd event %s id %#x buffer at offset %lu len %zu ", 
	 ev.event, ev.senderid, ev.data, ev.datalen);

  rc = _mw_ipc_putmessage(dest, (char *) &ev, sizeof (Event), 0);
  
  DEBUG1("_mw_ipc_putmessage returned %d", rc);

  return rc;
};

/**
   Get an event of the IPC message queue. This is non blocking and
   will return -EAGAIN if no event are in the queue.

   @param ev A pointer to an allocated Event. 
   
   @return 0 on success -errno else, normally -EAGAIN if no events. 
*/
int _mw_ipc_getevent(Event * ev)
{
  size_t len;
  int rc;
  if (ev == NULL) return -EINVAL;
  len = sizeof(Event);
  rc = _mw_ipc_getmessage((char *) ev, &len, EVENT, IPC_NOWAIT);
  if (rc < 0) return rc;
  if (len == sizeof(Event)) {
    return 0;
  };
  Error("Got an event ipc message, but with wrong length! %zu != %zu", len, sizeof(Event));
  return -EMSGSIZE;
};
      
/************************************************************************/

/** Get the number of messages in my IPC message queue. 
    
@return the number of mesage of -errno on error
*/
int _mwCurrentMessageQueueLength()
{
  struct msqid_ds mqstat;
  int rc;

  rc = msgctl(_mw_my_mqid(), IPC_STAT, &mqstat);
  if (rc != 0) return -errno;

  return mqstat.msg_qnum;
};

/**
   Send a shutdown command to mwd. 

   @param delay The grace periode for shutdown in seconds. 

   @return 0 on success, -errno on error. 
*/
int _mw_shutdown_mwd(int delay)
{
  
  Administrative mesg;
  int rc;
  ipcmaininfo * ipcmain = NULL;

  if (_mw_isattached() == 0) return -ENOTCONN;
  ipcmain = _mw_ipcmaininfo();
  if (ipcmain == NULL) return -EADDRNOTAVAIL;

  DEBUG1("CALL: _mw_shutdown:mwd(%d)", delay);

  mesg.mtype = ADMREQ;
  mesg.opcode = ADMSHUTDOWN;
  mesg.cltid = _mw_get_my_clientid();

  /* THREAD MUTEX BEGIN */
  DEBUG1(	"Sending a shutdown message to mwd");

  rc =  _mw_ipc_putmessage(0, (void *) &mesg, sizeof(mesg),0);
  
  if (rc != 0) {
    Warning(	   "_mw_ipc_putmessage(%d,...)=>%d failed with error %d(%s)", 
	   0, rc, errno, strerror(errno));
    /* THREAD MUTEX END */
    return 0;
  };

  /* THREAD MUTEX END */
  
  
  return 0;
};
