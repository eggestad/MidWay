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

#include <errno.h>
#include <sys/msg.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>

#include <MidWay.h> 
#include <ipcmessages.h>
#include <ipctables.h>
#include <mwserverapi.h>
#include <shmalloc.h>
#include <internal-events.h>
#include "mwd.h"
#include "tables.h"
#include "events.h"

/**********************************************************************
 * this module is exeptional simple and clean.
 * We provide only one function int request_parse().
 * it waits infinitly (until interrupted) on the mwd message queue
 * for administrative requests.
 **********************************************************************/

static char mesgbuffer[MWMSGMAX+1];

static int send_reply(void * data, int len, int mqid)
{
  int rc;
  if (data != mesgbuffer) {
    Error(	  "Internal error, returning message not at beginning at buffer");
    return -ENOSYS;
  };
  /* we do not block mwd because som buggey server/client code,
     let the sender beware, hence IPC_NOWAIT
     */
  rc = msgsnd(mqid, data, len, IPC_NOWAIT); 
  if (rc == -1) {
    Warning(	  "ipc msgsnd on mqid %d returned %d, errno=%d", mqid, rc, errno);
  };
  
  return 0;
};


static int do_attach(void * mp)
{
  Attach * areq;
  int id = -1, type;

  areq = mp;

  /* what the hell did we need the flag for?? */

  DEBUG(	"Got an attach message from pid=%d, on qid=%d client=%s server=%s gatewayid=%d",
	areq->pid, areq->ipcqid, 
	areq->client?"TRUE":"FALSE", 
	areq->server?"TRUE":"FALSE", 
	areq->gwid != -1?areq->gwid & MWINDEXMASK:-1);
  
  areq->mtype = ATTACHRPL;
  areq->srvid = UNASSIGNED;
  areq->cltid = UNASSIGNED;
  
  /* ipcmain->status == MWDEAD really is impossible, advise and exit */
  if (ipcmain->status == MWDEAD) {
    areq->returncode = -ESHUTDOWN;
    send_reply(areq, sizeof(Attach) -sizeof(long),areq->ipcqid);
    return -ESHUTDOWN;
  };
  if ( (ipcmain->status == MWSHUTDOWN) && (areq->server == TRUE) ) {
    areq->returncode = -ESHUTDOWN;
    return send_reply(areq, sizeof(Attach) -sizeof(long),areq->ipcqid);
  };

  areq->returncode = 0;

  /* Request is now deemed OK */
  if (areq->server) {

    // servers are illegal on gateways.
    if (areq->gwid != -1) {
      areq->returncode = -EUCLEAN;
      return send_reply(areq, sizeof(Attach) -sizeof(long),areq->ipcqid);
    };
    
    areq->srvid = addserver(areq->srvname,areq->ipcqid, areq->pid);
    DEBUG("pid=%d, on queue id=%d attached as server %#x",
	  areq->pid, areq->ipcqid, areq->srvid);

    if (areq->srvid < 0) {
      areq->returncode = areq->srvid;
      areq->srvid = UNASSIGNED;
      Warning(	    "Failed to register server pid=%d, on queue id=%d reason=%d",
	    areq->pid, areq->ipcqid, areq->returncode);
      return send_reply(areq, sizeof(Attach) -sizeof(long),areq->ipcqid);
    } 
    Info("SERVER ATTACHED: pid %d as server %#x",
	  areq->pid, areq->srvid);
  };

  /* check to see if it was a client handling gateway or an IPC agent */
  if (areq->client) {
    if (areq->gwid != -1) {
      type = MWNETCLIENT;
      id = areq->gwid;
    } else {
      if (areq->srvid > 0) {
	id = (int) areq->srvid;
 	type = MWIPCSERVER;
      } else {
	id = areq->cltid;
	type = MWIPCCLIENT; 
      };
    };
  
    areq->cltid = addclient(type, areq->cltname,areq->ipcqid, areq->pid, id);
    DEBUG("pid=%d, on queue id=%d attached as client %#x, type=%d",
	  areq->pid, areq->ipcqid, id, type);
    if (areq->cltid < 0) {
      areq->returncode = areq->cltid;
      areq->cltid = UNASSIGNED;
      Warning("Failed to register client pid=%d, on queue id=%d reason=%d",
	      areq->pid, areq->ipcqid, areq->returncode);
    }
    
    if (type == MWIPCCLIENT) {
      Info("CLIENT ATTACHED: pid %d clientid %d",
	   areq->pid, areq->cltid & MWINDEXMASK);
    } else {
      Info("CLIENT ATTACHED: on gateway %d clientid %d",
	   areq->gwid & MWINDEXMASK, areq->cltid & MWINDEXMASK);
    }; 
  };
  return send_reply(areq, sizeof(Attach) -sizeof(long),areq->ipcqid);
};

