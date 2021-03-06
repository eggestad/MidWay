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

#include <errno.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include <MidWay.h>
#include <ipcmessages.h>
#include <ipctables.h>
#include <shmalloc.h>
#include <mwclientapi.h>
#include <mwclientipcapi.h>
#include <mwserverapi.h>
#include <tasks.h>

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

static int provided = 0;

Call * _mw_server_get_callbuffer(void)
{
  return callmesg;
};

void _mw_server_set_callbuffer(Call * cb)
{
  if (cb == NULL) 
    callmesg = (Call *) buffer;
  else 
    callmesg = cb;
};

void _mw_incprovided(void)
{
  provided++;
};
void _mw_decprovided(void)
{
  provided--;
};

/*
  We have an internal list for holding the pionter ro the functioon to
  be called for each service. */

static struct ServiceFuncEntry * serviceFuncList = NULL;

/* two methods for poping and pushing functions references to C
   functions coupled with SERICEID.  */

struct ServiceFuncEntry * _mw_pushservice(SERVICEID sid, int (*svcfunc) (mwsvcinfo*)) 
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

void _mw_popservice(SERVICEID sid)
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
static void install_sigactions(int flag);

static void signal_handler(int sig) 
{
  static int sigloop = 0;

  /*  install_sigactions(1);*/
  
  Info("Going down on signal %d", sig);
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
   struct sigaction sa = { 0 };
  
  if (flag)
    sa.sa_handler = SIG_DFL;
  else 
    sa.sa_handler = signal_handler;
  sigfillset(&sa.sa_mask);

  sigaction(SIGHUP, &sa, NULL);
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);
  sigaction(SIGQUIT, &sa, NULL);
  /*  sigaction(SIGSEGV, &sa, NULL);*/
  /*  sigaction(SIGFPE, &sa, NULL);*/
  /* sigaction(SIGILL, &sa, NULL);*/
};


/*********************************************************************
 *
 *  API begins here 
 *
 ********************************************************************/
  
/*
  provide and unprovide sends messages to mwd about what service calls
  this server provides.  */

int mwprovide(const char * service, int (*svcfunc) (mwsvcinfo*), int flags)
{
  SERVICEID svcid;
  serviceentry * svcent;
  int rc;
  
  rc = _mwsystemstate();
  if (rc) return rc;

  install_sigactions(0);

  DEBUG1("mwprovide() providing %s to mwd", 
	service);
  svcid = _mw_ipc_provide(service,flags);
    
  if (svcid < 0) return svcid;
  /* THREAD MUTEX BEGIN */
  svcent = _mw_get_service_byid(svcid);
  if (svcent == NULL) {
    Error("This can't happen, possible error in mwd, got an OK on a provide request but returned service id  (%#x) does not exist ", svcid);
    return -1;
  };

  /* instead of an index internally here, by storing tha addres of the
     ServiceFuncEntry object in the service entry in the SHM
     Tables. It must be searched anyway by the client or gateway
     process.  Likewise we can from say Perl store a reference here.  */
  svcent->svcfunc = (void *) _mw_pushservice (svcid, svcfunc);
  provided++;

  /* THREAD MUTEX ENDS */
  return svcid;
};

int mwunprovide(const char * service)
{
  SERVICEID svcid;
  int rc;

  if (!_mw_isattached()) return -ENOTCONN;
  /* short cut if now services are provided.*/;
  if (!provided) return -ENOENT;
  /* We must lookup service type somewhere TODO:: FIXME: LOOKATME */
  svcid = _mw_get_service_byname(service, 0);
  if (svcid < 0 ) {
    Error("mwunprovide() failed with rc=%d",svcid);
    return svcid;
  };

  rc = _mw_ipc_unprovide(service,  svcid);
  _mw_popservice(svcid);
  provided--;
  DEBUG1("mwunprovide() returned %d", rc);
  return rc;
}

