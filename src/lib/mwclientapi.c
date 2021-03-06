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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include <sys/time.h>
#include <math.h>

// needed for the gethostbyname and inet_ntop in mwgeturl
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <MidWay.h>
#include <address.h>
#include <utils.h>
#include <ipcmessages.h>
#include <ipctables.h>
#include <shmalloc.h>
#include <mwclientipcapi.h>
#include <SRBclient.h>

// all the not conncected and not implemeted versions of the client calls:


int _mw_notimp_attach (int type, mwaddress_t * mwadr, const char * cname, mwcred_t * cred, int flags)
{
   return -ENOSYS;
};
int _mw_notimp_detach(void)
{
   return -ENOSYS;
};

int _mw_notimp_acall (const char * svcname, const char * data, size_t datalen, int flags)
{
   return -ENOSYS;
};
int _mw_notimp_fetch (int *hdl, char ** data, size_t * len, int * appreturncode, int flags)
{
   return -ENOSYS;
};
int _mw_notimp_listsvc (const char *glob, char *** list, int flags)
{
   return -ENOSYS;
};
   
int _mw_notimp_event (const char * evname, const char * data, size_t datalen, const char * username, const char * clientname, 
		 MWID fromid, int remoteflag)
{
   return -ENOSYS;
};
void _mw_notimp_recvevents(void)
{
   return;
};
int _mw_notimp_subscribe (const char * pattern, int id, int flags)
{
   return -ENOSYS;
};
int _mw_notimp_unsubscribe (int id)
{
   return -ENOSYS;
};

int _mw_notconn_attach (int type, mwaddress_t * mwadr, const char * cname, mwcred_t * cred, int flags)
{
   return -ENOTCONN;
};
int _mw_notconn_detach (void)
{
   return -ENOTCONN;   
};
int _mw_notconn_acall (const char * svcname, const char * data, size_t datalen, int flags)
{
   return -ENOTCONN;
};
int _mw_notconn_fetch (int *hdl, char ** data, size_t * len, int * appreturncode, int flags)
{
   return -ENOTCONN;
};
int _mw_notconn_listsvc (const char *glob, char *** list, int flags)
{
   return -ENOTCONN;
};

int _mw_notconn_event (const char * evname, const char * data, size_t datalen, const char * username, const char * clientname, 
		 MWID fromid, int remoteflag)
{
   return -ENOTCONN;
};
void _mw_notconn_recvevents(void)
{
   return;
};
int _mw_notconn_subscribe (const char * pattern, int id, int flags)
{
   return -ENOTCONN;
};
int _mw_notconn_unsubscribe (int id)
{
   return -ENOTCONN;   
};



static struct timeval deadline = {0, 0};

static mwaddress_t _mwaddress = {
   .protocol = MWNOTCONN,
   .proto.attach      = _mw_notconn_attach,
   .proto.detach      = _mw_notconn_detach,
   .proto.acall       = _mw_notconn_acall,
   .proto.fetch       = _mw_notconn_fetch,
   .proto.listsvc     = _mw_notconn_listsvc,
   .proto.event       = _mw_notconn_event,
   .proto.recvevents  = _mw_notconn_recvevents,
   .proto.subscribe   = _mw_notconn_subscribe,
   .proto.unsubscribe = _mw_notconn_unsubscribe,
   .domain            = NULL };
   
void _mw_clear_mwaddress(void)
{
   _mwaddress.protocol = MWNOTCONN;
   _mwaddress.proto.attach      = _mw_notconn_attach;
   _mwaddress.proto.detach      = _mw_notconn_detach;
   _mwaddress.proto.acall       = _mw_notconn_acall;
   _mwaddress.proto.fetch       = _mw_notconn_fetch;
   _mwaddress.proto.listsvc     = _mw_notconn_listsvc;
   _mwaddress.proto.event       = _mw_notconn_event;
   _mwaddress.proto.recvevents  = _mw_notconn_recvevents;
   _mwaddress.proto.subscribe   = _mw_notconn_subscribe;
   _mwaddress.proto.unsubscribe = _mw_notconn_unsubscribe;
   _mwaddress.domain            = NULL;
};

