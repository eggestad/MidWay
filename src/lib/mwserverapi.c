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
 * Revision 1.1  2000/03/21 21:04:15  eggestad
 * Initial revision
 *
 * Revision 1.1.1.1  2000/01/16 23:20:12  terje
 * MidWay
 *
 */

#include <errno.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>

#include <MidWay.h>
#include <ipcmessages.h>
#include <ipctables.h>
#include <shmalloc.h>

/**********************************************************************
 *
 * Here we have the functions that are necessary to make a server.
 * Since servers must use IPC to communicate they are implicit IPC.
 * A lot of the stuff here should really be in ipcmessages.c
 * At a time I wondered if I should make a client only lib to same RAM 
 * on the client, and I though I needed different libs for IPC and TCP/IP. 
 * Right now I've decided agains it, but the question may come back up again.
 *
 * NB: NOT THREAD SAFE, threads must create their own buffers.
 *********************************************************************/
static char buffer[MWMSGMAX];
static Call * callmesg = (Call *) buffer;

extern int _mw_attached;
extern int _mw_fastpath;
static int provided = 0;

/*
  We have an internal list for holding the pionter ro the functioon to
  be called for each service. */

struct ServiceFuncEntry {
  SERVICEID svcid;
  int (*func)  (mwsvcinfo*);
  struct ServiceFuncEntry * next;
};

static struct ServiceFuncEntry * serviceFuncList = NULL;

/* two methods for poping and pushing functions references to C
   functions coupled with SERICEID.  */

static struct ServiceFuncEntry * pushservice(SERVICEID sid, int (*svcfunc) (mwsvcinfo*)) 
{
  struct ServiceFuncEntry * newent;

  newent = malloc(sizeof(struct ServiceFuncEntry));
  newent->svcid = sid;
  newent->func = svcfunc;
  newent->next = NULL;
  
  /* push at top of list, ( we really should have an index.*/
  if (serviceFuncList == NULL) {
    serviceFuncList = newent;
  } else {
    newent->next =  serviceFuncList;
    serviceFuncList = newent;
  };
  return newent;
};

static void popservice(SERVICEID sid)
{
  struct ServiceFuncEntry * thisp, * prev;

  if (serviceFuncList == NULL) return;
  
  thisp = serviceFuncList;
  if (thisp->svcid == sid) {
    serviceFuncList = thisp->next;
    free(thisp);
    return;
  }

  prev = thisp;
  thisp = thisp->next;

  while (thisp != NULL) {
    if (thisp->svcid == sid) {
      prev->next = thisp->next;
      free(thisp);
      return;
    };
    thisp = thisp->next;
  };
  return;
};

  
/* 
   Handling of signals. Here we install default actions on 
   HUP, INT, QUIT, TERM,  SEGV, FPR, ILL
   which is to mark our self down in SHM tables, send detach to mwd, 
   detach ipc, and exit(),
*/
#include <signal.h>
static void install_sigactions(int flag);

static void signal_handler(int sig) 
{
  static sigloop = 0;

  /*  install_sigactions(1);*/
  
  mwlog(MWLOG_INFO, "Going down on signal %d", sig);
  if (!sigloop) {
    sigloop++;
    _mw_set_my_status(SHUTDOWN);
    mwdetach();
    _mw_detach_ipc();
  } else {
    abort();
  };
  exit(-1);
};
  
static void install_sigactions(int flag)
{
  struct sigaction sa;
  
  if (flag)
    sa.sa_handler = SIG_DFL;
  else 
    sa.sa_handler = signal_handler;
  sigfillset(&sa.sa_mask);
  sa.sa_flags = 0;
  sa.sa_restorer = NULL;

  sigaction(SIGHUP, &sa, NULL);
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);
  sigaction(SIGQUIT, &sa, NULL);
  sigaction(SIGSEGV, &sa, NULL);
  sigaction(SIGFPE, &sa, NULL);
  sigaction(SIGILL, &sa, NULL);
};


/*********************************************************************
 *
 *  API begins here 
 *
 ********************************************************************/
  
/*
  provide and unprovide sends messages to mwd about what service calls
  this server provides.  */

