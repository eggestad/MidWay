/*
  MidWay
  Copyright (C) 2005 Terje Eggestad

  MidWay is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License as
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


#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <signal.h>

#include <MidWay.h>
#include <ipctables.h>
#include <ipcmessages.h>
#include <SRBprotocol.h>
#include <urlencode.h>

#include <internal-events.h>
#include <shmalloc.h>

#include "ipcserver.h"

#include "SRBprotocolServer.h"
#include "connections.h"
#include "srbevents.h"
#include "gateway.h"
#include "impexp.h"
#include "callstore.h"
#include "store.h"
#include "tcpserver.h"

/**
   @file

   Here we handle all IPC messages specific things. 

*/

DECLAREEXTERNMUTEX(bigmutex);

/************************************************************************
 * functions to handle the different IPC messages, and the ipcmainloop
 * that don't  do anything but just receive  IPC messages and dispatch
 * them.
 ************************************************************************/
static void do_event_message(char * message, int len)
{
  Event *          evmsg  = (Event *)          message;
  int newsvc, delsvc; 
  serviceentry * svcent;
  mwprovideevent * pe;
	  
  DEBUG("Got an event %s", evmsg->event);

  /* only events that don't start with a. is passed on too peers */
  if (evmsg->event[0] != '.') {
     do_srb_event_dispatch(evmsg);
     return; // only event beginning with . is processed below
  };

  /* special handle for .mw(un)provide messages */
  newsvc = strcmp(evmsg->event, NEWSERVICEEVENT);
  delsvc = strcmp(evmsg->event, DELSERVICEEVENT);
  
  if ( (newsvc == 0) || (delsvc == 0) ){
    
    if (evmsg->datalen != sizeof(mwprovideevent)) {
      Error("in datalength with provide event (%zu != %zu), probably an error in mwd(1)", 
	    evmsg->datalen,  sizeof(mwprovideevent));
      return;
    };
    
    pe = _mwoffset2adr(evmsg->data, _mw_getsegment_byid(evmsg->datasegmentid));
    if (pe == NULL) {
      Error("in getting data with provide event, probably an error in mwd(1)");
      return;
    };
	  
    DEBUG("Got an event %s: for service %s provider %#x svcid = %d", 
	  evmsg->event, pe->name, pe->provider, pe->svcid);
    
    svcent = _mw_get_service_byid(pe->svcid);
    if ( (svcent != NULL) && (svcent->location == GWPEER) ) {
      DEBUG("(un)provide event for foreign service in my own domain, ignoring");
      goto ack;
    };
    
    /* after this point we must send an ack. */
    if (pe->provider == _mw_get_my_gatewayid()) {
      DEBUG("(un)provide event from my self, ignoring");
      goto ack;
    } 
    
    if (newsvc == 0) {
      serviceentry * se;
      se = _mw_get_service_byid(pe->svcid);
    
      if (!se) {
	DEBUG("Got an provide event for a service that no longer exist, ignoring");
	goto ack;
      };

      if (se->location == GWLOCAL) {
	doprovideevent(pe->name);
      } else {
	DEBUG("we got a provide event for a foreign service, ignoring");
      }
    } else // delsvc == 0
      dounprovideevent(pe->name);
  } else {
     DEBUG("Ignoring event %s", evmsg->event);
  };
  ack:
  // we only ack messages with acompanying buffers
  if (evmsg->data) {
     evmsg->mtype = EVENTACK;
     DEBUG("acking event to mwd"); 
     _mw_ipc_putmessage(MWD_ID, message, len, 0);
  };
  return;
};

static void do_svcreply(Call * cmsg, int len);