static int do_detach(void * mp)
{
  Attach * dm;
  int rc;

  dm = mp;
  if (dm->mtype != DETACHREQ) {
    Error("Expected a detach request but got 0x%lx", dm->mtype);
    return -EUCLEAN;
  };
  
  DEBUG("got a detach request from pid=%d on mqid %d, force=%s", 
	 dm->pid, dm->ipcqid, (dm->flags & MWFORCE)?"Yes":"No");
  dm->mtype = DETACHRPL;

  /* we need better tests here to ensure that the correct entry is 
   * deleted. However if our ciode is correct, this is sufficient.
   * In either case, how de we respond to a failure. mwd's periodic 
   * cleanups would be the preffered way.
   */
  if (dm->server) {
    rc = delserver(dm->srvid);
    
    if (rc == 0) {
      Info("DETACHED SERVER pid %d serverid %#x", 
	     dm->pid, dm->srvid);
    } else {
      Warning(	     "FAILED TO DETACHED SERVER pid %d  serverid %#x, rc=%d", 
	     dm->pid, dm->srvid, rc);
    }
    /* remove all service entries belonging to this server */
    delallservices(dm->srvid);

    /* if this server is the only one providing this service 
     * and it is exported thru a gateway, the gateway may want to 
     * take the service offline on the remote domains.
     * we must signal the gateway server here.
     */
  };
  if (dm->client) {
    rc = delclient(dm->cltid);
    if (rc == 0) {
      Info("CLIENT DETACHED pid %d clientid %d", 
	     dm->pid, dm->cltid&MWINDEXMASK);
    } else {
      Warning("FAILED TO DETACHED CLIENT pid=%d clientid %d, rc=%d", 
	     dm->pid, dm->cltid&MWINDEXMASK, rc);
    }
  };
  dm->returncode = 0;

  /* if force flag, the server/client is already down, no reply */
  if (dm->flags & MWFORCE) {
    return 0;
  }
  else 
    return send_reply(dm, sizeof(Attach) -sizeof(long),dm->ipcqid);
};
  