int mwprovide(char * service, int (*svcfunc) (mwsvcinfo*), int flags)
{
  SERVICEID svcid;
  serviceentry * svcent;
  int len;
  int rc;
  
  if (! _mw_attached) return -ENOTCONN;
  rc = _mwsystemstate();
  if (rc) return rc;

  install_sigactions(0);

  mwlog(MWLOG_DEBUG, "mwprovide() providing %s to mwd", 
	service);
  svcid = _mw_ipc_provide(service,flags);
    
  if (svcid < 0) return svcid;
  /* THREAD MUTEX BEGIN */
  svcent = _mw_get_service_byid(svcid);
  if (svcent == NULL) {
    mwlog(MWLOG_ERROR, "This can't happen, possible error in mwd, got an OK on a provide request but returned service id  (%#x) does not exist ", svcid);
    return -1;
  };

  /* instead of an index internally here, by storing tha addres of the ServiceFuncEntry
     object in the service entry in the SHM Tables. It must be searched anyway
     by the client or gateway process.
     Likewise we can from say Perl store a reference here.
  */
  svcent->svcfunc = (void *) pushservice (svcid, svcfunc);
  provided++;

  /* THREAD MUTEX ENDS */
  return svcid;
};

int mwunprovide(char * service)
{
  SERVICEID svcid;
  int rc;

  if (!_mw_attached) return -ENOTCONN;
  /* short cut if now services are provided.*/;
  if (!provided) return -ENOENT;
  /* We must lookup service type somewhere LOOKATME */
  svcid = _mw_get_service_byname(service, 0);
  if (svcid < 0 ) {
    mwlog(MWLOG_ERROR, "mwunprovide() failed with rc=%d",svcid);
    return svcid;
  };

  rc = _mw_ipc_unprovide(service,  svcid);
  popservice(svcid);
  provided--;
  mwlog(MWLOG_DEBUG, "mwunprovide() returned %d");
  return rc;
}

/* These are used to calculate statistics between mwservicerequest and mwreply.
   NB: NOT TREADSAFE. If we make us thread safe these are thread private data.
*/
static struct timeval starttv, endtv; 
static int waitmsec, servemsec, _mw_requestpending = 0;

static void completeStats()
{
  gettimeofday(&endtv, NULL);
  servemsec = (endtv.tv_sec - starttv.tv_sec) * 1000 
    + (endtv.tv_usec - starttv.tv_usec) / 1000;
  _mw_update_stats(_mwCurrentMessageQueueLength(), waitmsec, servemsec);
  return;
};

/*
  mwreply() are used by an application service routines to send replys.
  Since a service routine must reply an application return code as well 
  as (normally) a data string we can't use the normal return.
  Another reason is that the serice routine should be able to do 
  fault recovery if the caller fails to get the reply.
*/
int mwreply(char * data, int len, int returncode, int appreturncode)
{
  char * ptr;
  int rc, dataoffset;
  void * dbuf;
  
  rc = _mwsystemstate();
  if (rc) return rc;

  /* First we handle return buffer. If data is not NULL, and len is 0,
     datat is NULL terminated. if buffer is not a shared memory
     buffer, get one and copy over. */
    
  if (data != NULL) {
    if (len == 0) len = strlen(data);

    dataoffset = _mwshmcheck(data);
    if (dataoffset == -1) {
      dbuf = _mwalloc(len);
      if (dbuf == NULL) {
	mwlog(MWLOG_ERROR, "mwalloc(%d) failed reason %d", len, (int) errno);
	return -errno;
      };
      memcpy(dbuf, data, len);
      dataoffset = _mwshmcheck(dbuf);
    }
    
    callmesg->data = dataoffset;
    callmesg->datalen = len;
  } else {
    callmesg->data = 0;
    callmesg->datalen = 0;
  }

  /* return code handling. rteurncode is boolean. */
  if (returncode)
    callmesg->returncode = 0;
  else 
    callmesg->returncode = EFAULT;
  callmesg->appreturncode = appreturncode;

  /* set "opcode" on message */
  callmesg->mtype = SVCREPLY;
  callmesg->srvid = _mw_get_my_serverid();

  /* send reply buffer*/
  /* we should check to see if callmesg->ipcqid is legal in client table*/
  rc = _mw_ipc_putmessage(callmesg->cltid, (char *) callmesg, sizeof(Call), IPC_NOWAIT);
  if (rc != 0) {
    mwlog(MWLOG_WARNING,"Failure on replying to client %#x reason %s", 
	  callmesg->cltid, errno);
    return -errno;
  } 
  mwlog(MWLOG_DEBUG, "mwreply sent to client %d rc= %d", 
	callmesg->cltid, callmesg->returncode);

  _mw_requestpending = 0;
  
  gettimeofday(&endtv, NULL);
  completeStats();
  return rc;
};