static void do_svccall(Call * cmsg, int len)
{
  Connection * conn;

  TIMEPEGNOTE("enter do_svccall");
  if (cmsg == NULL) {
    Error("Internal Error: do_svccall called with NULL pointer, this can't happen");
    return;
  };

  DEBUG2("%s", conn_print());
  DEBUG("got a call to service %s svcid %#x", cmsg->service, cmsg->svcid);
  TIMEPEG();
  conn = impfindpeerconn(cmsg->service, cmsg->svcid);

  //  DEBUG2("%s", conn_print());

  TIMEPEG();

  if (conn == NULL) {
    Warning("Hmm got a svccall for service %s svcid %#x, but I've got no such import, returning error", 
	    cmsg->service, cmsg->svcid);
    cmsg->returncode = -ENOENT;
    if (cmsg->gwid != UNASSIGNED) {
      // if we ourselves managed to send this, we short cut the reply,
      // or else we get an infinite loop. We do actually send to
      // ourselves if we're the only one providing the service. It's
      // inefficient, but clean, and it's only _mwacallipc() theat do
      // the _mw_get_services_byname() call which is heavy.
      if (cmsg->gwid == _mw_get_my_gatewayid()) {
	do_svcreply(cmsg, len);
	return;
      };
      _mw_ipc_putmessage(cmsg->gwid, (char *) cmsg, len, 0);
    } else {
      _mw_ipc_putmessage(cmsg->cltid, (char *) cmsg, len, 0);
    };
    return;
  };


  DEBUG2("service is on connection %p", conn);
  //  DEBUG2("service is on connection %p", conn);
  //  DEBUG2("%s", conn_print());

  DEBUG("service is on connection with fd=%d at %s, gwid=%d", 
	conn->fd, conn->peeraddr_string, GWID2IDX(conn->gwid));

  if (conn->peerinfo) {
    Connection * pconn;
    pconn = ((struct gwpeerinfo *) conn->peerinfo)->conn;
    Assert(conn == pconn);
  }

  TIMEPEG();


  storeIPCCall(cmsg, conn);

  TIMEPEGNOTE("normal leave do_svccall");
  return;
  
};
  
static void do_svcreply(Call * cmsg, int len)
{
  int rc;
  char * data;
  SRBmessage srbmsg;
  Connection * conn;

  DEBUG("Got a svcreply service %s from server %d for client %d, gateway %d ipchandle=%#x callerid=%x instrance=%s", 
	cmsg->service, SRVID2IDX(cmsg->srvid),
	CLTID2IDX(cmsg->cltid), GWID2IDX(cmsg->gwid), cmsg->handle, cmsg->callerid, cmsg->instance);

  
  /* we must lock since it happens that the server replies before
     we do storeSetIpcCall() in SRBprotocol.c */
  strcpy(srbmsg.command, SRB_SVCCALL);
  srbmsg.marker = SRB_RESPONSEMARKER;
  
  rc = storeIPCReply(cmsg, &conn, &srbmsg.map);

  if (rc != 0) {
    DEBUG("Couldn't find a waiting call for this message, possible quite normal");
    return;
  };

  dbg_srbprintmap(&srbmsg);

  if (cmsg->flags & MWNOREPLY) {
    DEBUG( "Noreply flags set, ignoring");
    _mwfree(_mwoffset2adr(cmsg->data, _mw_getsegment_byid(cmsg->datasegmentid)));
    return;
  };
  
  if (cmsg->datalen == 0) {
    data = NULL;
    len = 0;
  } else {
     /// @todo large data return. 
    len = cmsg->datalen;
    data = _mwoffset2adr(cmsg->data, _mw_getsegment_byid(cmsg->datasegmentid));
  };
  
  DEBUG( "sending reply on conn=%s", conn->peeraddr_string);
  
  switch (cmsg->returncode) {
  case MWSUCCESS:
  case MWFAIL:
  case MWMORE:
    rc = cmsg->returncode;
    break;
    
  default:
    rc = _mw_errno2srbrc(cmsg->returncode);
  };
  
  _mw_srbsendcallreply(conn, &srbmsg, data, len, 
		       cmsg->appreturncode,  rc, 
		       cmsg->flags); 
  DEBUG( "reply sendt");
  
  /* if there are no more replies to this call, we called
     storePopCall() above, and we must free the map. */
  urlmapfree(srbmsg.map);
  
  _mwfree(_mwoffset2adr(cmsg->data, _mw_getsegment_byid(cmsg->datasegmentid)));
  /* may be either a client or gateway, if clientid is myself,
     then gateway. */
  return;
};

