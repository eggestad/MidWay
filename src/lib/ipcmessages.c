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
 * 
 * $Log$
 * Revision 1.25  2003/07/20 23:12:06  eggestad
 * - in acall, corrected handling of data ptr to offset and buffer alloc
 * - added usleep() loop if no buffers are available
 *
 * Revision 1.24  2003/07/08 19:23:43  eggestad
 * critcal stack corruption error in _mwfetchipc()
 *
 * Revision 1.23  2003/07/06 22:11:40  eggestad
 * - added timepegs
 * - added debuggig
 * - added data in dump of messages
 *
 * Revision 1.22  2003/06/26 17:03:53  eggestad
 * - reworked the call request queue to FIFO
 *  - added data contents in message dump
 *
 * Revision 1.21  2003/06/19 21:45:31  eggestad
 * fix of pop of pending received call reply
 *
 * Revision 1.20  2003/06/12 07:43:21  eggestad
 * added MWIPCONLY flag to _mwipcacall, to force local service
 *
 * Revision 1.19  2003/03/16 23:53:53  eggestad
 * bug fixes
 *
 * Revision 1.18  2002/10/22 21:58:20  eggestad
 * Performace fix, the connection peer address, is now set when establised, we did a getnamebyaddr() which does a DNS lookup several times when processing a single message in the gateway (Can't believe I actually did that...)
 *
 * Revision 1.17  2002/10/20 18:11:25  eggestad
 * - added instance to the Call struct. Needed later for gateway to gateway calls
 * - fixup for setting the clientid and gatewayid propperly in Call struct l
 * - bad bug in copying domain into the Call struct
 *
 * Revision 1.16  2002/10/17 22:04:46  eggestad
 * - added more field to _mwacallipc() needed for gateway to gateway calls
 * - Call struct now also has callerid and hops fields
 *
 * Revision 1.15  2002/10/06 23:54:15  eggestad
 * - updated  mwacall so  we  also call  gateways.  We now  also try  all
 *   services  with given service  name. We  don't favour  local services
 *   over remote, something for the future
 *
 * Revision 1.14  2002/10/03 21:04:03  eggestad
 * - split  up ipc_(un)provide()  and  created ipc_(un)provide_for_id  so
 *   gateways can send (un)provides on behalf on foreign gateways
 *
 * Revision 1.13  2002/09/26 22:34:34  eggestad
 * added cost to the debug dump of provide message
 *
 * Revision 1.12  2002/09/22 23:01:16  eggestad
 * fixup policy on *ID's. All ids has the mask bit set, and purified the consept of index (new macros) that has the mask bit cleared.
 *
 * Revision 1.11  2002/08/09 20:50:15  eggestad
 * A Major update for implemetation of events and Task API
 *
 * Revision 1.10  2002/07/07 22:35:20  eggestad
 * *** empty log message ***
 *
 * Revision 1.9  2002/02/17 13:56:50  eggestad
 * - debugging fixup
 * - MWMORE is now a return code, not input flag
 *
 * Revision 1.8  2001/09/15 23:59:05  eggestad
 * Proper includes and other clean compile fixes
 *
 * Revision 1.7  2001/05/12 18:00:31  eggestad
 * changes to multiple reply handling, MWMULTIPLE are no langer sent to server, replies are cat'ed in client
 *
 * Revision 1.6  2000/11/15 21:23:38  eggestad
 * fix for NULL as input data
 *
 * Revision 1.5  2000/09/24 14:05:01  eggestad
 * fixed to warning messages
 *
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

static char * RCSId UNUSED = "$Id$";


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

   DEBUG1(" Callreply queue length = %d free %d", q, f);   
   for (elm = callReplyQueue; elm != NULL; elm = elm->next) q++;
   DEBUG1(" Callreply queue length = %d free %d", q, f);
   for (elm = callReplyFreeList; elm != NULL; elm = elm->next) f++;
   
   DEBUG1(" Callreply queue length = %d free %d", q, f);
};
 