/*
  mwforward() are used by an application service routines to send the request
  onto another server, substitution the data buffer with a new. 
  The next service will either do anothre forward, or send the response 
  directly to the client. This and the services involvement, just like mwreply.
*/
int mwforward(char * svcname, char * data, int len, int flags)
{
  char * ptr;
  int rc, dataoffset, dest;
  int ipcflags = 0;
  void * dbuf;
  int maxforwards = 100;
  SERVICEID svcid;

  rc = _mwsystemstate();
  if (rc) return rc;

  /* we should provite a way to set the "TTL" */
  if (callmesg->forwardcount > maxforwards) {
    mwlog(MWLOG_WARNING, "Tried to forward a request from %#x that already had been forwarded %d times.", callmesg->cltid, maxforwards);
    return -ELOOP;
  };
  callmesg->forwardcount++;

  /* find the svcid of the service we are to call. */
  svcid = _mw_get_service_byname(svcname,flags&MWCONV);
  if (svcid < 0) {
    mwlog(MWLOG_WARNING,"mwforward() getting a serviceid for service %s failed reason %d",
	  svcname,svcid);
    return svcid;
  };

  dest = _mw_get_server_by_serviceid (svcid);
  if (dest < 0) {
    mwlog(MWLOG_WARNING,"mwforward(): getting the serverid for serviceid %d(%s) failed reason %d",
	  svcid, svcname, dest);
    return dest;
  };


  /* First we handle return buffer. If data is not NULL, and len is 0,
     datat is NULL terminated. if buffer is not a shared memory
     buffer, get one and copy over. */
  if (data != NULL) {
    if (len == 0) len = strlen(data);

    dataoffset = _mwshmcheck(data);
    if (dataoffset == -1) {
      dbuf = _mwalloc(len);
      if (dbuf == NULL) {
	mwlog(MWLOG_ERROR, "mwalloc(%d) failed reason %d", len, (int) errno);
	return -errno;
      };
      memcpy(dbuf, data, len);
      dataoffset = _mwshmcheck(dbuf);
    }
    
    callmesg->data = dataoffset;
    callmesg->datalen = len;
  } else {
    callmesg->data = 0;
    callmesg->datalen = 0;
  }

  /* set "opcode" on message */
  callmesg->mtype = SVCCALL;
  callmesg->srvid = _mw_get_my_serverid();

  /* flags tarnsalition, INCOMPLETE */
  if (flags & MWNOBLOCK) ipcflags != IPC_NOWAIT;

  rc = _mw_ipc_putmessage(dest, (char *) callmesg, sizeof(Call), ipcflags);
  if (rc != 0) {
    mwlog(MWLOG_WARNING,"Failure on replying to client %#x reason %s", 
	  callmesg->cltid, errno);
    return -errno;
  } 
  mwlog(MWLOG_DEBUG, "mwreply sent to client %d rc= %d", 
	callmesg->cltid, callmesg->returncode);

  _mw_requestpending = 0;
  
  gettimeofday(&endtv, NULL);
  completeStats();
  return rc;
};

/*
 * This function is used to perform a service call. Thus it finds out if 
 * we really provides this service, sets stats data in shm and calls
 * the applictaion service function.
 */