/**
   The main loop for the IPC thread (this is the primary thread). This
   thread shall only return when gw shutdown is complete.
*/
int ipcmainloop(void)
{
  int rc, errors;
  size_t len;
  char message[MWMSGMAX];
  int fd, connid;
  SRBmessage srbmsg;
  cliententry * cltent;
  Connection * conn;
  sigset_t set;

  /* really a union with the message buffer*/
  long *           mtype  = (long *)           message;
  
  Attach *         amsg   = (Attach *)         message;
  Administrative * admmsg = (Administrative *) message;
  Provide *        pmsg   = (Provide *)        message;
  Call *           cmsg   = (Call *)           message;

  memset (&srbmsg, '\0', sizeof(SRBmessage));
  gw_setmystatus(MWREADY);

  DEBUG("blocking ALARM signed in IPC thread");
  sigemptyset(&set);
  sigaddset(&set, SIGALRM);
  pthread_sigmask(SIG_BLOCK, &set, NULL);

  Info("Ready.");
  errors = 0;
  while(!globals.shutdownflag) {
    len = MWMSGMAX;

    TIMEPEGNOTE("end ipc");
    timepeg_log();
    DEBUG( "/\\ /\\ /\\ /\\ /\\ /\\ /\\ /\\ /\\ /\\ /\\ /\\ /\\ /\\ /\\ /\\ /\\ /\\ /\\ /\\ /\\ /\\ /\\ /\\ ");
    DEBUG("doing _mw_ipc_getmessage()");

    UNLOCK_BIGLOCK();

    rc = _mw_ipc_getmessage(message, &len, 0, 0);

    LOCK_BIGLOCK();


    DEBUG( "\\/ \\/ \\/ \\/ \\/ \\/ \\/ \\/ \\/ \\/ \\/ \\/ \\/ \\/ \\/ \\/ \\/ \\/ \\/ \\/ \\/ \\/ \\/ \\/ ");
    DEBUG( "_mw_ipc_getmessage() returned %d", rc);
    TIMEPEGNOTE("startipc");
    TIMEPEGNOTE("start");
    
    if (rc == -EIDRM) {
      globals.shutdownflag = TRUE;
      break;
    };
    if (rc == -EINTR) 
      continue;
    if (rc < 0) {
      errors++;
      if (errors > 10) {
	Error("To many errors while trying to read message queue");
	return -rc; /* going down on error */
      };
    }

    //    _mw_dumpmesg(message);

    switch (*mtype) {
    case ATTACHREQ:
    case DETACHREQ: 
      /*  it  is  consiveable  that  we may  add  the  possibility  to
         disconnect  spesific clients, but  that may  just as  well be
         done  thru  a  ADMREQ.  We  have so  fare  no  provition  for
         disconnecting clients other than mwd marking a client dead in
         shm tables. */
      Warning("Got a attach/detach req from pid=%d, ignoring", amsg->pid);
      break;

    case ATTACHRPL:
      /*  now this  is in  response to  an attach.  mwd must  reply in
	 sequence, so this is the  reply to my oldest pending request.
	 (not that it matters)*/
      strcpy(srbmsg.command, SRB_INIT);
      srbmsg.marker = SRB_RESPONSEMARKER;
      rc = storePopAttach(amsg->cltname, &connid, &fd, &srbmsg.map);
      cltent = _mw_get_client_byid(amsg->cltid);

      if (rc != 1) {
	Warning("Got a Attach reply that I was not expecting! clientname=%s gwid=%#x srvid=%#x", amsg->cltname, amsg->gwid, amsg->srvid); 

	/* we don't bother mwd with detaches, mwd will reuse if
           cltent->status == UNASSIGNED */
	if (cltent != NULL) {
	  cltent->location = UNASSIGNED;
	  cltent->type = UNASSIGNED;
	  cltent->pid = UNASSIGNED;
	  cltent->mqid = UNASSIGNED;
	  cltent->clientname[0] = '\0';
	  cltent->username[0] = '\0';
	  /* hand over to mwd */
	  cltent->status = UNASSIGNED;
	};
	amsg->cltid |= MWCLIENTMASK;
	conn_setinfo(fd, &amsg->cltid, NULL, NULL, NULL);
	/* nothing more to do */
	break;
      };

      DEBUG( "connection for clientname %s on fd=%d is assigned the clientid %#x", 
	    amsg->cltname, fd, amsg->cltid);
      /* now assign the CLIENTID to the right connection. */
      conn_setinfo(fd, &amsg->cltid, NULL, NULL, NULL);
      conn = conn_getentry(fd);

      /* copy peer socket addr into the ICP table */ 
      conn_setpeername(conn);

      _mw_srbsendinitreply(conn, &srbmsg, SRB_PROTO_OK, NULL);
      urlmapfree(srbmsg.map);
      
      break;
      
    case DETACHRPL:
      /* do I really care? */
      break;

    case SVCCALL:
    case SVCFORWARD:
      do_svccall(cmsg, len);
      /* gateway only, we have imported a service, find the remote gw
         and send the call */
      break;

    case SVCREPLY:
      do_svcreply(cmsg, len);
      break;

    case ADMREQ:
      /* the only adm request we accept right now is shutdown, we may
         later accept reconfig, and connect/disconnect gateways. */
      if (admmsg->opcode == ADMSHUTDOWN) {
	Info("Received a shutdown command from clientid=%d", 
	      CLTID2IDX(admmsg->cltid));
	return 2; /* going down on command */
      }; 
      break;

    case PROVIDEREQ:
      Warning("Got a providereq from serverid=%d, sending rc=EBADRQC", 
	    pmsg->srvid&MWINDEXMASK);
      pmsg->returncode = -EBADRQC;
      _mw_ipc_putmessage(pmsg->srvid, message, len, IPC_NOWAIT);
      break;

    case PROVIDERPL:
      impsetsvcid(pmsg->svcname, pmsg->svcid, pmsg->gwid);
      break;

    case UNPROVIDEREQ:
      Warning("Got a providereq from serverid=%d, sending rc=EBADRQC", 
	    pmsg->srvid&MWINDEXMASK);
      pmsg->returncode = -EBADRQC;
      _mw_ipc_putmessage(pmsg->srvid, message, len, IPC_NOWAIT);
      break;

     case UNPROVIDERPL:
      /* do I really care? */
      break;

       /* events */
    case EVENT:
      do_event_message(message, len);
      break;

    default:
      Warning("got a unknown message with message type = %#lx", *mtype);
    } /* end switch */
  } /* end while */
  UNLOCK_BIGLOCK();
  return 1; /* going down on signal */
  
};