mwaddress_t * _mw_get_mwaddress(void)
{
   return &_mwaddress;
};

#define URLMAXLEN 256
char * mwgeturl()
{
   static char url[URLMAXLEN];
   mwaddress_t * mwadr;
   struct hostent * hent;
   char ipadr[256];

   mwadr = _mw_get_mwaddress();
   switch(mwadr->protocol) {
   case MWNOTCONN:
      return NULL;

   case MWSYSVIPC:

      if (mwadr->instancename != NULL)
	 snprintf(url, URLMAXLEN, "ipc:/%s", mwadr->instancename);
      else 
	 snprintf(url, URLMAXLEN, "ipc:%u", mwadr->sysvipckey);
      return url;
      
   case MWSRBP:      
      hent = gethostbyaddr( (char *) mwadr->ipaddress.sin4, sizeof(struct sockaddr_in), AF_INET);
      DEBUG1("hent = %p", hent); 
      ipadr[255] = '\0';
      ipadr[0] = '\0';
      if (hent && hent->h_name) {
	 strncpy(ipadr, hent->h_name, 255);
      } else {
	 inet_ntop(mwadr->ipaddress.sa->sa_family, &mwadr->ipaddress.sin4->sin_addr, ipadr, 255);
      };
      snprintf(url, URLMAXLEN, "srbp://%s/%s", ipadr, mwadr->domain);
      return url;      
   }
   return NULL;
};

static mwcred_t credentials = { 0 };

/************************************************************************/

int _mw_deadline(struct timeval * tv_deadline, float * ms_deadlineleft)
{
  struct timeval now;
  float timeleft;

  if (deadline.tv_sec == 0) return 0;
  gettimeofday(&now, NULL);

  if (ms_deadlineleft != NULL) {
    timeleft = now.tv_sec;
    timeleft -= deadline.tv_sec;
    timeleft += now.tv_usec / 1000000;
    timeleft -= deadline.tv_usec / 1000000;
    *ms_deadlineleft = timeleft;
  };
  
  if (tv_deadline != NULL) {
    tv_deadline->tv_sec = deadline.tv_sec;
    tv_deadline->tv_usec = deadline.tv_usec; 
  };
  
  return 0 ;
};

int _mw_isattached(void)
{   
  if (_mwaddress.protocol == MWNOTCONN) return 0;
  return 1;
};


static mwhandle_t handle = -1;
DECLAREMUTEX(callhandle);
/**
   Get a call handle unique to this process. 
   
   Contraint: 1 <= handle <= MAX_INT32 (2^31). 
   It is inc'ed everytime we need a new, wrapped to 1, and randomly
   assigned the first time.
*/
mwhandle_t _mw_nexthandle(void)
{
  LOCKMUTEX(callhandle);

  /* test for overflow (or init) */
  if (handle == -1) {
     handle = _mw_irand(INT_MAX/2);
  } 

  handle++;
  if (handle < 0) {
     handle = 1;
  };

  UNLOCKMUTEX(callhandle);
  return handle;
};

// we set the credentials by a seperate func call, the arguments after username depends on the auth type
int mwsetcred(int authtype, const char * username, ...)
{
   int rc = 0;
   va_list ap;

   va_start(ap, username);

   switch(authtype) {
      
   case MWAUTH_NONE:
      credentials.authtype = authtype;
      credentials.username = NULL;
      credentials.cred.data = NULL;
      break;
   
   case MWAUTH_PASSWORD:
      credentials.authtype = authtype;
      credentials.username = strdup(username);
      credentials.cred.password = strdup(va_arg(ap, char *));
      break;

   case MWAUTH_X509:
      rc = -ENOSYS;
      break;

   case MWAUTH_KRB5:
      rc = -ENOSYS;
      break;

   default:
      rc = -EINVAL;
      break;
   };
   
   va_end(ap);
   return rc;
};


/* should it take a struct as arg??, may as well not, even in a struct, all
   members must be init'ed.*/
