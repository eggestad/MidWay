/*
  MidWay
  Copyright (C) 2000,2001,2002 Terje Eggestad

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

/*
 * $Log$
 * Revision 1.26  2004/11/17 20:58:08  eggestad
 * Large data buffers for IPC
 *
 * Revision 1.25  2004/06/06 16:09:42  eggestad
 * fix for segfault when domain are is empty
 *
 * Revision 1.24  2004/04/12 23:05:25  eggestad
 * debug format fixes (wrong format string and missing args)
 *
 * Revision 1.23  2004/04/08 10:34:06  eggestad
 * introduced a struct with pointers to the functions implementing the midway functions
 * for a given protocol.
 * This is in preparation for be able to do configure with/without spesific protocol.
 * This creates a new internal API each protocol must addhere to.
 *
 * Revision 1.22  2004/03/20 18:57:47  eggestad
 * - Added events for SRB clients and proppagation via the gateways
 * - added a mwevent client for sending and subscribing/watching events
 * - fix some residial bugs for new mwfetch() api
 *
 * Revision 1.21  2003/06/12 07:35:50  eggestad
 * fix for NULL data when forward to peer
 *
 * Revision 1.20  2003/06/05 21:59:21  eggestad
 * sigsegv bug fix
 *
 * Revision 1.19  2003/06/05 21:52:56  eggestad
 * commonized handling of -l option
 *
 * Revision 1.18  2003/03/16 23:50:24  eggestad
 * Major fixups
 *
 * Revision 1.17  2003/01/07 08:27:56  eggestad
 * * Major fixes to get three mwgwd working correctly with one service
 * * and other general fixed for suff found on the way
 *
 * Revision 1.16  2002/11/19 12:43:55  eggestad
 * added attribute printf to mwlog, and fixed all wrong args to mwlog and *printf
 *
 * Revision 1.15  2002/11/18 00:21:21  eggestad
 * - gw_provideservices_to_peer() called once to many
 * - added clean up of all imp/exp on peer disconnect
 * - remove some junk since mwlog() now handle default log file name correctly
 *
 * Revision 1.14  2002/10/22 21:58:21  eggestad
 * Performace fix, the connection peer address, is now set when establised, we did a getnamebyaddr() which does a DNS lookup several times when processing a single message in the gateway (Can't believe I actually did that...)
 *
 * Revision 1.13  2002/10/20 18:21:53  eggestad
 * - added handling of outbound srvcall, and tok out outbound replies in a separate function (where it belongs)
 * - Fatal handling fixup
 * - added good logging markers on beginning and end of processiong an ipcmessage
 *
 * Revision 1.12  2002/10/06 23:58:35  eggestad
 * _mw_get_services_byname() has a new prototype
 *
 * Revision 1.11  2002/10/03 21:19:35  eggestad
 * - fix to prevent export of imported services (we still need to handle foreign domains later)
 * - impsetsvcid() needed GWID.
 * - get cost of service fixup
 * - segfault in a DEBUG fixed.
 * - improved handling of logfile prefix, added -L to give it spesifically (still not complete, see mwlog.c)
 *
 * Revision 1.10  2002/09/29 17:44:01  eggestad
 * added unproviding over srb
 *
 * Revision 1.9  2002/09/26 22:38:21  eggestad
 * we no longer listen on the standalone  port (11000) for clients as default. Default is now teh broker.
 *
 * Revision 1.8  2002/09/22 22:53:30  eggestad
 * - added IPC event message handler
 * - fix of mutex locking error
 *
 * Revision 1.7  2002/08/09 20:50:16  eggestad
 * A Major update for implemetation of events and Task API
 *
 * Revision 1.6  2002/07/07 22:45:48  eggestad
 * *** empty log message ***
 *
 * Revision 1.5  2001/10/04 19:18:10  eggestad
 * CVS tags fixes
 *
 * Revision 1.4  2001/10/03 22:37:38  eggestad
 * many bugfixes: memleak, gwid, propper shutdown
 *
 * Revision 1.3  2001/09/15 23:49:38  eggestad
 * Updates for the broker daemon
 * better modulatization of the code
 *
 * Revision 1.2  2000/08/31 22:06:54  eggestad
 * - srbsend*() funcs now has a _mw_ prefix
 * - all direct use of urlmap, and sprintf() in making messages, we now use SRBmsg structs
 *
 * Revision 1.1  2000/07/20 18:49:59  eggestad
 * The SRB daemon
 *
 */

#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <limits.h>
#include <signal.h>

#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>

#ifdef DEBUGGING
#include <mcheck.h>
#endif

#include <MidWay.h>
#include <SRBprotocol.h>
#include <ipctables.h>
#include <ipcmessages.h>
#include <urlencode.h>
#include <shmalloc.h>
#include <version.h>
#include <address.h>
#include <internal-events.h>
#include <multicast.h>

#define _GATEWAY_C
#include "SRBprotocolServer.h"
#include "gateway.h"
#include "tcpserver.h"
#include "store.h"
#include "remdomains.h"
#include "connections.h"
#include "toolbox.h"
#include "impexp.h"
#include "srbevents.h"

static char * RCSId UNUSED = "$Id$";
static char * RCSName UNUSED = "$Name$";

/***********************************************************************
 This  modules  differs  a  bit   from  every  one  else  in  that  is
 threaded. The reason  for threading is that one thread  need to be in
 blocking wait in the select call in tcpserver.c:tcpservermainloop and
 the other here  in ipcmainloop(). Since we need to  report our pid to
 mwd, and we want to attach  to mwd as a local gateway before starting
 to  listen, the  inside doing  IPC  is not  the new  thread, but  the
 "program".

 In this rather preliminary version  we only allow one listen port per
 mwgwd, but  there shall  not be  any limitations on  how many  port a
 mwgwd can listen on.

 what needs to be determined if we need a gateway table for each port,
 or just for  each domainname, or if  there shall be a 1  to 1 between
 the two. So far we only use 1.
 ***********************************************************************/