struct ipcfifo {
   MWID mwid;
   void * message;
   int msglen;
   struct ipcfifo * next, * prev;
};
static struct ipcfifo * head = NULL, * tail = NULL;
static int ipcfifolen = 0;

static inline struct ipcfifo * del_fifo_entry(struct ipcfifo * qe)
{
   free(qe->message);
   if (qe->next) qe->next->prev  = qe->prev;
   if (qe->prev) qe->prev->next  = qe->next;
   if (qe == head) head = qe->prev;
   if (qe == tail) tail = qe->next;
   free(qe);
   ipcfifolen--;
   return qe->next;
};

/** Try pushing out IPC messgaess that where queued on full queue. The
    fifo is the messages that should have been put on recipient queue,
    but the recipient queue were full.

    @return the number if pending messages in the quere. 
 */

int flush_ipc_fifo(void)
{
   struct ipcfifo * qe;
   Call * cmsg; //! @todo Call shoud not be need here, there are other messages with more data 
   seginfo_t * dataseg;
   int rc;

   if (ipcfifolen == 0) return 0;
   
   DEBUG("flushing ipc fifo which has %d entries",ipcfifolen); 

   qe = tail;
   while (qe != NULL) {
      DEBUG("entry has dest %x", qe->mwid);
      rc = _mw_ipc_putmessage(qe->mwid, qe->message, qe->msglen, IPC_NOWAIT);
      switch(rc) {
      case 0: // message sent OK
	 DEBUG("message sent, removing entry");
	 qe = del_fifo_entry(qe);
	 break;
	 
      case -EAGAIN:
      case -ENOMEM: // should never happen, but is a retry case
	 qe = qe->next;
	 break;
	 
      default:
	 // all other errors are drop message;
	 Error("failed to send message to %#x errno = %d", qe->mwid, rc);
	 cmsg  = (Call *) qe->message;
	 dataseg = _mw_getsegment_byid(cmsg->datasegmentid);
	 _mwfree(_mwoffset2adr(cmsg->data, dataseg));
	 if (cmsg->datasegmentid > LOW_LARGE_BUFFER_NUMBER) _mw_detach_mmap(dataseg);
	 qe = del_fifo_entry(qe);
	 break;
      }
   }
   return ipcfifolen;
};