mwsvcinfo *  _mwGetServiceRequest (int flags)
{
  int rc, mesglen;
  ipcmaininfo * ipcmain;
  serviceentry * svcent;
  serverentry * srvent;
  mwsvcinfo * svcreqinfo;

  if (!_mw_attached) { errno = ENOTCONN; return NULL;}
  if (!provided) { errno = ENOENT; return NULL;};

  mesglen = MWMSGMAX;
  ipcmain = _mw_ipcmaininfo();
  if (ipcmain == NULL) {errno = EADDRNOTAVAIL; return NULL;}

  _mw_set_my_status(NULL);

  rc = _mw_ipc_getmessage(buffer, &mesglen, SVCCALL, flags);
  _mw_requestpending = 1;

  if (rc < 0) {
    /* mwd will notify us about shutdown by removing our message queue.*/
    if ( (errno == EIDRM) || (errno == EINVAL) ) {
      mwlog(MWLOG_INFO, "Administrative Shutdown.");
      _mw_ipcsend_detach(1);
      _mw_detach_ipc();
      errno = ESHUTDOWN;
      return NULL;
      /* should we recreate the message queue if accidental removes? No, for now */
    }
    errno = rc > 0 ? rc : -rc;
    return NULL;
  };

  _mw_set_my_status(callmesg->service);
  
  mwlog(MWLOG_DEBUG,
	"mwfetch: got a CALL/FORWARD to service \"%s\" id %#x from clt:%#x srv:%#x", 
	callmesg->service, callmesg->svcid, 
	callmesg->cltid, callmesg->srvid);
    
  /* timeout info */
  svcent = _mw_get_service_byid(callmesg->svcid);
  if (svcent == NULL) {
    mwlog(MWLOG_ERROR, "Can't happen, Got a call to %s (%d) but it does not exist in BBL", 
	  callmesg->service, callmesg->svcid);
    callmesg->returncode = -ENOENT;
    _mw_ipc_putmessage(callmesg->cltid,callmesg, sizeof(Call), 0);
    _mw_requestpending = 0;  
    _mw_set_my_status("(ERROR)");
    errno = ENOENT;
    return NULL;
  };

    /*    if (callmesg->srvid != _mw_get_my_serverid()) {
	  mwlog (MWLOG_ERROR, "Got a call request for service \"%s\" serviceid %#x and serverid %#x, but my serverid is %#x", 
	  callmesg->service, callmesg->svcid, 
	  callmesg->srvid, _mw_get_my_serverid());
	  callmesg->returncode = -EBADMSG;
	  _mw_ipc_putmessage(callmesg->cltid,callmesg, sizeof(Call), 0);
	  _mw_set_my_status(NULL);
	  errno = EBADMSG;
	  return NULL;
	  }
    */
    /* In the call stuct we get the time issued in struct timeval format and timeout in millisecs.
       In the SI struct we give the deadline in struct timeval type data.
       If the deadline is allready reached we just return ETIME.
       Also we use this data to calc the time it took from issued to we begun to serv it, 
       and how long we used to process the service request.
       Also we keep track of busy time.
       All this goes into the statistic section for the server in shm tbls. 
    */

  svcreqinfo = malloc(sizeof(mwsvcinfo));
		     
  /* now we do calulations on issue time, now, and deadlines.
     we need to update stats (NYI) and to check to see if the deadline has expired, 
     and returne a timeout reply if it has.
  */
  gettimeofday(&starttv, NULL);
  if (callmesg->timeout > 0 ) {
    svcreqinfo->deadline  = callmesg->issued + callmesg->timeout/1000;
    svcreqinfo->udeadline = callmesg->uissued; + ((callmesg->timeout)%1000)*1000000;
  } else {
    svcreqinfo->deadline = 0;
    svcreqinfo->udeadline = 0;
  };
  waitmsec = (starttv.tv_sec - callmesg->issued) * 1000 
    + (starttv.tv_usec - callmesg->uissued) / 1000;
  
  if ( (callmesg->timeout > 0) && 
       (starttv.tv_sec >= svcreqinfo->deadline) && 
       (starttv.tv_usec > svcreqinfo->udeadline)) {
    mwlog (MWLOG_WARNING, "Got a service request that had already expired by %d.%3.3d seconds.", 
	   starttv.tv_sec - svcreqinfo->deadline,  
	   (starttv.tv_usec - svcreqinfo->udeadline)/1000);
    mwlog(MWLOG_DEBUG, "issued %d.%d timeout %d now %d.%d deadline %d.%d",
	  callmesg->issued, callmesg->uissued, callmesg->timeout,
	  starttv.tv_sec ,  starttv.tv_usec, 
	  svcreqinfo->deadline, svcreqinfo->udeadline);
    callmesg->returncode = -ETIME;
    _mw_ipc_putmessage(callmesg->cltid,callmesg, sizeof(Call), 0);
    _mw_set_my_status(NULL);
    errno = ETIME;
    free(svcreqinfo);
    return NULL;
  };

  /* transfer data from ipc message to mwsvcinfo struct. */  
  svcreqinfo->cltid = callmesg->cltid;
  svcreqinfo->srvid = callmesg->srvid;
  svcreqinfo->svcid = callmesg->svcid;
  /**************** handle */
  svcreqinfo->flags = callmesg->flags;
  memset(svcreqinfo->service, 0, MWMAXSVCNAME);
  strncpy(svcreqinfo->service, callmesg->service, MWMAXSVCNAME);

  /* transfer of the data buffer, unless in fastpath where we recalc the pointer. */
  if (_mw_fastpath) {
    svcreqinfo->data = _mwoffset2adr(callmesg->data);
    svcreqinfo->datalen = callmesg->datalen;
  } else {
    char * ptr;
    svcreqinfo->data = malloc(callmesg->datalen+1);
    ptr = _mwoffset2adr(callmesg->data);
    memcpy(svcreqinfo->data, ptr, callmesg->datalen);
    svcreqinfo->data[callmesg->datalen+1] = '\0';
    svcreqinfo->datalen = callmesg->datalen;
    _mwfree(ptr);
  };
    
  
  return svcreqinfo;
};