int mwattach(const char * url, const char * name, int flags)
{
  FILE * proc;
  int type;
  char buffer[256];
  int rc, retval;

  /* if we're already connected .... */
  if (_mwaddress.protocol != MWNOTCONN) 
    return -EISCONN;;

  rc = _mwsystemstate();
  DEBUG3("_mwsystemstate returned %d", rc);
  if (rc == 0) {
     retval = -EISCONN;;
     goto out;
  };

  if (rc != -ENAVAIL) {
     retval = rc;
     goto out;
  };

  errno = 0;
  rc = _mwdecode_url(url, &_mwaddress);
  if (rc != 0) {
     DEBUG1("_mwdecode_url(%s, ...) returned %d", url, rc);
    if (errno == ETIME) {
      retval = -EHOSTDOWN;
      goto out;
    };
    Error("The URL %s is invalid, decode returned %d", url, errno);
    retval = -EINVAL;
    goto out;
  };

  if (name == NULL) {
    /* if on linux this works, else open fails 
     * /proc/self/cmdline gives each cmdline param, \0 terminated.
     */
    proc = fopen("/proc/self/cmdline", "r");
    if (proc == NULL) name = "Anonymous";
    else {
      fgets(buffer, 255, proc);
      name = buffer;
    };
  };
    
  if ( (flags & MWSERVER) && (_mwaddress.protocol != MWSYSVIPC) ) {
     Error("Attempting to attach as a server on a protocol other than IPC.");
     retval = -EINVAL;
     goto out;
  };

#ifdef SYSVIPCPROTO
  /* protocol is IPC */
  if (_mwaddress.protocol == MWSYSVIPC) {

     _mwipcprotosetup(&_mwaddress);

    type = 0;
    if (flags & MWSERVER)       type |= MWIPCSERVER;
    if (!(flags & MWNOTCLIENT)) type |= MWIPCCLIENT;
    if ((type < 1) || (type > 3)) {
       retval = -EINVAL;
       goto out;
    };

    DEBUG("attaching to IPC name=%s, IPCKEY=0x%x type=%#x", 
	  name, _mwaddress.sysvipckey, type);
    rc = _mwaddress.proto.attach(type, &_mwaddress, name, NULL, 0);
    if (rc != 0) {
      DEBUG("attaching to IPC failed with rc=%d",rc);
      _mw_clear_mwaddress();
    };
    retval = rc;
    goto out;
  }
#endif

#ifdef SRBPROTO
  /* protocol is SRB */
  if (_mwaddress.protocol == MWSRBP) {
     
     _mwsrbprotosetup(&_mwaddress);
     retval = _mwaddress.proto.attach(MWCLIENT, &_mwaddress, name, &credentials, flags);
     goto out;
  };
#endif

  out:
  
  if (retval < 0)  _mw_clear_mwaddress();
  return retval;


  return -EINVAL;
};

int mwdetach()
{
   int rc;

   DEBUG1("mwdetach: %s", _mwaddress.protocol?"detaching":"not attached");

   rc = _mwaddress.proto.detach();

   _mw_clear_mwaddress();
   return rc;
};

int mwlistsvc(char * glob, char *** list, int flags)
{
  return _mwaddress.proto.listsvc (glob, list, flags);
};