static int pushCallReply(Call * callmsg)
{
   struct CallReplyQueue * newelm;

   DEBUG1("pushing call with handle %d", callmsg->handle);
   debugReplyQueue();

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
   newelm->next = callReplyQueue;
   callReplyQueue = newelm;
   DEBUG1("newelm %p callReplyQueue %p newelm->next %p", newelm, callReplyQueue, newelm->next);
   debugReplyQueue();
   return 0;
};

static Call * popCallReplyByHandle(int handle)
{
   struct CallReplyQueue ** pelm, *elm;
   Call * callptr = NULL;

   DEBUG1("poping call with handle %d", handle);

   debugReplyQueue();

   if (callReplyQueue == NULL) goto out;

   if (handle == -1) {
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

static char * strdata(int dataoff, int len)
{
   static char data[80];
   char * rdata;
   int i, l;
   rdata = _mwoffset2adr(dataoff);
   
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
	  
/* for debugging purposes: */
void  _mw_dumpmesg(void * mesg)
{
  long * mtype;
  Attach *am; 
  Provide * pm;
  Call *rm;
  Event *ev;
  //  Converse *cm;
  
  mtype = (long *) mesg;
  switch (*mtype) {
  case ATTACHREQ:
  case ATTACHRPL:
  case DETACHREQ:
  case DETACHRPL:
    am = (Attach * ) mesg;
    DEBUG1("ATTACH MESSAGE: %#x\n\
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
    DEBUG1("PROVIDEMESSAGE: %#x\n\
          SERVERID    srvid              =  %#x\n\
          SERVICEID   svcid              =  %#x\n\
          GATEWAYID    gwid              =  %#x\n\
          char        svcname            =  %.32s\n\
          int         cost               =  %d\n\
          int         flags              =  %#x\n\
          int         returncode         =  %d", 
	   pm->mtype, pm->srvid, pm->svcid, pm->gwid, 
	   pm->svcname, pm->cost, pm->flags, pm->returncode);
    return;
  case SVCCALL:
  case SVCFORWARD:
  case SVCREPLY:
    rm = (Call * )  mesg;
    DEBUG1("CALL/FRD/REPLY MESSAGE: %#x\n\
          int         handle             = %d\n\
          CLIENTID    cltid              = %#x\n\
          SERVERID    srvid              = %#x\n\
          SERVICEID   svcid              = %#x\n\
          GATEWAYID   gwid               = %#x\n\
          MWID        callerid           = %#x\n\
          int         forwardcount       = %d\n\
          char        service            = %.32s\n\
          char        origservice        = %.32s\n\
          time_t      issued             = %d\n\
          int         uissued            = %d\n\
          int         timeout            = %d\n\
          int         data               = %d \"%s\"\n\
          int         datalen            = %d\n\
          int         appreturncode      = %d\n\
          int         flags              = %#x\n\
          char        domainname         = %.64s\n\
          int         returncode         = %d", 
	   rm->mtype, rm->handle, 
	   rm->cltid, rm->srvid, rm->svcid, rm->gwid, 
	   rm->callerid,
	   rm->forwardcount, rm->service, rm->origservice, 
	   rm->issued, rm->uissued, rm->timeout, 
	   rm->data, 
	   strdata(rm->data,  rm->datalen), 
	   rm->datalen, 
	   rm->appreturncode, rm->flags, rm->domainname, rm->returncode);
    return;

  case EVENT:
  case EVENTACK:
    ev = (Event *) mesg;
    DEBUG1("EVENT/EVENTACK MESSAGE: %#x\n"
	   "          event                          = %.64s\n"
	   "          MWID        eventid            = %#x\n"
	   "          MWID        subscriptionid     = %d\n"
	   "          MWID        senderid           = %#x\n"
	   "            CLIENTID    cltid            = %d\n"
	   "            SERVERID    srvid            = %d\n"
	   "            SERVICEID   svcid            = %d\n"
	   "            GATEWAYID   gwid             = %d\n"
	   "          int         data               = %d \"%s\"\n"
	   "          int         datalen            = %d\n"
	   "          char        username           = %.64s\n"
	   "          char        clientname         = %.64s\n"
	   "          int         flags              = %#x\n",
	   ev->mtype, ev->event, 
	   ev->eventid,
	   ev->subscriptionid,
	   ev->senderid,
	   CLTID2IDX(ev->senderid),
	   SRVID2IDX(ev->senderid),
	   SVCID2IDX(ev->senderid),
	   GWID2IDX(ev->senderid),
	   ev->data, 
	   strdata(ev->data,  ev->datalen),
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
    DEBUG1(" EVENT(UN)SUBSCRIBE[REQ|RPL] MESSAGE: %#x\n"
	   " event                          = %.64s\n"
	   " MWID        subscriptionid     = %d\n"
	   " MWID        senderid           = %#x\n"
	   "   CLIENTID    cltid            = %d\n"
	   "   SERVERID    srvid            = %d\n"
	   "   SERVICEID   svcid            = %d\n"
	   "   GATEWAYID   gwid             = %d\n"
	   " int         data               = %d \"%s\"\n"
	   " int         datalen            = %d\n"
	   " int         flags              = %#x\n"
	   " int         returncode         = %d\n",
	   ev->mtype,  
	   ev->event,
	   ev->subscriptionid,
	   ev->senderid,
	   CLTID2IDX(ev->senderid),
	   SRVID2IDX(ev->senderid),
	   SVCID2IDX(ev->senderid),
	   GWID2IDX(ev->senderid),
	   ev->data, 
	   strdata(ev->data,  ev->datalen),
	   ev->datalen, 
	   ev->flags, ev->returncode);
    return;

  default:
    DEBUG1("Unknown message type %#X ", (long *) mesg);
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

/* return 0 on success or -errno */
int _mw_ipc_getmessage(char * data, int *len, int type, int flags)
{
  int rc; 

  if (len == NULL) return -EINVAL;
  *len -= sizeof(long);

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

  DEBUG1("_mw_ipc_getmessage a msgrcv(type=%d, flags=%d) returned %d and %d bytes of data", 
	type, flags, rc, *len);
  
  _mw_dumpmesg((void *)data);
  
  return 0;
};

/* return 0 on success or -errno */
int _mw_ipc_putmessage(int dest, char *data, int len,  int flags)
{
  int rc; 
  int qid;

  TIMEPEG();

  if (len > MWMSGMAX) {
    Error("_mw_ipc_putmessage: got a to long message %d > %d", len, MWMSGMAX);
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

  DEBUG1("_mw_ipc_putmessage: msgsnd(dest=%d, msglen=%d, flags=%#x) returned %d", 
	dest, len, flags, rc);
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
    return -errno;
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
int _mw_ipcsend_attach(int attachtype, char * name, int flags)
{
  
  Attach mesg;
  int rc, error, len;

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
  DEBUG1(	"Sending an attach message to mwd name = %s, client = %s server = %s size = %d",
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

/* we need this  for gateways where a gw has entries  in the gwtbl for
   foreign/peer  gateways, and  the gwid  shall  be the  gwid for  the
   foreign,  and not the  local gw.   We made  however a  more general
   function here so that in the  future we can conduct such madness as
   having a single process be more than one server.*/
int _mw_ipcsend_provide_for_id(MWID mwid, char * servicename, int cost, int flags)
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

int _mw_ipcsend_provide(char * servicename, int cost, int flags)
{
  SERVERID srvid;
  GATEWAYID gwid;
  
  srvid =  _mw_get_my_serverid();
  if (srvid != UNASSIGNED) 
    return _mw_ipcsend_provide_for_id(srvid, servicename, cost, flags);
 
  gwid = _mw_get_my_gatewayid();
  return _mw_ipcsend_provide_for_id(srvid, servicename, cost, flags);
};

SERVICEID _mw_ipc_provide(char * servicename, int flags)
{
  Provide providemesg;
  int rc, error, len;


  rc = _mw_ipcsend_provide(servicename, /* cost= */ 0, flags);
  if (rc < 0) return rc;

  /* Add timeout */
  len = sizeof(Provide);
  rc = _mw_ipc_getmessage((char *) &providemesg, &len, PROVIDERPL, 0);
  /* MUTEX END */
  if (rc == -1) {
    Error(	   "Failed to get a reply to a provide request error %d(%s)", 
	   errno, strerror(errno));
    return -error;
  };
  DEBUG1("Received a provide reply with service id %d and rcode %d",
	providemesg.srvid, providemesg.returncode);

  if (providemesg.svcid < 0) return providemesg.returncode;
  return providemesg.svcid;
};

int _mw_ipcsend_unprovide(char * servicename,  SERVICEID svcid)
{
  SERVERID srvid;
  GATEWAYID gwid;
  
  srvid =  _mw_get_my_serverid();
  if (srvid != UNASSIGNED) 
    return _mw_ipcsend_unprovide_for_id(srvid, servicename, svcid);
 
  gwid = _mw_get_my_gatewayid();
  return _mw_ipcsend_unprovide_for_id(srvid, servicename, svcid);
};

int _mw_ipcsend_unprovide_for_id(MWID mwid, char * servicename,  SERVICEID svcid)
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
  DEBUG1("mwunprovide() sent request for unprovide %s with serviceid=%d to mwd", 
	servicename, SVCID2IDX(svcid));

  return rc;
};

int _mw_ipc_unprovide(char * servicename,  SERVICEID svcid)
{
  Provide unprovidemesg;
  int rc, len;

  rc = _mw_ipcsend_unprovide(servicename, svcid);

  len = MWMSGMAX;
  rc = _mw_ipc_getmessage((char *) &unprovidemesg, &len, UNPROVIDERPL, 0);
  DEBUG1("mwunprovide() got reply for unprovide %s with rcode=%d serviceid = %d(rc=%d)", 
	servicename, unprovidemesg.returncode, svcid, rc);
  
  return unprovidemesg.returncode;
};

/* the IPC implementation of mwacall() */

/* the mwid  shall be set to  UNASSIGNED for IPC  clients.  mwgwd sets
   mwid  for teh srb  client or  gwid for  peer gateway.   the domain,
   callerid, and hops is only used  by mwgwd when one mwgwd is passing
   a call to  another gateway callerid is the id  on the peer gateways
   side. It's must be kept here  since we must passit back to the peer
   gateway. It's done this way  because the peer gateway don't need to
   remember the call, and hence reduses the time the peer gateway uses
   to process the reply (no lookups) */
int _mwacallipc (char * svcname, char * data, int datalen, int flags, 
		 MWID mwid, char * instance, char * domain, MWID callerid, int hops)
{
  int dest; 
  int rc;
  int hdl, n, idx;
  Call callmesg;
  struct timeval tm;
  float timeleft;
  SERVICEID * svclist;

  TIMEPEG();

  DEBUG1("BEGIN: (svcname=%s, data=%p, datalen=%d, flags=%#x, mwid=%#x, instance=%s, domain=%s, "
	 "callerid=%ld, hops=%d", svcname, data, datalen, flags, mwid, 
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
      DEBUG1("call to %s was made %d ms after deadline had expired", 
	    timeleft, svcname);
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
     since shmat on Linux do not always reurn tha same address for a given shm segment
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
  
  DEBUG1("dataoffset %d length %d",   callmesg.data, callmesg.datalen);

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
 
    DEBUG1("Sending a ipcmessage to serviceid %#x service %s on server %#x my clientid %#x buffer at offset%d len %d ", 
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
  int rc;
  char * buffer;
  struct timeval tv;
  Call * callmesg;
  MWID id;

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

  while ( callmesg == NULL) {
    /* get the next message of type SVCREPLY of teh IPC queue. */
    rc = _mw_ipc_getmessage(buffer, len , SVCREPLY, flags);
    if (rc != 0) {
      DEBUG1("returned with error code %d", rc);
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
  DEBUG1("Got a message of type %#x handle %d ", 
	callmesg->mtype, callmesg->handle);
  /* Retriving info from the message 
     If fastpath we return pointers to the shm area, 
     else we copy to private heap.
  */

  if (*data != NULL) free (*data);
  
  if (callmesg->data) {
     id = 0;
     _mwshmgetowner(callmesg->data, &id);
     DEBUG1("owner id of data is %d", id);
     
     if (! _mw_fastpath_enabled()) {
	
	DEBUG3("copying data to local byffer, freeing shm buffer");
	*data = malloc(callmesg->datalen+1);
	memcpy(*data, _mwoffset2adr(callmesg->data), callmesg->datalen);
	(*data)[callmesg->datalen] = '\0';
	_mwfree(_mwoffset2adr(callmesg->data));
     } else {
	*data = _mwoffset2adr(callmesg->data);
     }; 

     *len = callmesg->datalen;
  } else {
     *data = NULL;
     *len = 0;
  };

  *appreturncode = callmesg->appreturncode;

  /* deadline info is invalid even though I can provide it */

  DEBUG1("returned with returncode=%d and with %d bytes of data", 
	callmesg->returncode, callmesg->datalen);

  if (callmesg->returncode > MWMORE) 
    return -callmesg->returncode;
  return callmesg->returncode;
};

/************************************************************************/

/* EVENTS */
int _mw_ipc_subscribe(char * pattern, int subid, int flags)
{
  Event ev;
  int rc, len;

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

int _mw_ipc_unsubscribe(int subid)
{
  Event ev;
  int rc, len;

  rc = _mw_ipcsend_unsubscribe (subid);

  len = MWMSGMAX;
  rc = _mw_ipc_getmessage((char *) &ev, &len, EVENTUNSUBSCRIBERPL, 0);
  DEBUG1("got reply for unsubscribe %d with rcode=%d (rc=%d)", 
	 subid, ev.returncode,  rc);
  
  return ev.returncode;
};

int _mw_ipcsend_subscribe (char * pattern, int subid, int flags)
{
  MWID id;
  int  dest; 
  int rc;
  int pattlen;
  char * dbuf;
  Event ev;

  memset (&ev, '\0', sizeof(Event));
  errno = 0;

  dest = 0; // mwd

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
    
int _mw_ipcsend_unsubscribe (int subid)
{
  MWID id;
  int  dest; 
  int rc;
  Event ev;

  memset (&ev, '\0', sizeof(Event));
  errno = 0;

  dest = 0; // mwd

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
    
int _mw_ipcsend_event (char * event, char * data, int datalen, char * username, char * clientname)
{
  MWID id;
  int  dest; 
  int rc;
  int dataoffset;
  char * dbuf;
  Event ev;

  memset (&ev, '\0', sizeof(Event));
  errno = 0;

  dest = 0;
  
  
  /* now the data string are passed in shm, we only pass the address (offset) , and 
     the other neccessary data.
     since shmat on Linux do not always reurn tha same address for a given shm segment
     we must operate on offset into the segment (se shmalloc.c) */

  if (data != NULL) {
    dataoffset = _mwshmcheck(data);
    if (dataoffset == -1) {
      dbuf = _mwalloc(datalen);
      if (dbuf == NULL) {
	Error("Send Event: mwalloc(%d) failed reason %d", datalen, (int) errno);
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

  id = _mw_get_my_mwid();
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
  
  DEBUG1("Sending a ipcmessage to mwd event %s id %#x buffer at offset%d len %d ", 
	 ev.event, ev.senderid, ev.data, ev.datalen);

  rc = _mw_ipc_putmessage(dest, (char *) &ev, sizeof (Event), 0);
  
  DEBUG1("_mw_ipc_putmessage returned %d", rc);

  return rc;
};

int _mw_ipc_getevent(Event * ev)
{
  int len;
  int rc;
  if (ev == NULL) return -EINVAL;
  len = sizeof(Event);
  rc = _mw_ipc_getmessage((char *) ev, &len, EVENT, IPC_NOWAIT);
  if (rc < 0) return rc;
  if (len == sizeof(Event)) {
    return 0;
  };
  Error("Got an event ipc message, but with wrong length! %d != %d", rc, sizeof(Event));
  return -EMSGSIZE;
};
      
/************************************************************************/

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