static int do_provide(void * mp)
{
  int type, rmqid;
  Provide * pmesg;
  serverentry * se = NULL;
  gatewayentry * ge = NULL;

  pmesg = (Provide * ) mp;
  DEBUG("Got an provide request from server %#x for service \"%s\"", 
	pmesg->srvid, pmesg->svcname);

  pmesg->mtype = PROVIDERPL;

  /* Gateways don't used this message, but goes directly to the table
   * since they are a mwd child. */

  if (ipcmain->status == MWSHUTDOWN) {
    pmesg->returncode = -ESHUTDOWN;
    return send_reply(pmesg,  sizeof(Provide) -sizeof(long), se->mqid);
  };
  pmesg->returncode = 0;

  /* We should do duplicate test here and 
     we should do uniqueness tests here */
  
  if ((pmesg->flags & MWCONV) == MWCONV) 
    type = MWCONVSVC; 
  else
    type = MWCALLSVC;

  DEBUG("srvid = %d gwid = %d", pmesg->srvid, pmesg->gwid);
  if (pmesg->srvid != -1) {
    DEBUG("1 srvid = %d gwid = %d", pmesg->srvid, pmesg->gwid);
    se = _mw_get_server_byid(pmesg->srvid);
    if (se == NULL) {
      pmesg->returncode = -ENOENT;
      pmesg->svcid = UNASSIGNED;
      Warning("Rejected a provide of service \"%s\" from server %#x which is not attached, "
	      "no where to send reply!", pmesg->svcname, pmesg->srvid);
      return -ENOENT;
    } 
    rmqid = se->mqid;
    /* now we've decided that the request is OK, creating entries. */
    pmesg->svcid = addlocalservice(pmesg->srvid,pmesg->svcname,type);
    
  } else if (pmesg->gwid != -1) {
    DEBUG("2 srvid = %d gwid = %d", pmesg->srvid, pmesg->gwid);
    ge = _mw_get_gateway_byid(pmesg->gwid);
    if (ge == NULL) {
      pmesg->returncode = -ENOENT;
      pmesg->svcid = UNASSIGNED;
      Warning("Rejected a provide of service \"%s\" from gateway %#x which is not attached, "
	      "no where to send reply!", pmesg->svcname, pmesg->gwid);
      return -ENOENT;
    } 
    rmqid = ge->mqid;
    /* now we've decided that the request is OK, creating entries. */
    pmesg->svcid = addremoteservice(pmesg->gwid,pmesg->svcname,pmesg->cost, type);
  } else {
    Warning("Got a provide with both srvid and gwid == -1, rejecting");
    pmesg->returncode = -EINVAL;
    pmesg->svcid = UNASSIGNED;
    goto errout;
  };

  if (pmesg->svcid < 0) {
    Warning("Rejected a provide of service \"%s\" from server %#x reason %d", pmesg->svcname, pmesg->srvid, pmesg->svcid);
    pmesg->returncode = pmesg->svcid;
    pmesg->svcid = UNASSIGNED;
  } else {
    Info("PROVIDE: Server %#x gateway %#x provided service \"%s\"",  
	  pmesg->srvid,  pmesg->gwid, pmesg->svcname);
  };
  
 errout:
  DEBUG("Replying to provide request from server %#x gateway %#x on mqid %d: service \"%s\" got id %#x rcode = %d", 
	pmesg->srvid, pmesg->gwid, rmqid, 
	pmesg->svcname, pmesg->svcid, 
	pmesg->returncode );
  
  return send_reply(pmesg,  sizeof(Provide) -sizeof(long), rmqid);
};

static int do_unprovide(void * mp)
{
  Provide * pmesg;
  serviceentry * svcent;
  serverentry * srvent;
  gatewayentry * gwent;
  int mqid = -1;

  pmesg = (Provide * ) mp;
  DEBUG("Got an unprovide request from server %#x or gateway %#x for service \"%s\" (%#x)", 
	pmesg->srvid, pmesg->gwid, pmesg->svcname, pmesg->svcid);

  svcent = _mw_getserviceentry(SVCID(pmesg->svcid));

  if (svcent == NULL) {
    Error("unable to get service tbl entry for %d", SVCID(pmesg->svcid));
    return -ENOENT;
  };

  if (pmesg->gwid != svcent->gateway)   
    Warning("in unprovide request gateway ids mismatch pmesg->gwid %#x != svcent->gateway %#x", 
	    pmesg->gwid,  svcent->gateway);
  if (pmesg->srvid != svcent->server)   
    Warning("in unprovide request server ids mismatch pmesg->srvid %#x != svcent->server %#x", 
	    pmesg->srvid, svcent->server);

  DEBUG("service entry: id=%x server=%x gw=%x", SVCID(pmesg->svcid), svcent->server, svcent->gateway);

  Info("UNPROVIDE: Service \"%s\" on server %#x or gateway %#x", pmesg->svcname, pmesg->srvid, pmesg->gwid);
  if (svcent->server != UNASSIGNED) {
     srvent = _mw_getserverentry(svcent->server);
     if (srvent) mqid = srvent->mqid;      
     DEBUG("srv %p mqid = %d", srvent, mqid);
  } else {
     gwent = _mw_getgatewayentry(svcent->gateway);
     if (gwent) mqid = gwent->mqid;
     DEBUG("gw %p mqid = %d", gwent, mqid);
  };

  pmesg->returncode = delservice(pmesg->svcid);
  if (pmesg->returncode != 0) {
     Warning("Failed to unprovide service \"%s\" from server %#x reason %d", 
	     pmesg->svcname, pmesg->srvid, pmesg->returncode); 
  }

  DEBUG("Replying to unprovide request for \"%s\" rcode = %d", 
	pmesg->svcname,pmesg->returncode );
  pmesg->mtype = UNPROVIDERPL;
  return send_reply(pmesg,  sizeof(Provide) -sizeof(long), mqid);
};