/* THis is only used in applications using C for implemeting service routines.
*/
int _mwCallCServiceFunction(mwsvcinfo * svcinfo)
{
  int rc, mesglen;
  ipcmaininfo * ipcmain;
  serviceentry * svcent;
  struct ServiceFuncEntry * serviceptr;

  mwlog(MWLOG_DEBUG, "calling C service routine for %s(%d) (it had waited %d millisecs)", 
	callmesg->service, callmesg->svcid, waitmsec);
  
  if (serviceFuncList == NULL) {
    rc  = -ENOENT;
  } else {
    serviceptr = serviceFuncList;
    while (serviceptr != NULL) {
      if (serviceptr->svcid == svcinfo->svcid) {
	rc = serviceptr->func(svcinfo);
	break;
      }
      serviceptr = serviceptr->next;
    };
  };

  /* it really is the mwreply() responsibility to figure the service time. But we must 
     check here just in case, mwreply was not called.
     Busy time is calculated by _mw_set_my_status()
  */
  if (_mw_requestpending) {
    completeStats();
  };

  /* we should now check to see if the handler has replied but 
     That requier proper queing mechanisims in _mw_put/get_ipc_message()
     I think that the caller should test regurarly if the called server
     if up and running. Maybe we should add clientid and handle to status.
  */
  mwlog(MWLOG_DEBUG, "call on service %s(%d) returned %d", 
	callmesg->service, callmesg->svcid, rc);
  
  return rc;  
};

int mwservicerequest(int flags)
{
  mwsvcinfo * svcinfo;

  int rc;
  
  rc = _mwsystemstate();
  if (rc) return rc;

  svcinfo = _mwGetServiceRequest (flags);

  /* it will return on failure or interrupt.
     if noblovking return, with errno, else do a recurive call until success.
  */
  mwlog(MWLOG_DEBUG, "mwGetServiceRequest(%#x) returned %#X with errno = %d", 
	flags, svcinfo, errno);
  
  /*  if (svcinfo == NULL && (flags & MWNOBLOCK)) return -errno; */
  if (svcinfo == NULL) return -errno;

  /* mwservicerequest() are called on C service handler only */
  return _mwCallCServiceFunction(svcinfo);
};
/*
  Normal application servers shall call MailLoop() after
  initalization.  and do clean up after it terminates.  In tuxedo
  main() if locked away from the programmer, and the init and clean up
  are functions. we adopt a more common approact used much in X11
  toolkits. The reason is that the tuxedo approach are more difficult
  to make cooperate with other API that imply access to main().
  Imagine that a server should have the possibility to open a X11
  window to show info. This can with pthreads be easily implemented.
  If a programmer choose to provide their own mwMailLoop it can even
  be done without pthreads.  
*/

int mwMainLoop(int flags)
{
  int rc, counter = 0;
  
  while(1) {
    counter++;
    mwlog(MWLOG_DEBUG, "std MailLoop begining for the %d time.", counter);
    rc = mwservicerequest(flags & ! MWNOBLOCK);
    if ( (rc == -EINTR) && (flags & MWSIGRST)) continue;
    if (rc < 0) return rc;
  };
};
  


  