ipcmaininfo * ipcmain = NULL;
gatewayentry * gwtbl = NULL;

globaldata globals = { 0 };


void usage(char * arg0)
{
  printf ("%s: [-A uri] [-l level] [-L logprefix] [-c clientport] [-g gatewayport] [-p commonport] [domainname]\n",
	  arg0);
};

/* 
   OK, here we provide a lock for dealing with peers. Both threads may
   mess with import export lists,  as well as disconnecting peers. The
   most important part is that while the tcp thread are in progress of
   sending provides to  a just connected peer, the mwd  may send us an
   new service  changed event, which  may result in  provide unprovide
   SRB  messages from the  ipc thread.   In this  case the  tcp thread
   shall complete, and when teh ipc thread get the lock, checks to see
   if any peers need an update. */

DECLAREMUTEX(peersmutex);

DECLAREGLOBALMUTEX(bigmutex);

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
      Error("in datalength with provide event (%lld != %ld), probably an error in mwd(1)", 
	    evmsg->datalen, (long) sizeof(mwprovideevent));
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
  SRBmessage srbmsg;
  Connection * conn;
  unsigned char marker;
  void * data = NULL;
  int rc, l;

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

  // TODO: handle arbitrary long data
  if (cmsg->datalen != 0) {
    data = _mwoffset2adr(cmsg->data, _mw_getsegment_byid(cmsg->datasegmentid));
    if (!data) {
      Error("got a corrupt svccall message, dataoffset = %lld ", 
	    cmsg->data);
      return;
    };

    TIMEPEG();

    l = _mwshmcheck(data);
    if ( (l < 0) || (l < cmsg->datalen)) {
      Error("got a corrupt svccall message, dataoffset = %lld len = %lld shmcheck() => %d", 
	    cmsg->data, cmsg->datalen, l);
      return;
    };
  } 

  if (cmsg->flags & MWNOREPLY) 
    marker = SRB_NOTIFICATIONMARKER;
  else 
    marker = SRB_REQUESTMARKER;

  TIMEPEG();

  _mw_srb_init(&srbmsg, SRB_SVCCALL, marker, 
	       SRB_SVCNAME, cmsg->service, 
	       SRB_INSTANCE, globals.myinstance, 
	       SRB_DOMAIN, globals.mydomain, 
	       NULL, NULL);
  TIMEPEG();

  if (data != NULL) {
    _mw_srb_nsetfield(&srbmsg, SRB_DATA, data, cmsg->datalen);
    _mwfree(data);
  };

  _mw_srb_setfieldi(&srbmsg, SRB_HOPS, cmsg->hops);
		 
  _mw_srb_setfieldi(&srbmsg, SRB_CALLERID, cmsg->cltid);

  _mw_srb_setfieldx(&srbmsg, SRB_HANDLE, cmsg->handle);

  TIMEPEG();
  rc = _mw_srbsendmessage(conn, &srbmsg);

  TIMEPEG();
  
  urlmapfree(srbmsg.map);
  TIMEPEGNOTE("normal leave do_svccall");
  return;
  
};
  