static int do_admin(void * mp)
{
  int rc;
  Administrative * admmsg;

  if (mp == NULL) return -EINVAL;
  admmsg = mp;

  switch(admmsg->opcode) {
  case ADMSHUTDOWN:
    ipcmain->shutdowntime = time(NULL) + admmsg->delay;
    mwaddtaskdelayed(do_shutdowntrigger, -1, admmsg->delay);

    Info("Received a shutdown request from client %d, shutdown in %d seconds", 
	 admmsg->cltid & MWINDEXMASK, admmsg->delay);

    rc = kill(ipcmain->mwwdpid, SIGALRM);
    if (rc != 0) {
      Warning("Failed to send the watchdog a SIGALRM, reason: %s", strerror(errno));
    };

    return 0;
  }
  Warning("Got an unknown request code %d on an administrative request from client %d" , 
	admmsg->opcode, admmsg->cltid & MWINDEXMASK);
  return -EBADRQC;
};

/*************************************************************************/


static int lastallocedid = LOW_LARGE_BUFFER_NUMBER;

static int getnextallocid(void)
{
   int i, rc = 0, n = 0;
   char path[256];
   i = lastallocedid;
   do {
      i++;
      n++;
      if(i < 0) { i = LOW_LARGE_BUFFER_NUMBER +1;};
      if (n < 0) { return -ENOMEM;};

      snprintf(path, 256, "%s/%d", ipcmain->mw_bufferdir, i);
      rc = access(path, F_OK);
   } while (rc == 0);
   lastallocedid = i;
   return i;
};

static int do_alloc(void * mp)
{
   Alloc * allocmesg;
   char path[256];
   int rc, fd, id;
   long s, ps;
   off_t n;
   mode_t mode, mask;

   chunkhead ch;

   if (mp == NULL) return -EINVAL;
   allocmesg = (Alloc *) mp;
   
   allocmesg->mtype = ALLOCRPL;

   id = getnextallocid();
   DEBUG("alloc buffer id %d", id);

   if (id < LOW_LARGE_BUFFER_NUMBER) {
      Error ("no free file slot for buffer");
      allocmesg->bufferid = -1;

      _mw_ipc_putmessage(allocmesg->mwid, (char *) allocmesg, sizeof(Alloc), 0);	
      return -EINVAL;
   };
   
   if (allocmesg->size > LONG_MAX) {
      return -ENOMEM;
   };
   s = allocmesg->size + sizeof(chunkhead);
   ps =  s / get_pagesize() + ((s % get_pagesize())?1:0);
   DEBUG("size=%ld needed pages = %ld", s, ps);

   snprintf(path, 256, "%s/%d", ipcmain->mw_bufferdir, id);  
   
   mask = umask(0);
   mode = mwdheapmode() & ~mask;
 
   if (mode&S_IRGRP)
      mode |= S_IWGRP;
   /* and for other access */
   if (mode&S_IROTH)     
      mode |= S_IWOTH;
   DEBUG("mask is %o opening with mode %o", mask, mode);

   fd = open(path, O_WRONLY|O_CREAT|O_EXCL, mode);  
   umask(mask);
   if (fd < 0) {
      Error("failed to open buffer file %s reason %s", path,  strerror(errno));
      return -ENOMEM;
   };


   ch.ownerid = allocmesg->mwid;
   ch.size = allocmesg->size;
   rc = write(fd, &ch, sizeof(chunkhead));
   Assert(rc == sizeof(chunkhead));

   s = (ps * get_pagesize())-1;
   DEBUG("buffer %d has %ld pages seeking to %ld", id, ps, s);
   n = lseek(fd, s, SEEK_SET); 
   DEBUG("seek to %#lx resulted in %#lx", s, n);
   Assert(n == s);

   rc = write(fd, "", 1);
   Assert(rc == 1);
   close(fd);

   allocmesg->bufferid = id;
   allocmesg->mtype = ALLOCRPL;
   allocmesg->pages = ps;

   _mw_ipc_putmessage(allocmesg->mwid, (char *) allocmesg, sizeof(Alloc), 0);	
   return 0;  
};