/** This is a wrapper to _mw_ipc_putmessage() that handles full
   queues. If a queue is full, we do a single 100us-1ms sleep to allow
   the scheduler the possibility to schedule the receipient process to
   process it's message queue, then we put it into a fifo queue of
   pending messages and schedule a tasklet to retry in the future. The
   tasklet must be active as long there are messages in the queue. We
   use a FIFO ensure that messages to a single recepient are delivered
   in order. For this reason we must always try to flush the FIFO
   every time called. We make a seperate function to flush the fifo,
   
 */

void ipc_sendmessage(MWID id, void * message, int mesgsize)
{
   int rc, retry_count = 0;
   Call * cmsg;
   struct ipcfifo * qe;
   cmsg = (Call *) message;
   seginfo_t * dataseg;

   flush_ipc_fifo();

   dataseg = _mw_getsegment_byid(cmsg->datasegmentid);
   
 retry:
   DEBUG("Sending message retry = %d", retry_count);
   rc = _mw_ipc_putmessage(id, message, mesgsize, IPC_NOWAIT);
   
   if (rc == 0) {
      DEBUG("Message sent OK");
      if (cmsg->datasegmentid > LOW_LARGE_BUFFER_NUMBER) 
	 _mw_detach_mmap(dataseg);
      return;
   };
   
   if (rc == -EAGAIN) {
      if (retry_count < 2) {
	 retry_count++;
	 usleep(1000); // 1 mS sleep
	 goto retry;
      };
      
      qe = malloc(sizeof(struct ipcfifo));
      qe->mwid = id;
      qe->message = message;
      qe->msglen = mesgsize;

      DEBUG("message send failed due to queue full, enqueing"); 
      qe->next = NULL;   
      
      if (head == NULL) {
	 qe->prev = NULL;
	 head = tail = qe;
      };
      ipcfifolen++;
      head->next = qe;
      qe->prev = head;
      head = qe;
      wake_fast_task();
      return;
   };
   Error("failed to send message to %#x errno = %d", id, rc);
   _mwfree(_mwoffset2adr(cmsg->data, dataseg));
   if (cmsg->datasegmentid > LOW_LARGE_BUFFER_NUMBER) _mw_detach_mmap(dataseg);
   return;
 };