int mwfetch(int * handle, char ** data, size_t * len, int * appreturncode, int flags) 
{
  int rc;

  TIMEPEGNOTE("begining fetch");
  if ( (data == NULL) || (len == NULL) || (handle == NULL) || (appreturncode == NULL) )
    return -EINVAL;

  DEBUG1("mwfetch called handle = %d", *handle);

  if (_mwaddress.protocol == MWNOTCONN) {
    DEBUG1("not attached");
    return -ENOTCONN;
  };

  /* if client didn't set MWMULTIPLE, the client is expecting one and
     only one reply, we must concat multiiple replies */
  if (! (flags & MWMULTIPLE)) {
    char * pdata = NULL;
    size_t pdatalen = 0;
    int first = 1;
 
    DEBUG1("No MWMULTIPLE flags set, collection all replies");

    * len = 0;

    do {
       rc = _mwaddress.proto.fetch(handle, &pdata, &pdatalen, appreturncode, flags);

       DEBUG1("protocol level fetch returned %d datalen = %zu", rc, pdatalen);

      /* if error */
      if (rc == MWFAIL) {
	return rc;
      };

      /* shortcut, iff one reply there is no copy needed. */
      if ( (rc != MWMORE) && (first) ) {
	*data = pdata;
	*len = pdatalen;	
	TIMEPEGNOTE("returning short fetch");
	timepeg_log();
	return rc;
      };
      
      first = 0;
      
      *data = realloc(*data, *len + pdatalen + 1);
      DEBUG1("Adding reply: *data = %p appending at %p old len = %zu new len = %zu",
	    *data, (*data)+(*len), *len, pdatalen);
      memcpy((*data)+(*len), pdata, pdatalen);
      *len += pdatalen;
      
    }  while (rc == MWMORE) ;

    (*data)[*len]  = '\0';
    TIMEPEGNOTE("returning long fetch");
    timepeg_log();
    return rc;
  };

  DEBUG1("MWMULTIPLE flags set");      

  /* multiple replies handeled in user code, we return the first reply */
  rc = _mwaddress.proto.fetch(handle, data, len, appreturncode, flags);

  TIMEPEGNOTE("returning fetch");
  timepeg_log();
  return rc;
};

int mwacall(const char * svcname, const char * data, size_t datalen, int flags) 
{
  int handle, rc = -EFAULT;
  float timeleft;
  TIMEPEGNOTE("begining acall");

  /* input sanyty checking, everywhere else we depend on params to be sane. */
  if ( svcname == NULL ) {
     rc = -EINVAL;
     goto out;
  };
  
  DEBUG1("mwacall called for service %.32s", svcname);

  if ( _mw_deadline(NULL, &timeleft)) {
    /* we should maybe say that a few ms before a deadline, we call it expired? */
    if (timeleft <= 0.0) {
      /* we've already timed out */
      DEBUG1("call to %s was made %f ms after deadline had expired", 
	     svcname, timeleft);
      rc = -ETIME;
      goto out;
    };
  };

  /* datalen may be zero if data is null terminated */
  if ( (data != NULL ) && (datalen == 0) ) datalen = strlen(data);

  handle = _mw_nexthandle();
  rc =  _mwaddress.proto.acall(svcname, data, datalen, flags);
  TIMEPEGNOTE("end acall");
  timepeg_log();
  
 out:
  DEBUG1("returning %d", rc);
  return rc;
};

int mwcall(const char * svcname, 
	       const char * cdata, size_t clen, 
	       char ** rdata, size_t * rlen, 
	       int * appreturncode, int flags)
{
  int hdl;
  /* input sanyty checking, everywhere else we depend on params to be sane. */
  if ( svcname == NULL ) 
    return -EINVAL;
  if  ( !(flags & MWNOREPLY) && ((rlen == NULL) || (rdata == NULL)))
    return -EINVAL; 
  if  ( (flags & MWMULTIPLE) )
    return -EINVAL; 

  DEBUG1("mwcall called for service %.32s", svcname);

  hdl = mwacall(svcname, cdata, clen, flags);
  DEBUG1("mwcall(): mwacall() returned handle %d", hdl);

  if (hdl < 0) return hdl;
  return mwfetch (&hdl, rdata, rlen, appreturncode, flags);

};

/************************************************************************/