static int do_free(void * mp)
{
  Alloc * allocmsg;
  char path[256];

  if (mp == NULL) return -EINVAL;
  allocmsg = mp;
     
  
  DEBUG("freeing file for bufferid %d", allocmsg->bufferid);
  snprintf(path, 256, "%s/%d", ipcmain->mw_bufferdir, allocmsg->bufferid);
  
  unlink(path);
  
  return 0;
  
};

/*************************************************************************/

static int do_event(void * mp)
{
  Event * evmsg;

  if (mp == NULL) return -EINVAL;
  evmsg = mp;
  return   event_enqueue(evmsg);
};

static int do_eventack(void * mp)
{
  Event * evmsg;

  if (mp == NULL) return -EINVAL;
  evmsg = mp;
  return   event_ack(evmsg);
};

static int do_subscribe(void * mp)
{
  Event * evmsg;
  int rc;

  if (mp == NULL) return -EINVAL;
  evmsg = mp;
  rc = event_subscribe(evmsg->event, evmsg->senderid, evmsg->subscriptionid, evmsg->flags);

  evmsg->mtype = EVENTSUBSCRIBERPL;
  evmsg->returncode = rc;
  _mw_ipc_putmessage(evmsg->senderid, (char *) evmsg, sizeof(Event), 0);

  return 0;
};

static int do_unsubscribe(void * mp)
{
  Event * evmsg;
  int rc;

  if (mp == NULL) return -EINVAL;
  evmsg = mp;
  rc = event_unsubscribe(evmsg->subscriptionid, evmsg->senderid);

  evmsg->mtype = EVENTUNSUBSCRIBERPL;
  evmsg->returncode = rc;
  _mw_ipc_putmessage(evmsg->senderid, (char *) evmsg, sizeof(Event), 0);

  return 0;
};

/* this replaces _mwGetServiceRequest  in a normal server*/
static int do_call(void * mp)
{
  int rc;
  Call * callmsg;
  mwsvcinfo svcinfo;
  extern int _mw_requestpending;

  if (mp == NULL) return -EINVAL;

  callmsg = mp;
  _mw_server_set_callbuffer(callmsg);

  DEBUG("do_call starts");

  rc = _mw_set_deadline(callmsg, &svcinfo);
  if (rc < 0) {
    Warning("Got a service request that had already expired by "
	   "%d milliseconds, replying ETIME", 
	   -rc);
    _mw_ipc_putmessage(callmsg->cltid, (char *) callmsg, sizeof(Call), 0);
    _mw_set_my_status(NULL);
    errno = ETIME;
    return -ETIME;
  };

  svcinfo.cltid = callmsg->cltid;
  svcinfo.srvid = callmsg->srvid;
  svcinfo.svcid = callmsg->svcid;
  svcinfo.flags = callmsg->flags;
  

  memset(svcinfo.service, 0, MWMAXSVCNAME);
  strncpy(svcinfo.service, callmsg->service, MWMAXSVCNAME);

  _mw_getbuffer_from_call_to_svcinfo(&svcinfo, callmsg);


  _mw_set_my_status(callmsg->service);
  if (!(callmsg->flags & MWNOREPLY) )
    _mw_requestpending = 1;

  DEBUG("calling mwCallCServiceFunction");
  return _mwCallCServiceFunction(&svcinfo);

};


/* possible retrun codes are only errors, or 0.
 * if we retrun EINTR a msgrcv was interrupted. THis should be due
 * one of the signals we can receive and should be considered OK.
 * typical SIGALRM for a local timer, or SIGCHLD if one of the child
 * gateway or client handlers died. 
 */