/* These are used to calculate statistics between mwservicerequest and mwreply.
   NB: NOT TREADSAFE. If we make us thread safe these are thread private data.
*/
static struct timeval starttv, endtv; 
static int waitmsec, servemsec;
int _mw_requestpending = 0;
static void completeStats(void)
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
int mwreply(const char * data, size_t len, int returncode, int appreturncode, int flags)
{
  int rc;
  int mwid;
  int ipcflags = 0; 

  /* if we already have completed the replies */
  if (! _mw_requestpending) return -EALREADY;

  DEBUG1("mwreply (data =%p, len=%zu rc=%d apprc=%d flags=%#x", 
	 data, len,  returncode, appreturncode, flags);

  rc = _mwsystemstate();
  if (rc) return rc;

  // if there is not available buffers,  we try for a while
  do {
     int n; 
     n = 0;
     rc = _mw_putbuffer_to_call(callmesg, data, len);
     
     if (rc) {
	if (n >= 100) return rc;
	if (n == 0) {
	   Warning("Reply: failed to place reply buffer rc=%d", rc);
	};
	usleep(20);
	n++;
     }
  } while (rc != 0);

  /* return code handling. returncode is EFAULT if MWFAIL, 
     0 on MWSUCCESS and MWMORE on MWMORE. */
  if (returncode == MWSUCCESS)
    callmesg->returncode = MWSUCCESS;
  else if (returncode == MWMORE)
    callmesg->returncode = MWMORE;
  else
    callmesg->returncode = -EFAULT;

  callmesg->appreturncode = appreturncode;

  /* set "opcode" on message */
  callmesg->mtype = SVCREPLY;
  callmesg->srvid = _mw_get_my_serverid();

  /* flags translation, INCOMPLETE */
  if (flags & MWNOBLOCK) ipcflags |= IPC_NOWAIT;

  /* send reply buffer, if gateway id is set, use that, or else used clientid.*/
  if (callmesg->gwid == UNASSIGNED) mwid  = callmesg->cltid;
  else mwid  = callmesg->gwid;
  rc = _mw_ipc_putmessage(mwid, (char *) callmesg, sizeof(Call), ipcflags);
  if (rc != 0) {
    Warning("Failure on replying to client %#x reason %d", 
	  callmesg->cltid, errno);
    return -errno;
  } 
  DEBUG1("mwreply sent to client %d rc= %d", 
	callmesg->cltid, callmesg->returncode);

  /* if the more flag is set, and multiple flag in the callmesg is
     set, do not clear pending flag.

     We only update stats on service call completion.
  */
  if (callmesg->returncode != MWMORE) {
    _mw_requestpending = 0;  
    gettimeofday(&endtv, NULL);
    completeStats();
  };
  return rc;
};