/* events. OK quite a bit of how this works. THe primary way to
   subscribe is by mwsucscribeCB() where you give a callback
   function. Later we'll add a mwsubscribe() that has its own default
   function that just queues the events and you'll fetch them from the
   queue with someinglike mwgetnextevent().

   Now the mwsubscribe*() calls will record the subscription here,
   while also assign a subscription id. The subid is later used for
   spesific unsubscribes.  The subscription is passed to mwd who is
   the one that distributes events. 

   In the IPC events model the same shm data buffer (if there is one)
   is passed to all recipients. In order to free the shm all clients
   must ack the event. Thus we break from service call convention,
   where the ownership of the shm buffer is passed along the call. The
   mwd once gotten ownership of the buffer always retain it.  One all
   the recipients has acked the event, mwd can free it. If there is no
   data buffer with the event, no ack is needed. IN the SRB version
   the event buffer is passed on socket and all buffers are local.

   So when we call the mwevent() an event message is sent to the mwd,
   with a buffer if needed.

   The problems crop up with the reception of event. Servers are
   normally in blocking wait, and will process either, whatever comes
   first. SRB clients can trigger on signals on the socket. But IPC
   clients will only handle events when in call/fetch or select other
   midway calls. IPC clients *should* regulary call mwrecvevents() to
   process them.
   
*/

struct subscribed_events {
  char * pattern;
  int subscriptionid;
  void (*callback)(const char * , const char *, size_t);
  int flags;
};

typedef  struct subscribed_events  subscribed_events_t;

static subscribed_events_t * subscriptions = NULL;
static int subscription_count = 0;
static int next_subscriptionid  = 0;

DECLAREMUTEX(eventmutex);

static int get_subscriptionid(void)
{
  int id;
  LOCKMUTEX(eventmutex);
  id = next_subscriptionid++;
  UNLOCKMUTEX(eventmutex);
  return id;
};

int _mwsubscribe(const char * pattern, int subid, int flags)
{
   return _mwaddress.proto.subscribe(pattern, subid, flags);
};
  

int mwsubscribeCB(const char * pattern, int flags, void (*func)(const char * eventname, const char * data, size_t datalen))
{
  subscribed_events_t * se;
  int error = 0;
  int rc;

  DEBUG1("SUBSCRIBE %p %x", pattern, flags);
  if (_mwaddress.protocol == MWNOTCONN) return -ENOTCONN;
  if (pattern == NULL) return -EINVAL;
  if (func == NULL) return -EINVAL;
  

  LOCKMUTEX(eventmutex);
  subscriptions = realloc(subscriptions, sizeof(subscribed_events_t) * (subscription_count+1));
  se = &subscriptions[subscription_count++];
  UNLOCKMUTEX(eventmutex);

  se->pattern = strdup(pattern);
  se->flags = flags;

  se->callback = func;
  se->subscriptionid = get_subscriptionid();
  
  rc =  _mwsubscribe(pattern, se->subscriptionid, flags);
  
  if (rc >= 0) {
    DEBUG1("subscription OK");
    return se->subscriptionid;
  };
  error = rc;

  LOCKMUTEX(eventmutex);
  subscription_count--;
  UNLOCKMUTEX(eventmutex);
  return error;
};

int _mwunsubscribe(int subid)
{
   return _mwaddress.proto.unsubscribe(subid);
};

int mwunsubscribe(int subid)
{
  subscribed_events_t * se = NULL;
  int error = 0;
  int i, rc;

  if (subid < 0) return -EINVAL;

  LOCKMUTEX(eventmutex);
  for (i = 0; i < subscription_count; i++) {
    if (subscriptions[i].subscriptionid == subid) {
      se = &subscriptions[i];
      break;
    }
  };

  if (se == NULL) {
    error = -ENOENT;
    goto errout;
  };

  DEBUG1("UNSUBSCRIBE %d", se->subscriptionid);
  rc =  _mwunsubscribe(se->subscriptionid);

  free(se->pattern);
  if (rc >= 0) {
    DEBUG1("unsubscription OK");    
    subscription_count--;
    memcpy(se, &subscriptions[subscription_count], sizeof(subscribed_events_t));
  };

 errout:

  UNLOCKMUTEX(eventmutex);
  return error;
};
    