int parse_request(int nonblock)
{  
  int rc, i;
  long mtype;
  char hex[330], shex[4];

  rc = msgrcv(mymqid(), (void *) mesgbuffer, MWMSGMAX, 0, nonblock?IPC_NOWAIT:0); 
  DEBUG("rc = %d max = %lu errno = %d", rc, MWMSGMAX, errno);
  /*  timeout is handleled by an alarm() else where, we simply return EINTR */
  
  if (rc == -1) {
    /* interrupts do usually happen for a reason. mwd should go down on SIGTERM.
       shutdown arrived. It really is the watchdog process that should do */
    if ((errno == EINTR) || (errno == ENOMSG)) {
      if (ipcmain->status ==  MWDEAD) return -ESHUTDOWN;

      /* 
      if (flags.terminate) {
	ipcmain->status = MWSHUTDOWN;
	ipcmain->shutdowntime = time(NULL);
      }
      */
      return -errno;
    }
    
    Error("msgrcv() returned no message, reason=%s(%d)", strerror(errno), errno);
    if (errno == E2BIG) {
      hex[0] = '\0';
      rc = msgrcv(mymqid(), (void *) mesgbuffer, MWMSGMAX, 0,MSG_NOERROR); 
      if (rc > 300) rc = 300;
      for (i = 0; i < rc; i++) {
	sprintf(shex,"%2.2x ", (unsigned char) mesgbuffer[i]);
	strcat(hex, shex);
      };
      Error("Got a too big message, first %d octets are: %s", rc, hex);
    };
    /* posible the message queue has been deleted. We call init_ipcmain 
       to recreate it.*/
    if (errno == EINVAL) {
      init_maininfo();
    };
    
    return -errno;
  }

  _mw_dumpmesg(mesgbuffer);

  mtype = * (long *) mesgbuffer;  

  switch(mtype) {
    
  case ATTACHREQ:
    if (rc < (sizeof(Attach) -sizeof(long))) return -EMSGSIZE;
    return do_attach(mesgbuffer);

  case DETACHREQ:
    if (rc < (sizeof(Attach) -sizeof(long))) return -EMSGSIZE;
    return do_detach(mesgbuffer);

  case PROVIDEREQ:
    if (rc < (sizeof(Provide) -sizeof(long))) return -EMSGSIZE;
    return do_provide(mesgbuffer);

  case UNPROVIDEREQ:
    if (rc < (sizeof(Provide) -sizeof(long))) return -EMSGSIZE;
    return do_unprovide(mesgbuffer);

  case EVENTSUBSCRIBEREQ:
    if (rc < (sizeof(Event) -sizeof(long))) return -EMSGSIZE;
    return do_subscribe(mesgbuffer);

  case EVENTUNSUBSCRIBEREQ:
    if (rc < (sizeof(Event) -sizeof(long))) return -EMSGSIZE;
    return do_unsubscribe(mesgbuffer);

  case EVENT:
    if (rc < (sizeof(Event) -sizeof(long))) return -EMSGSIZE;
    return do_event(mesgbuffer);

  case EVENTACK:
    if (rc < (sizeof(Event) -sizeof(long))) return -EMSGSIZE;
    return do_eventack(mesgbuffer);

  case ADMREQ:
    if (rc < (sizeof(Administrative) -sizeof(long))) return -EMSGSIZE;
    return do_admin(mesgbuffer);

  case SVCCALL:
  case SVCFORWARD:
    if (rc < (sizeof(Call) -sizeof(long))) return -EMSGSIZE;
    return do_call(mesgbuffer);

  case ALLOCREQ:
     if (rc < (sizeof(Alloc) -sizeof(long))) return -EMSGSIZE;
     return do_alloc(mesgbuffer);
     
  case FREEREQ:
     if (rc < (sizeof(Alloc) -sizeof(long))) return -EMSGSIZE;
     return do_free(mesgbuffer);

  };  


     
  return -EBADMSG;  
};