static void do_svcreply(Call * cmsg, int len)
{
  int rc;
  char * data;
  SRBmessage srbmsg;
  int fd;
  Connection * conn;
  MWID mwid;

  DEBUG("Got a svcreply service %s from server %d for client %d, gateway %d ipchandle=%#x callerid=%x instrance=%s", 
	cmsg->service, SRVID2IDX(cmsg->srvid),
	CLTID2IDX(cmsg->cltid), GWID2IDX(cmsg->gwid), cmsg->handle, cmsg->callerid, cmsg->instance);

  
  /* we must lock since it happens that the server replies before
     we do storeSetIpcCall() in SRBprotocol.c */
  strcpy(srbmsg.command, SRB_SVCCALL);
  srbmsg.marker = SRB_RESPONSEMARKER;
  
  storeLockCall();
  /* there are more replies, we get and not pop the map from the
     pending call list */

  if (cmsg->cltid != UNASSIGNED) 
    mwid = cmsg->cltid;
  else 
    mwid = cmsg->gwid;

  DEBUG("fetching SRB map from store for %#x handle = %x", mwid, cmsg->handle);
  if (cmsg->returncode == MWMORE) {
    rc = storeGetCall(mwid, cmsg->handle, &fd, &srbmsg.map);
    DEBUG2("storeGetCall returned %d, address of old map is %p", 
	   rc, srbmsg.map);
    srbmsg.map = urlmapdup(srbmsg.map);
    DEBUG2("dup map: new map at %p", srbmsg.map);
  } else {
    rc = storePopCall(mwid, cmsg->handle, &fd, &srbmsg.map);
    DEBUG2("storePopCall returned %d, address of old map is %p", 
	   rc, srbmsg.map);
  };
  storeUnLockCall();
  
  if (!rc ) {
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
    len = cmsg->datalen;
    data = _mwoffset2adr(cmsg->data, _mw_getsegment_byid(cmsg->datasegmentid));
  };
  
  DEBUG( "sending reply on fd=%d", fd);
  conn = conn_getentry(fd);
  
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

int biglock = 1;
int ipcmainloop(void)
{
  int i, rc, errors, len;
  char message[MWMSGMAX];
  int fd, connid;
  SRBmessage srbmsg;
  cliententry * cltent;
  Connection * conn;

  /* really a union with the message buffer*/
  long *           mtype  = (long *)           message;
  
  Attach *         amsg   = (Attach *)         message;
  Administrative * admmsg = (Administrative *) message;
  Provide *        pmsg   = (Provide *)        message;
  Call *           cmsg   = (Call *)           message;

  memset (&srbmsg, '\0', sizeof(SRBmessage));
  i = GWID2IDX(_mw_get_my_gatewayid());
  gwtbl[i].status = MWREADY;
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

/************************************************************************
 *  
 ************************************************************************/
int gwattachclient(Connection * conn, char * cname, char * username, char * password, urlmap * map)
{
  Attach mesg;
  int rc;

  /*  first  of all  we  should  test  authentication..... that  means
     checking  config  for  authentication  level  required  for  this
     gateway  or client  name, whatever,  we need  to code  the config
     first. */
  
  storePushAttach(cname, conn->connectionid, conn->fd, map);

  DEBUG("gwattachclient(fd=%d, cname=\"%s\", connid=%d)", conn->fd, cname, conn->connectionid);

  mesg.mtype = ATTACHREQ;
  mesg.gwid = _mw_get_my_gatewayid(); 
  mesg.client = TRUE;    
  mesg.server = FALSE;
  strncpy(mesg.cltname, cname, MWMAXNAMELEN);
  mesg.pid = globals.ipcserverpid;
  mesg.flags = 0;
  mesg.ipcqid = _mw_my_mqid();

  rc = _mw_ipc_putmessage(MWD_ID, (void *) &mesg, sizeof(mesg) ,0);

  if (rc != 0) {
    Error("gwattachclient()=>%d failed with error %d(%s)", 
	   rc, errno, strerror(errno));
    return -errno;
  };
  return 0;
};


int gwdetachclient(int cltid)
{
  Attach mesg;
  int rc;
  char buff[128];

  /*  first  of all  we  should  test  authentication..... that  means
     checking  config  for  authentication  level  required  for  this
     gateway  or client  name, whatever,  we need  to code  the config
     first. */
    
  conn_getclientpeername(cltid, buff, 128);
  DEBUG("gwdetachclient(clientid=%d) (%s)", CLTID2IDX(cltid), buff);

  memset (&mesg, '\0', sizeof(Attach));

  mesg.mtype = DETACHREQ;
  mesg.cltid = cltid | MWCLIENTMASK;
  mesg.gwid = _mw_get_my_gatewayid(); 
  mesg.client = TRUE;    
  mesg.server = FALSE;
  mesg.pid = globals.ipcserverpid;
  mesg.flags = MWFORCE;
  mesg.ipcqid = _mw_my_mqid();

  rc = _mw_ipc_putmessage(MWD_ID, (void *) &mesg, sizeof(mesg) ,0);

  if (rc != 0) {
    Error("gwdetachclient() putmessage =>%d failed with error %d(%s)", 
	   rc, errno, strerror(errno));
    return -errno;
  };
  return 0;
};

Connection * gwlocalclient(CLIENTID cid)
{
  cliententry  * ce;

  /* these two tests are fast since we don't do any sequential
     searches of the kind conn_getclient() does. So we do this to save
     time if the client is not imported by myself. calling
     conn_clients is actually enough. */ 

  ce = _mw_get_client_byid(cid);
  if (ce == NULL) return NULL;

  if (ce->gwid == UNASSIGNED) {
    DEBUG2(" client %d is IPC", CLTID2IDX(cid));
    return NULL; // IPC client
  };

  if (ce->gwid !=  _mw_get_my_gatewayid()) {
    DEBUG2(" client %d is handeled by GWID %d, which is not us", CLTID2IDX(cid), GWID2IDX(ce->gwid));
    return NULL; 
  };

  return conn_getclient(cid);
};

/************************************************************************/

#define LOCK -1
#define UNLOCK 1

static int lockgwtbl(int lock_unlock)
{
  struct sembuf sop[1];

  if (ipcmain == NULL) {
    errno = ENOENT;
    return -1;
  };
  sop[0].sem_num = 0;
  sop[0].sem_op =  lock_unlock;
  sop[0].sem_flg = 0;

  return semop(ipcmain->gwtbl_lock_sem, sop, 1);
};

GATEWAYID allocgwid(int location, int role)
{
  int rc, i, idx;
  errno = 0;
  if ( (rc = lockgwtbl(LOCK)) < 0) {
    Error("Failed to lock the gateway table, reason: %s. Exiting...", 
	  strerror(errno));
    return -1;
  };
  DEBUG( "start allocgwid(location=%d, role=%d) nextidx=%d max=%d", 
	location, role, ipcmain->gwtbl_nextidx, ipcmain->gwtbl_length);

  /* we loop thru the table starting at the last assigned gwid + 1. */
  for (i = 0; i < ipcmain->gwtbl_length; i++) {
    idx = (ipcmain->gwtbl_nextidx+i) % ipcmain->gwtbl_length;

    if (gwtbl[idx].pid == UNASSIGNED) {
      gwtbl[idx].srbrole = role;
      gwtbl[idx].mqid = _mw_my_mqid();
      gwtbl[idx].pid = globals.ipcserverpid;
      gwtbl[idx].status = MWBOOTING;
      gwtbl[idx].location = location;
      gwtbl[idx].connected = time(NULL);
      gwtbl[idx].addr_string[0] = '\0';

      DEBUG( "allocgwid() returns %d (>)", idx);
      ipcmain->gwtbl_nextidx = idx+1;

      lockgwtbl(UNLOCK);
      return idx|MWGATEWAYMASK;
    }
  }; 

  rc = lockgwtbl(UNLOCK);
  Error("Gateway table is full!");
  return UNASSIGNED;
};

void gw_setipc(struct gwpeerinfo * pi)
{
  gatewayentry * gwent;
  
  if (pi == NULL) return ;
  gwent = _mw_get_gateway_byid(pi->gwid);

  if (gwent == NULL) {
     Warning(" attempted to to get gw entry for gwid=%d, but no entry found", GWID2IDX(pi->gwid));
     return;
  };

  if (pi->conn == NULL) {
    Fatal("Called with NULL argument");
    abort(); 
  };

  if (pi->domainid == 0) {
    gwent->location = GWPEER;
    strncpy(gwent->domainname, globals.mydomain, MWMAXNAMELEN);
  }  else {
    DEBUG("Hmm remote domain with domainid == %d", pi->domainid);
    gwent->location = GWREMOTE;
    strcpy(gwent->domainname, "FOREIGN"); 
    //* TODO: remote domainname     
  }

  gwent->status = MWREADY;
  strncpy(gwent->instancename, pi->instance, MWMAXNAMELEN);
  conn_setpeername(pi->conn);
  return;
};
  
void freegwid(GATEWAYID gwid)
{

  if (gwid == UNASSIGNED) return;

  gwid &= MWINDEXMASK;

  if ( (gwid < 0) || (gwid > ipcmain->gwtbl_length)) return;

  lockgwtbl(LOCK);
  gwtbl[gwid].srbrole = 0;
  gwtbl[gwid].mqid = UNASSIGNED;
  gwtbl[gwid].status = MWDEAD;
  gwtbl[gwid].location = UNASSIGNED;
  gwtbl[gwid].pid = UNASSIGNED;
  gwtbl[gwid].addr_string[0] = '\0';
  lockgwtbl(UNLOCK);
};


/************************************************************************
 * Functions dealing with MW instances in my own domain
 ************************************************************************/

/* Known  peers are never  forgotten (well, we  need some clean  up at
   some time, but that's not  critical. We keep track of peer gataways
   in both our own and foreign/remote domains. */

static char ** peerhostnames = NULL;

static struct gwpeerinfo ** knownGWs = NULL;
static int GWcount = 0;
static int GWalloccount = 0;
static int dirsize = 16;

#define DIRENTRY(idx) (idx/dirsize)
#define GWENTRY(idx) (knownGWs[(idx/dirsize)][idx%dirsize])

static struct gwpeerinfo * addKnownPeer(struct gwpeerinfo * peer)
{
   int idx = -1, i;
   struct gwpeerinfo * gwpi;

   DEBUG2( "Adding %s as a know peer at address %s", 
	  peer->instance, peer->hostname);
   
   LOCKMUTEX(peersmutex);
   
   // first we check to see if we know about it from before
   for (i = 0; i < GWcount; i++) {    
      if (strcmp(peer->instance, GWENTRY(i).instance) == 0) {
	 DEBUG2( "Known peer on index %d", i);
	 idx = i;
	 goto out;
      };
   };    

   idx = GWcount;
   GWcount++;
   DEBUG2( "adding Known peer on index %d", idx);

   if (GWalloccount <= GWcount) {
      GWalloccount += dirsize;
      DEBUG2("extending the knownGW dir table from %d to %d, index = %d",
	    GWcount - 1 , GWalloccount, idx);
      
      knownGWs = realloc(knownGWs, GWalloccount * sizeof(struct gwpeerinfo *) / dirsize);

      knownGWs[DIRENTRY(GWcount)] = malloc(sizeof(struct gwpeerinfo) * dirsize);
   } 
   GWENTRY(idx) = * peer;
 
 out:

   DEBUG2( "added Known peer on index %d", idx);
   
   gwpi = & GWENTRY(idx);
   
   UNLOCKMUTEX(peersmutex);

   DEBUG2( "returning  %p", gwpi);
   return gwpi;
};

static struct gwpeerinfo * getKnownPeerByInstance(char * instance)
{
   int idx = -1, i;

   LOCKMUTEX(peersmutex);
   
   // first we check to see if we know about it from before
   for (i = 0; i < GWcount; i++) {    
      if (strcmp(instance, GWENTRY(i).instance) == 0) {
	 DEBUG2( "Known peer on index %d", i);
	 idx = i;
	 break;
      };
   };    

   UNLOCKMUTEX(peersmutex);

   if (idx == -1) {
      return NULL;
   }
   return & GWENTRY(idx);
};

static struct gwpeerinfo * getKnownPeerByGWID(GATEWAYID gwid)
{
   int idx = -1, i;
   struct gwpeerinfo * pi = NULL;

   LOCKMUTEX(peersmutex);
   
   // first we check to see if we know about it from before
   for (i = 0; i < GWcount; i++) {  
      pi = & GWENTRY(i);
      DEBUG2(" testing knownGWs[%d].gwid=%#x  ?= gwid=%#x", i, pi->gwid, gwid);
      if (gwid ==  pi->gwid) {
	 DEBUG2( "Known peer on index %d", i);
	 idx = i;
	 break;
      }
   };    

   UNLOCKMUTEX(peersmutex);

   if (idx == -1) {
      return NULL;
   }
   return pi;
};

/* provided for parsing the config, it's possible there to add a known
   peer host.  We'll ask the peer  host for ALL instances on that host
   later this should  be legal to do on the fly  by admins (thru mwd?,
   event?)  And  we may eventually allow  other GW to  advise us about
   peer costs .  (maybe, they should advise us on instances) */

int gw_addknownpeerhost(char * hostname) 
{
  int i = 0;

  if (peerhostnames != NULL) {
    for (i = 0; peerhostnames[i] != NULL; i++) {
      if (strcmp(hostname, peerhostnames[i]) == 0) {
	DEBUG( "Adding known peer host # %d \"%s\"", i, hostname);
	return 0;
      };
    };
    peerhostnames = realloc(peerhostnames, sizeof(char *) * i+2);
  } else {
    peerhostnames = malloc(sizeof(char *) * i+2);
  };

  if (peerhostnames == NULL) {
    Error("gw_addknownpeerhost failed to alloc memory reason %d", errno);
    return -1;
  };

  DEBUG( "Adding known peer host # %d \"%s\"", i, hostname);
  peerhostnames[i] = strdup(hostname);
  peerhostnames[i+1] = NULL;
  return 0;
};

/* we're called either on a multicast reply or if an unknown gateway
   connect us we must have the instance and domain name, and the
   sockaddr struct either on the connection of the sender of teh UDP
   multicast reply packet.  If piptr is non null it is set to point to
   the alloced knownGWs entry for this peer, or NULL on failure. */

int gw_addknownpeer(char * instance, char * domain, 
		    struct sockaddr * peeraddr, struct gwpeerinfo ** piptr) 
{
  int l;
  const void * sz;
  struct sockaddr_in * sain;
  struct gwpeerinfo peer;

  // if case of failure below
  if (piptr) *piptr = NULL;

  if ((instance == NULL) && (strlen(instance) > MWMAXNAMELEN)) {
    return -EINVAL;
  };

  peer.password = NULL;

  DEBUG( "(instance=%s, domain=%s)", instance, domain);

  switch (peeraddr->sa_family) {

  case AF_INET:    
    sain = (struct sockaddr_in * ) peeraddr;
    l = MAXADDRLEN;
    sz = inet_ntop(AF_INET, &sain->sin_addr, peer.hostname, l); 
    if (sz <= 0) {
      Error("got an address to peer that is not IPv4, ignoring family %d", 
	     peeraddr->sa_family);    
      return -EAFNOSUPPORT;
    }; 
    break;
    
  default:
    Error("got an address to peer that is not IPv4, ignoring family %d", 
	   peeraddr->sa_family);    
    return -EAFNOSUPPORT;
  };
 
  DEBUG("testing to see if theis is a foreign domain mydomain=%s peer has %s", domain, globals.mydomain);
  if (strcmp(domain, globals.mydomain) == 0)
    peer.domainid = 0;
  else 
    peer.domainid = remdom_getid(domain);

  if (peer.domainid < 0) return -EPERM;

  
  strncpy(peer.instance, instance, MWMAXNAMELEN);
  peer.conn = NULL;
  peer.gwid = UNASSIGNED;

  DEBUG( "Adding %s as a know peer in domain %s (domid %d) at address %s", 
	 peer.instance, domain, peer.domainid, peer.hostname);

  * piptr  = addKnownPeer(&peer);

  return 0;
};

  
/* called when we get and SRB INIT request from a gateway */
int gw_peerconnected(char * instance, char * peerdomain, Connection * conn)
{
   int domid = -1;
  struct gwpeerinfo * pi = NULL;
  GATEWAYID gwid;

  if ( (instance == NULL) || (conn == NULL) ){
    Error( "Internal error: " 
	   " %s Was called with invalid params instance=%p conn=%p",  __FUNCTION__, instance, conn);
    return -1;
  };

  if (peerdomain == NULL) {
    DEBUG( "peer %s connected us", instance);
    domid = 0; // my domain
  } else {
    domid = remdom_getid(peerdomain);
    if (domid < 0) {
      Info("Rejected connection from %s unknown domain %s", 
	    instance, peerdomain);
      return -1;
    };
    DEBUG( "gateway %s connected us from domain %s", instance, peerdomain);
  };

  pi = getKnownPeerByInstance(instance);

  if (pi == NULL) {
    struct sockaddr sa;
    int l;
    DEBUG( "got an connection from an unknown peer, adding and retrying");
    l = sizeof(struct sockaddr_in);
    getpeername(conn->fd, &sa, &l);
    gw_addknownpeer(instance, peerdomain, &sa, &pi);    
  };

  if (pi == NULL) {
    Info("rejected  peer %s on domain %s", instance, peerdomain);      
    return -1;
  }

  if (pi->conn != NULL) {
     Warning("Got a connection from peer %s in domain %s, but is already conneced!", 
	     instance, peerdomain);
     errno = EISCONN;
     return -1;
  };

  pi->domainid = domid;
  if (pi->domainid == 0) 
    gwid = allocgwid(GWPEER, SRB_ROLE_GATEWAY);
  else 
    gwid = allocgwid(GWREMOTE, SRB_ROLE_GATEWAY);

  
  conn->peerinfo = pi;
  pi->conn = conn;
  pi->gwid = conn->gwid = gwid;

  DEBUG( "conn=%p got a peer %s on domainid %d, gwid = %d pi->conn = %p fd=%d", 
	 conn, instance, pi->domainid, GWID2IDX(gwid), pi->conn, pi->conn->fd);
  gw_setipc(pi);

  return 0;
};

void gw_provideservice_to_peers(char * service)
{
   int i;
   struct gwpeerinfo * pi;
   
   DEBUG( "exporting service %s to all peers", service);
   
   LOCKMUTEX(peersmutex);
   for (i = 0; i < GWcount; i++) {
      pi = & GWENTRY(i);
      if (pi->conn == NULL) {
	 DEBUG(" peer %d is not connected", i);
	 continue;
      };

      if (pi->conn->state != CONNECT_STATE_UP) {
	 DEBUG("peer %d is not yet in connected state, ignoring", i);
	 continue;
      };

      exportservicetopeer(service, pi);
   };
   UNLOCKMUTEX(peersmutex);
   DEBUG( "export complete");
};

void gw_send_to_peers(SRBmessage * srbmsg)
{
   int i;
   struct gwpeerinfo * pi;
   
   DEBUG( "sending a SRB message to all peers");
   
   LOCKMUTEX(peersmutex);
   for (i = 0; i < GWcount; i++) {
      pi = & GWENTRY(i);
      if (pi->conn == NULL) {
	 DEBUG(" peer %d is not connected", i);
	 continue;
      };

      if (pi->conn->state != CONNECT_STATE_UP) {
	 DEBUG("peer %d is not yet in connected state, ignoring", i);
	 continue;
      };

      _mw_srbsendmessage(pi->conn, srbmsg);
   };
   UNLOCKMUTEX(peersmutex);
   DEBUG( "send complete");
};

/* now this is called only every time a new peer is connected. At that
   time we must go thru the full  svc ipc table, and get a list of the
   uniue service names . */

void gw_provideservices_to_peer(GATEWAYID gwid)
{
  struct gwpeerinfo * pi = NULL;
  serviceentry * svctbl = _mw_getserviceentry(0);
  int i, n;
  char ** svcnamelist = NULL;
  DEBUG( "Checking to see if we need to send provide or unprovide to GW %d", GWID2IDX(gwid));

  pi = getKnownPeerByGWID(gwid);

  if (pi == NULL) {
    Fatal("Internal error: could not find a gwpeerinfo entry for gwid %d", GWID2IDX(gwid));
  };
  
  /* get an unique list off all services  */
  n = 0;
  for (i = 0; i < ipcmain->svctbl_length; i++) {
    if (svctbl[i].type == UNASSIGNED) {
       //      DEBUG2("service table entry %d is empty", i);
      continue;
    };

    DEBUG("new service %s, on idx=%d location=%d", svctbl[i].servicename, i, svctbl[i].location);

    /* only imported services from foreign domains may be rexported. */
    if (svctbl[i].location == GWPEER) {
      DEBUG("We don't export services imported from gws in my own domain.");
      continue;
    };

    svcnamelist = charlistaddunique(svcnamelist, svctbl[i].servicename);
  };

  if (svcnamelist == NULL) return;

  /* now, here we got the list of all services with their costs */
  for (i = 0; svcnamelist[i] != NULL; i++) {
    DEBUG(" CALLING exportservicetopeer(%s, pi); i = %d addr: %p", svcnamelist[i], i, svcnamelist[i]); 
    exportservicetopeer(svcnamelist[i], pi);
  };
  free(svcnamelist);
  return;

};

int gw_getcostofservice(char * service)
{
  SERVICEID * svclist;
  serviceentry * svctbl;
  int n = 0, cost = INT_MAX, idx, tmpcost;;

  svctbl = _mw_getserviceentry(0);
  svclist = _mw_get_services_byname(service, NULL, 0);

  DEBUG2("svctbl = %p, svclist = %p",svctbl,  svclist);

  if (svclist  == NULL) return -1;
  if (svctbl  == NULL) return -1;


  for (n = 0; svclist[n] != UNASSIGNED; n++) {
    idx = SVCID2IDX(svclist[n]);
    /* first we check to see if we got the service as a local service,
       if so, we export it with cost=0 (thus here we return 0) the cost
       if bumped on the importing side. */
    tmpcost = svctbl[idx].cost;
    if (tmpcost == 0) {
      DEBUG2("service %s is local cost = 0", service);
      free(svclist);
      return 0;
    };
    
    if ((tmpcost > -1) && (tmpcost < cost))
      cost = tmpcost;
  };
  DEBUG2("service %s has cost = %d", service, cost);
  free(svclist);
  return cost;
};

/* TODO: I'm not happy about clean up, who are resp for sending TERM,
   closing, etc, need a policy */
void gw_closegateway(Connection * conn) 
{
  struct gwpeerinfo * pi = NULL;
  GATEWAYID gwid; 

  if (conn == NULL) {
     return;
  };
  
  gwid = conn->gwid;

  DEBUG( "cleaning up after gwid %d", GWID2IDX(gwid));

  if (gwid != -1) {
     // we get the konownpeer entry and clear it. 
     pi = getKnownPeerByGWID(gwid);
     if (pi != NULL) {
	
	/* TODO: first of all we must unprovde all services importet from peer. */
	impexp_cleanuppeer(pi);
	
	conn = pi->conn;
	DEBUG2("conn = %p", conn);
	
	pi->gwid = UNASSIGNED;
	pi->conn = NULL;
     } 
     
     freegwid(gwid);
  } else {
     Warning("called to clean up after GWID == -1!");     
  };

  // we clear the connections entry.
  if (conn != NULL) {
    DEBUG( "gw has a connection entry fd=%d", conn->fd);
    conn->gwid = UNASSIGNED;
    if (conn->peerinfo)
       ((struct gwpeerinfo *) conn->peerinfo)->conn = NULL;
    conn->peerinfo = NULL;
    conn_del(conn->fd);
  };    

};

/*  here we  do  a non-blocking  connect  to a  known but  unconnected
   gateway. The  SRV INIT are not send  until we get a  write ready on
   poll() and we get a SRB READY message on the connection */
void gw_connectpeer(struct gwpeerinfo * peerinfo)
{
  int s, n, fl, rc;
  Connection * nc;
  GATEWAYID gwid;
  char * _addrlist[2];
  struct hostent * he, _he;
  struct sockaddr raddr;
  struct sockaddr_in * raddr4 = (struct sockaddr_in *) & raddr;

  if (peerinfo == NULL) {
    Error("gw_connectpeer called with NULL argument");
    return;
  }

  if (peerinfo->conn != NULL) {
    DEBUG( "GW already connected");
    return;
  }

  DEBUG( "doing nonblock connect to %s %s", 
	peerinfo->hostname,  peerinfo->instance );

  raddr4->sin_family = AF_INET;
  raddr4->sin_port = htons(SRB_BROKER_PORT);

  // we create the  socker and conn entry
  s = socket(PF_INET, SOCK_STREAM, 0);
  if (s == -1) abort();
  nc = conn_add(s, SRB_ROLE_GATEWAY, CONN_TYPE_GATEWAY);
  nc->state = CONNECT_STATE_CONNWAIT;
  
  /* now convert the hostname in dot format, or lookup the
     hostname. If we've an address in dot format, we create a fake hostent struct */
  rc = inet_pton(AF_INET, peerinfo->hostname,& raddr4->sin_addr.s_addr);
  if (rc > 0) {
    DEBUG( "%s is an IPv4 address, creating an hostent", peerinfo->hostname );
    _he.h_name = peerinfo->hostname;
    _he.h_aliases = NULL;
    _he.h_addrtype = AF_INET;
    _he.h_length = 4;
    _he.h_addr_list = _addrlist;
    _addrlist[0] = (char *) & raddr4->sin_addr.s_addr;
    _addrlist[1] = NULL;;
    he = & _he;
  } else {
    // test for IPv6
    
    /* hostname is canonical, we must lookup */
    
    he = gethostbyname(peerinfo->hostname);
    if (he == NULL) {
      Error("unable to resolve \"%s\", reason %s, will try again", 
	    peerinfo->hostname, hstrerror(h_errno));
      
      conn_del(s);
      close(s);
      return;
    }
  };

  
  // OK we now have a legal hostent, now lets try to connect, but first we need to assign an GWID

  gwid = allocgwid(GWPEER, SRB_ROLE_GATEWAY);
  // if table full
  if (gwid == UNASSIGNED) return;

  nc->gwid = gwid;
  peerinfo->gwid = gwid;

  fl =  fcntl(s, F_GETFL);
  fl |= O_NONBLOCK;
  fcntl(s, F_SETFL, fl);

  /* DNS may give us multiple addresses for a cononical name, we must
     try them all */
  for (n = 0; he->h_addr_list[n] != NULL; n++) {
    // IPv6
    memcpy(&raddr4->sin_addr.s_addr, he->h_addr_list[n], sizeof(struct sockaddr_in));
    
    rc = connect(s, (struct sockaddr *) raddr4, sizeof(struct sockaddr_in));
    if (rc == 0) { /* according to docs this may happen if remote address is localhost */
      peerinfo->conn = nc;
      nc->state = CONNECT_STATE_READYWAIT;
      DEBUG( "connected to peer on localhost");
      break;
    } else if (errno == EINPROGRESS) { // This is the good "normal end
      char buff[64];

      inet_ntop(AF_INET, he->h_addr_list[n], buff, 64);
      DEBUG( "_he = %p he = %p he->h_name %p", &_he, he, he->h_name );
      DEBUG( "connect to peer %s(%s) in progress", he->h_name, buff);
      poll_write(s);
      peerinfo->conn = nc;
      nc->peerinfo = peerinfo; 
      break;
    } else {
      gw_closegateway(nc);
      break;
    };
  };
  return;
};

/* called from a tasklet, needed, teh same tasklet will send
   multicasts, and we connect on their replies. */
void gw_connectpeers(void)
{
   int i; 
   struct gwpeerinfo * pi = NULL;
  
   LOCKMUTEX(peersmutex);
   
   for (i = 0; i < GWcount; i++) {
      pi = &GWENTRY(i);
      if (pi->conn == NULL) {
	 DEBUG( "Unconnected peer at host %s, instance %s", 
		pi->hostname,  pi->instance);      
	 gw_connectpeer(pi);
      } else {
	 DEBUG( "Already Connected peer at host %s, instance %s, fd=%d state = %d", 
		pi->hostname,  pi->instance, 
		pi->conn->fd,  pi->conn->state);
    }
  };
   UNLOCKMUTEX(peersmutex);
};

/* wrapper for sending a multicast on our domain and all defined
   foreign domains */
void gw_sendmcasts(void)
{
  int fd; 
  fd = conn_getmcast()->fd;

  DEBUG( "sending multicast query on fd=%d for my domain %s", 
	fd, globals.mydomain);
  _mw_sendmcastquery(fd, globals.mydomain, NULL);

  remdom_sendmcasts();

  return;
};


/************************************************************************
 * main
 ************************************************************************/

/* undocumented  in lib/mwlog.c */
void _mw_copy_on_stderr(int flag);

int main(int argc, char ** argv)
{
  int loglevel = MWLOG_INFO, gateway = 0, client = 0, port = -1;
  int role = 0;
  char * uri = NULL;
  char c, *name;
  mwaddress_t * mwaddress;
  char logprefix[PATH_MAX] = "mwgwd";
  pthread_t tcp_thread;
  int tcp_thread_rc;
  int rc = 0, idx;
  char * penv;

#ifdef DEBUGGING
  mtrace();
  loglevel = MWLOG_DEBUG2;
#endif

  penv = getenv ("MWGWD_LOGLEVEL");
  if (penv != NULL) {
     rc = _mwstr2loglevel(penv);
     if (rc != -1) {
	loglevel = rc;
     };
  };
  

  name = strrchr(argv[0], '/');
  if (name == NULL) name = argv[0];
  else name++;

  /* first of all do command line options */
  while((c = getopt(argc,argv, "A:l:cgp:L:")) != EOF ){
    switch (c) {       
    case 'l':
       rc =  _mwstr2loglevel(optarg);
       if (rc != -1) loglevel  = rc;
       else usage(argv[0]);
      break;

    case 'L':
      strncpy(logprefix, optarg, PATH_MAX);
      printf("logprefix = \"%s\" at %s:%d\n", logprefix, __FUNCTION__, __LINE__);
      break;

    case 'A':
      uri = strdup(optarg);
      break;

    case 'c':
      client = 1;
      break;

    case 'g':
      gateway = 1;
      break;

    case 'p':
      port = atoi(optarg);
      break;

    default:
      usage(argv[0]);
      break;
    }
  }

  /* env variable to set biglock, default is set in gateway.h, possibly from configure. */
  {
    char * env;

    env = getenv(envBIGLOCK);
    if (env == NULL) globals.biglock = DEFAULT_BIGLOCK_FLAG;
    else globals.biglock = atoi(env);
  };

  mwopenlog(name, logprefix, loglevel);

  Info("MidWay GateWay Daemon version %s starting", mwversion());

  if (argc == optind) {
    DEBUG( "no domain name given");
    globals.mydomain = "";
  } else if (argc == optind+1) {
    DEBUG( "domain name = %s", argv[optind]);
    globals.mydomain=argv[optind];
  } else {
    usage(argv[0]);
  };

  /* if neither g or c was giver, both is assumed.*/
  if ((gateway == 0) && (client == 0)) gateway = client = 1;
  if (client) role |= SRB_ROLE_CLIENT;
  if (gateway) role |= SRB_ROLE_GATEWAY;


  /* the address of the mwd: uri is fetched above either from env, or
     arg.  we fail if the address is not a ipc address. connection to
     a srbp address don't make sence. */
  mwaddress = _mw_get_mwaddress();
  rc = _mwdecode_url(uri, mwaddress);
  
  if (rc != 0) {
    Error("Unable to parse URI %s, expected ipc:12345 " 
	  "where 12345 is a unique IPC key", uri);
    exit(-1);
  };

 if (mwaddress->protocol != MWSYSVIPC) {
    Error("url prefix must be ipc for %s, url=%s errno=%d", argv[0], uri, errno);
    exit(-1);
  };

 /* now we attach to the mwd, if that fail, there are no mwd running
    and we shall exit. */
  rc = _mw_attach_ipc(mwaddress->sysvipckey, MWIPCSERVER);
  if (rc != 0) {
    Error("MidWay instance is not running");
    exit(rc);
  };

  
  /* now we attach the shm control segments. */
  ipcmain = _mw_ipcmaininfo();
  gwtbl = _mw_get_gateway_table();


  /* the logfile revisited, of we used a default logfile above (we're
     not started by mwd we now switch to the SYSTEM defualt
     logfile. */

  if (logprefix[0] == '\0') {
    strncpy(logprefix, ipcmain->mw_homedir, 256);
    strcat(logprefix, "/");
    strcat(logprefix, ipcmain->mw_instance_name);
    strcat(logprefix, "/log/SYSTEM");
    printf("logprefix = %s at %s:%d\n", logprefix, __FUNCTION__, __LINE__);
    mwopenlog(name, logprefix, loglevel);
  };


  if (loglevel <= MWLOG_INFO)
    _mw_copy_on_stderr(0);

  /* another start message since we may have changed logfile. */
  Info("MidWay GateWay Daemon version %s comming up", mwversion());
  globals.ipcserverpid = getpid();


  /* lock the gateway table and assign ourself the first available
     entry, after this point we're really open for buisness. */
  _mw_set_my_gatewayid(allocgwid(GWLOCAL,role));
  idx = _mw_get_my_gatewayid() & MWINDEXMASK;
  if (idx == UNASSIGNED) {
    Error("No entry was available in the gateway IPC table. Exiting...");
    _mw_detach_ipc();
    exit(-1);
  };

  /*********************************************************************************
  /* we can now start in earnest 
   *********************************************************************************/

  
  Info("mwgwd starting gateway id %d domain=%s instance name=%s instance id=%s", 
	MWINDEXMASK&_mw_get_my_gatewayid(), 
	globals.mydomain?globals.mydomain:"(none)", 
       ipcmain->mw_instance_name?ipcmain->mw_instance_name:"(none)",
	ipcmain->mw_instance_id?ipcmain->mw_instance_id:"(none)");

  globals.myinstance = ipcmain->mw_instance_id;

  strncpy(gwtbl[idx].domainname, globals.mydomain, MWMAXNAMELEN);
  strncpy(gwtbl[idx].instancename, globals.myinstance, MWMAXNAMELEN);
  gwtbl[idx].status = MWREADY;

  DEBUG("Subscribing to events");
  rc = _mw_ipc_subscribe("*", 0, MWEVGLOB);
  DEBUG2("subscribe * returned %d", rc);
  rc = _mw_ipc_subscribe(".*", 1, MWEVGLOB);
  DEBUG2("subscribe .* returned %d", rc);

  /* now do the magic on the outside (TCP) */
  tcpserverinit ();

  /* if port is -1 we never has set a port, and we use broker only */
  if (port != -1) {
    rc = tcpstartlisten(port, role);
    if (rc == -1) {
      Error("Failed to init tcp server, reason %d", errno);
      exit(errno);
    };
  };

  rc = pthread_create(&tcp_thread, NULL, tcpservermainloop, &tcp_thread_rc);
  DEBUG( "tcp_thread has id %d,", (int) tcp_thread);

  Info("mwgwd startup complete");

  rc = ipcmainloop();
  DEBUG("ipcmainloop() returned %d, going down", rc);

  globals.shutdownflag = 1;

  Info("Executing normal shutdown");

  pthread_kill(tcp_thread, SIGQUIT);
  pthread_join(tcp_thread, NULL);
  
  Info("Releasing gwtable slot %d", idx);
  freegwid(idx);

  _mw_detach_ipc();
  Info("Bye!");
  exit(rc);
};