int mwevent(const char * event, const char * data, size_t datalen, const char * username, const char * clientname) 
{

  /* input sanyty checking, everywhere else we depend on params to be sane. */
  if ( event == NULL ) 
    return -EINVAL;
  
  DEBUG1("called with event %.64s", event);

  /* datalen may be zero if data is null terminated */
  if ( (data != NULL ) && (datalen == 0) ) datalen = strlen(data);

  return _mwaddress.proto.event(event, data, datalen, username, clientname, UNASSIGNED, 0);
};

void mwrecvevents(void)
{
   _mwaddress.proto.recvevents();
    
  return;
};


/* called after receiving a IPC of SRB event message, actually
   executes the event handler */
void _mw_doevent_c(int subid, const char * event, const char * data, int datalen)
{
  int i;

  DEBUG1("attempring to find callback for event %s subid %d", event, subid);
  
  for (i = 0; i < subscription_count; i++) {
    if (subscriptions[i].subscriptionid == subid) {
      
      DEBUG1("Found callback calling with data = %p, datalen = %d", data, datalen);
      subscriptions[i].callback(event, data, datalen);
      break;
    }
  };
};

mw_do_event_handler_t _mw_do_event_handler = _mw_doevent_c;

void _mw_doevent(int subid, const char * event, const char * data, size_t datalen)
{
   LOCKMUTEX(eventmutex);

   _mw_do_event_handler(subid, event, data, datalen);

   UNLOCKMUTEX(eventmutex);
};

void _mw_register_event_handler(mw_do_event_handler_t event_handler)
{
   _mw_do_event_handler = event_handler;
};


/************************************************************************/

/* TRANSACTIONAL API
   these calls are currently here only to set a deadline.
   (the MWNOTRAN flags is implied. Should we require it?)
*/
int mwbegin(float fsec, int flags)
{
  struct timeval now;
  float s, ss;
  
  errno = 0;
  DEBUG1("mwbegin called timeout = %f", fsec);
  if (fsec <= 0) return -EINVAL;

  gettimeofday(&now, NULL);
  s = fabs(fsec);
  ss = fsec - s;
  
  deadline.tv_sec = now.tv_sec + (int) s;
  deadline.tv_usec = now.tv_usec + (int) (ss * 1000000); /*micro secs*/
  DEBUG3("mwbegin deadline is at %ld.%ld",
	 deadline.tv_sec, (long) deadline.tv_usec); 
  return 0;
};

int mwcommit()
{
  DEBUG1("mwcommit:");

  deadline.tv_sec = 0;
  deadline.tv_usec = 0;
  return 0;
};

int mwabort()
{
  DEBUG1("mwabort:");

  deadline.tv_sec = 0;
  deadline.tv_usec = 0;
  return 0;
};

/**********************************************************************
 * Memory allocation 
 **********************************************************************/

/* for IPC we use shmalloc. For network we use std malloc */
void * mwalloc(size_t size) 
{
   void * addr;

   DEBUG3("size = %zu", size);
   if (size < 1) return NULL;

   if (_mwaddress.protocol != MWSYSVIPC) {
      DEBUG3("mwalloc: using malloc");
      addr = malloc(size);
   } else {
      DEBUG3("mwalloc: using _mwalloc");
      addr = _mwalloc(size);
   };
   DEBUG3("returning mem at %p", addr);
   return addr;
};

void * mwrealloc(void * adr, size_t newsize)
{
   char * naddr;
   
   DEBUG3("old mem %p size = %zu", adr, newsize);
   if (_mwshmcheck(adr) == -1) {
      DEBUG3("mwrealloc: using realloc");
      naddr =  realloc(adr, newsize);
   } else {
      DEBUG3("mwrealloc: using _mwrealloc");
      naddr = _mwrealloc(adr, newsize);
   };
   DEBUG3("returning mem at %p", naddr);
   return naddr;
};


int mwfree(void * adr) 
{
   int rc = 0;
   DEBUG3("mem %p", adr);
   if (_mwshmcheck(adr) == -1) {
      DEBUG3("mwfree: using free");
      free(adr);
   } else {
      DEBUG3("mwfree: using _mwfree");
      rc =  _mwfree( adr);
   };
   DEBUG3("returning rc = %d", rc);
   return rc;
};
 