/*
  mwforward() are used by an application service routines to send the request
  onto another server, substitution the data buffer with a new. 
  The next service will either do anothre forward, or send the response 
  directly to the client. This and the services involvement, just like mwreply.
*/
int mwforward(const char * svcname, const char * data, size_t len, int flags)
{
  int rc, dataoffset, dest;
  int ipcflags = 0;
  void * dbuf;
  int maxforwards = 100;
  SERVICEID svcid;

  rc = _mwsystemstate();
  if (rc) return rc;

  /* we should provite a way to set the "TTL" */
  if (callmesg->forwardcount > maxforwards) {
    Warning("Tried to forward a request from %#x that already had been forwarded %d times.", callmesg->cltid, maxforwards);
    return -ELOOP;
  };
  callmesg->forwardcount++;

  /* find the svcid of the service we are to call. */
  svcid = _mw_get_service_byname(svcname,flags&MWCONV);
  if (svcid < 0) {
    Warning("mwforward() getting a serviceid for service %s failed reason %d",
	  svcname,svcid);
    return svcid;
  };

  dest = _mw_get_provider_by_serviceid (svcid);
  if (dest < 0) {
    Warning("mwforward(): getting the serverid for serviceid %d(%s) failed reason %d",
	  svcid, svcname, dest);
    return dest;
  };

  // update call struct so that next server know that service we're calling
  callmesg->svcid = svcid;
  strncpy(callmesg->service, svcname, MWMAXSVCNAME);

  /* First we handle return buffer. If data is not NULL, and len is 0,
   data is NULL terminated. if buffer is not a shared memory buffer,
   get one and copy over. */

  if (data != NULL) {
    if (len == 0) len = strlen(data);

    dataoffset = _mwshmcheck((void *)data);
    if (dataoffset == -1) {
      dbuf = _mwalloc(len);
      if (dbuf == NULL) {
	Error("Forward: mwalloc(%zu) failed reason %d", len, (int) errno);
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

  /* flags translation, INCOMPLETE */
  if (flags & MWNOBLOCK) ipcflags |= IPC_NOWAIT;

  rc = _mw_ipc_putmessage(dest, (char *) callmesg, sizeof(Call), ipcflags);
  if (rc != 0) {
    Warning("Failure on replying to client %#x reason %d", 
	  callmesg->cltid, errno);
    return -errno;
  } 
  DEBUG1("mwreply sent to client %d rc= %d", 
	callmesg->cltid, callmesg->returncode);

  _mw_requestpending = 0;
  
  gettimeofday(&endtv, NULL);
  completeStats();
  return rc;
};


int _mw_set_deadline(Call * callmesg, mwsvcinfo * svcreqinfo)
{
  int remain_ms;
  /* now we do calulations on issue time, now, and deadlines.
     we need to update stats (NYI) and to check to see if the deadline has expired, 
     and returne a timeout reply if it has.
  */
  gettimeofday(&starttv, NULL);
  if (callmesg->timeout > 0 ) {
    svcreqinfo->deadline  = callmesg->issued + callmesg->timeout/1000;
    svcreqinfo->udeadline = callmesg->uissued + ((callmesg->timeout)%1000)*1000000;
  } else {
    svcreqinfo->deadline = 0;
    svcreqinfo->udeadline = 0;
  };
  
  waitmsec = (starttv.tv_sec - callmesg->issued) * 1000 
    + (starttv.tv_usec - callmesg->uissued) / 1000;

  /* we return time left to deadline(, or expired by if negative) */

  if (callmesg->timeout > 0) {    
    remain_ms  = (svcreqinfo->deadline - starttv.tv_sec);
    if (remain_ms > 1000000) remain_ms = 1000000;
    remain_ms *= 1000;
    remain_ms += (svcreqinfo->udeadline - starttv.tv_usec) / 1000; 
    return remain_ms;
  }

  /* if no deadline, we can't calc whats remains */
  return 1000000000;
};

/*
 * This function is used to perform a service call. Thus it finds out if 
 * we really provides this service, sets stats data in shm and calls
 * the applictaion service function.
 * 
 * if your're in need of replacing it since you use tha message queue
 * for other messages, see do_call() in mwd/request_parse.c
 */
mwsvcinfo *  _mwGetServiceRequest (int flags)
{
  int rc;
  size_t mesglen;
  ipcmaininfo * ipcmain;
  serviceentry * svcent;
  cliententry * cltent;
  mwsvcinfo * svcreqinfo;
  long * mtype;
  MWID mwid;

  if (!_mw_isattached()) { errno = ENOTCONN; return NULL;}
  if (!provided) { errno = ENOENT; return NULL;};

  mesglen = MWMSGMAX;
  ipcmain = _mw_ipcmaininfo();
  if (ipcmain == NULL) {errno = EADDRNOTAVAIL; return NULL;}

  _mw_set_my_status(NULL);
  mtype = (long *) buffer;

  do {
    rc = _mw_ipc_getmessage(buffer, &mesglen, 0, flags);
    
    if (rc < 0) break;    
    if (*mtype == SVCCALL) break;
    if (*mtype == SVCREPLY) {
       Warning("got a call reply while waiting for a service call request");
       _mw_ipc_pushCallReply((Call *) buffer);
       errno = EINPROGRESS;
       return NULL;
    }
    if (*mtype == EVENT) {
      _mw_do_ipcevent((Event *) buffer);
      continue;
    };
    Error("Ignoring event of type %#lx", *mtype);
  } while(1);
      
  if (rc < 0) {
    /* mwd will notify us about shutdown by removing our message queue.*/
    if ( (errno == EIDRM) || (errno == EINVAL) ) {
      Info("Administrative Shutdown.");
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
  if (!(callmesg->flags & MWNOREPLY) )
    _mw_requestpending = 1;

  
  DEBUG1(	"mwfetch: got a CALL/FORWARD to service \"%s\" id %#x from clt:%#x gw:%#x srv:%#x", 
	callmesg->service, callmesg->svcid, 
	callmesg->cltid, callmesg->gwid, callmesg->srvid);
    
  /* timeout info */
  svcent = _mw_get_service_byid(callmesg->svcid);
  if (svcent == NULL) {
    Error("Can't happen, Got a call to %s (%d) but it does not exist in BBL", 
	  callmesg->service, callmesg->svcid);
    callmesg->returncode = -ENOENT;

    if (callmesg->gwid == UNASSIGNED) mwid  = callmesg->cltid;
    else mwid  = callmesg->gwid;
    _mw_ipc_putmessage(mwid, (char*) callmesg, sizeof(Call), 0);
    _mw_requestpending = 0;  
    _mw_set_my_status("(ERROR)");
    errno = ENOENT;
    return NULL;
  };

  /*    if (callmesg->srvid != _mw_get_my_serverid()) {
	Error("Got a call request for service \"%s\" serviceid %#x and serverid %#x, but my serverid is %#x", 
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

  rc = _mw_set_deadline(callmesg, svcreqinfo);
  if (rc < 0) {
    Warning("Got a service request that had already expired by "
	   "%d milliseconds, replying ETIME", 
	   -rc);
    
    DEBUG1("issued %ld.%d timeout %d now %ld.%ld deadline %ld.%d",
	  callmesg->issued, callmesg->uissued, callmesg->timeout,
	   starttv.tv_sec ,  (long) starttv.tv_usec, 
	  svcreqinfo->deadline, svcreqinfo->udeadline);
    callmesg->returncode = -ETIME;
    _mw_ipc_putmessage(callmesg->cltid, (char *) callmesg, sizeof(Call), 0);
    _mw_set_my_status(NULL);
    errno = ETIME;
    free(svcreqinfo);
    return NULL;
  };

  /* in this case the client sent junk, reply error, and return next 

     TODO: this is a DoS bug, send enough junk messages, and a server crashed due to stack overrun

   */ 
  rc = _mw_getbuffer_from_call_to_svcinfo(svcreqinfo, callmesg);
  if (rc != 0) {
    mwreply(NULL, 0, -1, 0, 0);
    free(svcreqinfo); 
    return _mwGetServiceRequest (flags);
  };

  /* transfer data from ipc message to mwsvcinfo struct. */  
  svcreqinfo->cltid = callmesg->cltid;
  svcreqinfo->gwid = callmesg->gwid;
  svcreqinfo->srvid = callmesg->srvid;
  svcreqinfo->svcid = callmesg->svcid;
  /**************** handle */
  svcreqinfo->flags = callmesg->flags;
  memset(svcreqinfo->service, 0, MWMAXSVCNAME);
  strncpy(svcreqinfo->service, callmesg->service, MWMAXSVCNAME);

  memset(svcreqinfo->username, 0, MWMAXSVCNAME);
  memset(svcreqinfo->clientname, 0, MWMAXSVCNAME);
  svcreqinfo->authentication = UNASSIGNED;
  
  /* get autentication info from client table */
  if (callmesg->cltid != UNASSIGNED) {
     cltent = _mw_get_client_byid(callmesg->cltid);
     if (cltent) {
	strncpy(svcreqinfo->username, cltent->username, MWMAXSVCNAME);
	strncpy(svcreqinfo->clientname, cltent->clientname, MWMAXSVCNAME);
	svcreqinfo->authentication = cltent->authtype;
     };
  };

  return svcreqinfo;
};


/* This is only used in applications using C for implemeting service routines.
*/
int _mwCallCServiceFunction(mwsvcinfo * svcinfo)
{
  int rc;
  struct ServiceFuncEntry * serviceptr;

  DEBUG1("calling C service routine for %s(%d) (it had waited %d millisecs)", 
	callmesg->service, callmesg->svcid, waitmsec);
  
  if (serviceFuncList == NULL) {
    rc  = -ENOENT;
  } else {
    rc = -EBADRQC;
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
    mwreply(NULL, 0, rc, 0, 0);
    completeStats();
  };

  /* we should now check to see if the handler has replied but 
     That requier proper queing mechanisims in _mw_put/get_ipc_message()
     I think that the caller should test regurarly if the called server
     if up and running. Maybe we should add clientid and handle to status.
  */
  DEBUG1("call on service %s(%d) returned %d", 
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
  DEBUG1("mwGetServiceRequest(%#x) returned %p with errno = %d", 
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
    DEBUG1("std MailLoop begining for the %d time.", counter);
    rc = mwservicerequest(flags & ! MWNOBLOCK);
    if (rc == -EINTR) {
       DEBUG1("interrupted due to interrupt");
       if (_mw_tasksignalled()) {
	  DEBUG1("doing tasks");
	  mwdotasks();
	  continue;
       };
       if (flags & MWSIGRST) continue;
    };

    if (rc < 0) return rc;
  };
};
  


  
