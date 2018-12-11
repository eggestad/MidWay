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
  License along with the MidWay distribution; see the file COPYING. If not,
  write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA. 
*/

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define _SRB_CLIENT_C

#include <MidWay.h>
#include <SRBprotocol.h>
#include <urlencode.h>
#include <address.h>
#include <mwclientapi.h>
#include <connection.h>

Connection cltconn = { 
   .fd =              -1, 
   .rejects =          0,
   .domain =        NULL, 
   .version =        0.0, 
   .messagebuffer = NULL,
   .role =            -1
}; 

#if 0
/* a good example of the difference between client code, and server
   code, in server code we would never call fcntl() twice, but rather
   store the flags. */
static void blockingmode(int fd)
{
   int flags;

   if (fd == -1) return;
   flags = fcntl(fd, F_GETFL);
   if (flags == -1) return;
   fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
   return;
};

static void nonblockingmode(int fd)
{
   int flags;

   if (fd == -1) return;
   flags = fcntl(fd, F_GETFL);
   if (flags == -1) return;
   fcntl(fd, F_SETFL, flags | O_NONBLOCK);
   return;
};
#endif

static void cltcloseconnect(void)
{
   DEBUG1("client close connection");
   cltconn.fd = -1;
   if (cltconn.domain != NULL) {
      free(cltconn.domain);
      cltconn.domain = NULL;
   };
   cltconn.rejects = 1;
   cltconn.version = 0.0;
};

/**********************************************************************
 * Functions for maintaining client receiving queues
 **********************************************************************/
struct _srb_callreplyqueue_element {
   char * handle;
   urlmap * map;
   int dataoctetsleft;
   int datatotal;
   char * data;
   struct _srb_callreplyqueue_element  * next;
} * callreplyqueue = NULL, * callreplyqueue_waiting = NULL;

/* we push onto the top (root) */
static int pushCRqueue(urlmap * map)
{
   struct _srb_callreplyqueue_element * crelm;
   int idx;

   TIMEPEGNOTE("begin");
   DEBUG3("pushCRqueue starts");
 
   if (map == NULL) return 0;
   idx = urlmapget(map, SRB_HANDLE);
   if (idx == -1) {
      Warning("Got a SVCCALL reply message from gateway without handle");
      return 0;
   };
   /* create the queue entry and enter the members */
   crelm = malloc(sizeof(struct _srb_callreplyqueue_element));
   /* just to be safe */
   memset(crelm, '\0', sizeof(struct _srb_callreplyqueue_element));

   crelm->map = map;
   crelm->handle = map[idx].value;
   DEBUG1("handle = %s map=%p", crelm->handle, map);

   /* used only to hold the rest of the data iff more data */
   crelm->data = NULL;
 
   idx = urlmapget(map, SRB_DATATOTAL);
   if (idx != -1) {      
      int l;
      crelm->dataoctetsleft = atoi(map[idx].value);
      crelm->datatotal = crelm->dataoctetsleft;
      crelm->data = malloc(crelm->dataoctetsleft);

      idx = urlmapget(map, SRB_DATA);
      if (idx != -1) {
	 l = map[idx].valuelen;
	 DEBUG3("got %d octets of data in call message", l);
	 memcpy(crelm->data, map[idx].value, l);
	 crelm->dataoctetsleft -= l;
      } else {
	 DEBUG1("no data in call message but %d octets pending", crelm->dataoctetsleft);
      };
   } 

   DEBUG3("SVCCALL reply message has %d dataoctets to go", 
	  crelm->dataoctetsleft);


   /* push it, on wait queue if more data chunks comming. */
   if (crelm->dataoctetsleft > 0) {
      crelm->next = callreplyqueue_waiting;
      callreplyqueue_waiting = crelm;
      DEBUG3("reply with handle %s placed in waiting queue, dataoctets = %d",
	     crelm->handle, crelm->dataoctetsleft);
   } else {
      crelm->next = callreplyqueue;
      callreplyqueue = crelm;
      DEBUG3("complete!");
   } 
   TIMEPEGNOTE("end");
   return 1;
};

/* like puchCRqueue we do not expect the caller to free */
static int pushCRqueueDATA(urlmap * map)
{
   char * data, * hdl;
   int dataidx;
   int datalen;
   struct _srb_callreplyqueue_element * crelm, **crptr;

   dataidx = urlmapget(map, SRB_DATA);

   if (dataidx == -1) {
      Warning("got a SVCDATA message without required data");
      urlmapfree(map);
      return -EINVAL;
   }
   hdl  = urlmapgetvalue(map, SRB_HANDLE);
   if (hdl == NULL) {
      Warning("got a SVCDATA message without required handle");
      urlmapfree(map);
      return -1;
   };

   DEBUG1("got a data part for handle %s with len %d", hdl , map[dataidx].valuelen);
   data = map[dataidx].value;
   datalen = map[dataidx].valuelen;

   crptr = & callreplyqueue_waiting;
   while(*crptr != NULL) {
      crelm = *crptr;
      off_t offset;

      DEBUG3("checking waiting element handel %s for this handle %s", crelm->handle, hdl);
      if (strcasecmp(crelm->handle, hdl) == 0) {
	 DEBUG1("found waiting element for key %s ", hdl);
	 DEBUG1("pending data = %d new data = %d remanding %d", crelm->dataoctetsleft, datalen,  crelm->dataoctetsleft - datalen);
	 if ( (crelm->dataoctetsleft - datalen) < 0) {
	    Error("peer has sent to much data, disconnecting");
	    // TODO: cleanup
	    return -EBADMSG;
	 };

	 offset = crelm->datatotal - crelm->dataoctetsleft;
	 memcpy(crelm->data+offset, data, datalen);
	 crelm->dataoctetsleft -= datalen;
	 //	 printf ("at %ld len %d left %d\n", offset, datalen, crelm->dataoctetsleft);

	 if ( crelm->dataoctetsleft == 0) {
	    // dequeue wait queue
	    *crptr = crelm->next;
	    // enqueue complete queue
	    crelm->next = callreplyqueue;
	    callreplyqueue = crelm;
	 };
	 break;
      }
      crptr = &crelm->next;
   }
   urlmapfree(map);
   return 0;
};

static struct _srb_callreplyqueue_element *  popCRqueue(char * handle)
{
   struct _srb_callreplyqueue_element * crethis, * creprev, **creprevmatch;
   

   TIMEPEGNOTE("begin");
   if (callreplyqueue == NULL) {
      DEBUG3("popCRqueue: queue empty");
      return NULL;
   };
   /* we treat the case of only one special, make clearer code, and is
      the most usual case. */
   if (callreplyqueue->next == NULL) {
      DEBUG3("popCRqueue: only one element in queue");
      if (handle != NULL) {
	 /* the only queue entry, and handle != NULL, and handle
	    don't match, return */
	 if (strcasecmp(callreplyqueue->handle,handle) != 0) {
	    TIMEPEGNOTE("end no found (1)");
	    return NULL;
	 }
      };
      crethis = callreplyqueue;
      callreplyqueue = NULL;
      DEBUG3("popCRqueue: poped the only (and correct) element");
      TIMEPEGNOTE("end one and only");
      return crethis;
   };

   /* if handle == NULL, get the last, and return */
   if (handle == NULL) {
      crethis = callreplyqueue->next;
      creprev = callreplyqueue;
      while (crethis->next != NULL) {
	 creprev = crethis;
	 crethis = crethis->next;
      };
      creprev->next = NULL;
      
      DEBUG3("popCRqueue: poped the oldest");
      TIMEPEGNOTE("end any");
      return crethis;
   };

   /* search thru for the handle, but we most get the oldest, (at the end) */
   crethis = callreplyqueue;
   creprev = NULL;
   creprevmatch = NULL;
   while (crethis != NULL) {
      if (strcasecmp(crethis->handle,handle) == 0) {
	 if (creprev == NULL) {
	    creprevmatch = &callreplyqueue;
	 } else {
	    creprevmatch = &creprev->next;
	 };
      };
      creprev = crethis;
      crethis = crethis->next;
   };

   if (creprevmatch == NULL) {
      DEBUG3("popCRqueue: no entry to pop for handle %s", handle);
      TIMEPEGNOTE("end no found");
      return NULL;
   };

   crethis = *creprevmatch;
   *creprevmatch = crethis->next;

   DEBUG3("popCRqueue: poped the reply for handle %s", handle);
   TIMEPEGNOTE("end found");
   return crethis;
};



  
/**********************************************************************
 * PUBLIC API
 **********************************************************************/
int _mwattach_srb(int type, mwaddress_t *mwadr, const char * name, mwcred_t * cred, int flags)
{
   int s, rc;
   int val = 1, len;
   SRBmessage * srbmsg;
   if (mwadr == NULL) return -EINVAL;

   _mw_srb_traceon(NULL);
   /* connect */
   if (mwadr->ipaddress.sin4 != NULL) {
      DEBUG3("_mwattach_srb: connecting to IP4 address");
      s = socket (AF_INET, SOCK_STREAM, 0);    
      if (s == -1) {
	 Error("_mwattach_srb: creation of socket failed with errno=%d", 
	       errno);
	 return -errno;
      };
      rc = connect(s, (struct sockaddr *) mwadr->ipaddress.sin4, 
		   sizeof(struct sockaddr_in));
      if (rc == -1) {
	 Error("_mwattach_srb: TCP/IP socket connect failed with errno=%d", 
	       errno);
	 return -errno;
      };
      cltconn.fd = s;
   } else {
      Error("_mwattach_srb: unsupported IP version");
      return -EPROTONOSUPPORT;
   };
   DEBUG("_mwattach_srb: socket connection filedesc = %d", cltconn.fd);
   if (cltconn.domain != NULL) free(cltconn.domain);
   cltconn.domain = mwadr->domain;
  
   /* we alloc the recv buffer the first time srbp are used, just to save some
      space incase srb is not used by the client. */
  
   alarm(10);
   /* read SRB READY */
   DEBUG3("_mwattach_srb: awaiting a srb ready message, blocking mode, 10 sec deadline.");
   srbmsg = _mw_srb_recvmessage(&cltconn, 0);
   alarm (0);

   if (srbmsg == NULL) {
      Error("_mwattach_srb: readmessage failed with %d", errno);
      cltcloseconnect();
      return -errno;
   };
   DEBUG3("_mwattach_srb: a message was received");

   /* check SRB READY */
   if (strncasecmp(srbmsg->command, SRB_READY, 32) != 0) {
      Error("_mwattach_srb: Didn't receive %s but %s, closing", 
	    SRB_READY,srbmsg->command);
      cltcloseconnect();
   };
  
   /* here we should check domain and version (see also below) */
   DEBUG3("_mwattach_srb: srb ready OK!");
   _mw_srb_destroy(srbmsg);
     
   len = sizeof(val);
   rc = setsockopt(s, IPPROTO_TCP, TCP_NODELAY, &val, len);
   DEBUG1("Nodelay is set to 1 rc %d errno %d", rc, errno);
    
 
   /* send init */  
   rc = _mw_srbsendinit(&cltconn, cred, name, cltconn.domain);
   if (rc <= 0){
      Error("_mwattach_srb: send srb init failed rc=%d", rc);
      cltcloseconnect();
      return rc;
   };

   /* get reply */
   alarm(10);
   DEBUG3("_mwattach_srb: awaiting a srb init reply, blocking mode, 10 sec deadline.");
   srbmsg = _mw_srb_recvmessage(&cltconn, 0);
   alarm (0);

   if ( (srbmsg  == NULL) || (srbmsg->command[0] == '\0') || (srbmsg->map == NULL) ) {
      rc = -ECONNRESET;
   };

   if (rc < 1) {
      Error("_mwattach_srb: readmessage failed with %d", rc);
      return rc;
   };
   DEBUG3("_mwattach_srb: a message was received");

   if (strncasecmp(srbmsg->command, SRB_INIT, 32) != 0) {
      Error("_mwattach_srb: Didn't receive %s but %s, closing", 
	    SRB_INIT,srbmsg->command);
      cltcloseconnect();
      return -EPROTO;
   };

   rc = _mw_get_returncode(srbmsg->map);
   if (rc < 0) {
      Error("_mwattach_srb: unable to get the return code from the SRB INIT reply, rc = %d", rc);
      cltcloseconnect();
      return -EPROTO;
   };
   if (rc > 0) {
      Error("_mwattach_srb: SRB INIT was rejected, reason = %s(%d)", 
	    _mw_srb_reason(rc), rc);;
      cltcloseconnect();
      return -EPROTO;
   };

   /* we call it OK here */
   return 0;
   /* if rejected for version, attempt to negotioate protocol version */
  
};

int _mwdetach_srb(void)
{
   if (cltconn.fd == -1) return -ENOTCONN;

   _mw_srbsendterm(&cltconn, -1);
   cltcloseconnect();
  
   return 0;
};

int _mwacall_srb(const char * svcname, const char * data, size_t datalen, int flags)
{
   int handle;
   int rc; 

   TIMEPEGNOTE("begin");
   handle = _mw_nexthandle();  
   DEBUG1("got handle=%#x", handle);
   rc = _mw_srbsendcall(&cltconn, handle, svcname, data, datalen, 
			flags);
   if (rc < 0) {
      DEBUG1("_mw_srbsendcall returned errno=%d", -rc);
      return rc;
   };
   DEBUG1("_mw_srbsendcall returned %d, returning handle=%#x", 
	  rc, handle);

   TIMEPEGNOTE("end");
   return handle;
};


static void process_event(urlmap * map)
{
   char * name, * data;
   int datalen;
   int idx, subid;

   DEBUG("enter");
   name = urlmapgetvalue(map, SRB_NAME);
   if (!name) {
      Warning("no event string, ignoring event");
      return;
   };
   idx = urlmapget(map, SRB_SUBSCRIPTIONID);
   if (idx == UNASSIGNED) {
      Warning("no subscription id, ignoring event");
      return;
   };
   subid = atoi(map[idx].value);
   
   idx = urlmapget(map, SRB_DATA);
   if (idx > UNASSIGNED) {
      data = map[idx].value;
      datalen = map[idx].valuelen;
   } else {
      data = NULL;
      datalen = 0;
   };
   DEBUG("data len=%d", datalen);
   _mw_doevent(subid, name, data, datalen);      
   return;
};

/* we drain of all the messages in the TCP queue */

int _mw_drain_socket(int flags)
{
   int rc = 0;
   SRBmessage * srbmsg = NULL;
   
   TIMEPEGNOTE("begin");
   DEBUG1("entring loop nonblocking = %d", flags&MWNOBLOCK);
   do {

      DEBUG3("at top of drain loop about to call _mw_srb_recvmessage(%#x)", 
	     flags);

      /* if we have call reply to return we don't block, nor if NOBLOCK flag is set '*/
      errno = 0;
      if (srbmsg) 
	 _mw_srb_destroy(srbmsg);

      srbmsg = _mw_srb_recvmessage(&cltconn, flags);

      if (srbmsg == NULL) {
	 DEBUG1("readmessage failed with errno=%d", errno);
	 if (rc) {
	    DEBUG1("but we've already read a message, returning OK");
	    return rc;
	 };

	 switch (errno) {
	 
	 case EPIPE:
	    DEBUG1("Disconnected");
	    return -EPIPE;

	 case EAGAIN:
	    DEBUG1("nothing left to read in the TCP queue");
	    return -EAGAIN;

	 case EINTR:
	    if (flags & MWSIGRST)
	       continue;
	    else {
	       DEBUG1("returning due to interrupt");	       
	       return -errno;
	    };
	 }
	 return -errno;
      };

      /* if a reject from the peer, ignore it. */
      if (srbmsg->marker == SRB_REJECTMARKER) { 
	 Error("got a rejected message");
	 continue;/* do loop */
      };
      
      if (strcmp(srbmsg->command, SRB_EVENT) == 0) {
	 process_event(srbmsg->map);
	 continue;
      };

      /* special handling of svccall replies */
      if (strcmp(srbmsg->command, SRB_SVCCALL) == 0) {
	 DEBUG1("got a SRBCALL message\"%s marker=%c\"",  
		srbmsg->command, srbmsg->marker);
	 if (srbmsg->marker != SRB_RESPONSEMARKER) {
	    Error("got a call request in TCP queue!");
	    /* should we send a reject? */
	    continue; /* do loop */
	 };
	 
	 /* format check on the SVCCALL */
	 if (!_mw_srb_checksrbcall(&cltconn, srbmsg)) {
	    continue; /* do loop */
	 };

	 pushCRqueue(srbmsg->map);	 
	 srbmsg->map = NULL;
	 flags |= MWNOBLOCK;
	 rc++;
	 continue; /* do loop */ 
      } 

      if (strcmp(srbmsg->command, SRB_SVCDATA) == 0) {
	  DEBUG1("got a SRBDATA message\"%s%c\"",  
		srbmsg->command, srbmsg->marker);
	  pushCRqueueDATA(srbmsg->map);	 
	  srbmsg->map = NULL;
	  flags |= MWNOBLOCK;
	  rc++;
	  continue;
      };
      /* this is a message other than SVCCALL, the only one we
	 recognize is TERM */
      if (strcmp(srbmsg->command, SRB_TERM) == 0) {
	 DEBUG1("got a TERM message\"%s\"",  srbmsg->command);
	 cltcloseconnect();
	 return -EPIPE;
      }
      
      Warning("an unexpected SRB message\"%s\"", srbmsg->command);
      /* TODO: we should reject it */
      continue;
      
   } while(1);
   
   TIMEPEGNOTE("end");
   return rc;
};

  
int _mwfetch_srb(int *hdl, char ** data, size_t * len, int * appreturncode, int flags)
{
   struct _srb_callreplyqueue_element * callreqelm;
   char * szHdl, buffer[9];;
   int idx= -1, rc;
   int handle = *hdl;

   DEBUG1("ENTER hdl=%#x ", handle);
   TIMEPEGNOTE("begin");

   // first of all drain the socket, not optimal for the client (the
   // fastest would be just to check the internal queue directly, but
   // inorder to be well behaved with the gateway we drain any
   // potential send queues in the gateway. THis will also process any events we're gotten
   _mw_drain_socket(MWNOBLOCK);

   /* first of we check to see if a reply with the handle exists in the
      CR queue already */
   if (handle == 0) 
      szHdl = NULL;
   else {
      szHdl = buffer;
      sprintf (szHdl, "%8.8x", handle);
   };
   callreqelm = popCRqueue(szHdl);

  
   DEBUG1("pop in the CR queue for handle \"%s\"(%d) %s", 
	  szHdl, handle, callreqelm?"found a match":"had no match");

   // if nonblocking and no reply at this point we return
   if ( (callreqelm == NULL) && (flags&MWNOBLOCK) ) return -EAGAIN;

   while (callreqelm == NULL) {
      DEBUG1("no SVCCALL with right handle waiting and MWNOBLOCK=%d",
	     (flags&MWNOBLOCK));
      
      rc = _mw_drain_socket(1);
      DEBUG3("drain socket returned %d", rc);
      if (rc < 0) return rc;
      if (rc == 0) continue;
      callreqelm = popCRqueue(szHdl);
   } 
   
   rc = _mw_get_returncode(callreqelm->map);

   DEBUG1("RETURNCODE=%d", rc);
  
   switch(rc) {
   case -1:
      rc = -EUCLEAN;
      goto errout;
   case SRB_PROTO_NO_SUCH_SERVICE:
      rc = -ENOENT;
      goto errout;
   case SRB_PROTO_SERVICE_FAILED:
      rc = -EFAULT;
      goto errout;      

      /* legal replies. */ 
   case MWMORE:
   case MWSUCCESS:
   case MWFAIL:
      break;
   default:
      rc = -EPROTO;
      goto errout;
   };

   /* now return the data, and RC's */
   DEBUG1("about to return %zu bytes of data and RC=%d", * len, rc);
   TIMEPEGNOTE("getting return params");

   if (callreqelm->data != NULL) {
      *data = callreqelm->data;
      *len = callreqelm->datatotal;
   } else {
      urlmap * map = callreqelm->map;
      idx = urlmapget(map, SRB_DATA);
      if (idx != -1) {
	 // TODO: this will break in future rewrite of urlmap alloc we should go to inline data at the same time
	 *data = map[idx].value;
	 map[idx].value = NULL;
	 *len = map[idx].valuelen;
	 map[idx].valuelen = 0;
      } else {
	 *data = NULL;
	 *len = 0;
      };
   };
  
   idx = urlmapget(callreqelm->map, SRB_APPLICATIONRC);
   if (idx != -1) {
      /* negative?? */
      *appreturncode = atoi(callreqelm->map[idx].value);
   } else {
      *appreturncode = 0;
   };

   idx = urlmapget(callreqelm->map, SRB_APPLICATIONRC);
   if (idx != -1) {
      /* negative?? */
      *appreturncode = atoi(callreqelm->map[idx].value);
   } else {
      *appreturncode = 0;
   };

   idx = urlmapget(callreqelm->map, SRB_HANDLE);
   if (idx == -1) {
      Fatal("failed to find HANDLE in a SRB CALL reply");
   };
   handle = strtol(callreqelm->map[idx].value, NULL, 16);
   DEBUG1("handle = %s => %#x", callreqelm->map[idx].value, handle);
   *hdl = handle;

 errout:
   if (callreqelm->map) urlmapfree(callreqelm->map);
   free(callreqelm);
   TIMEPEGNOTE("end");
   DEBUG1("LEAVE ");
   return rc; 

};

int _mwevent_srb(const char * evname, const char * data, size_t datalen, const char * username, const char * clientname, MWID fromid, int remoteflag)
{
   return _mw_srbsendevent(&cltconn, evname, data, datalen, username, clientname);
};

int _mwsubscribe_srb(const char * pattern, int id, int flags) 
{
   return _mw_srbsendsubscribe(&cltconn, pattern, id, flags);
};

int _mwunsubscribe_srb(int id) 
{
   return _mw_srbsendunsubscribe(&cltconn, id);
};

void _mwrecvevens_srb(void)
{
   _mw_drain_socket(MWNOBLOCK);
};


void _mwsrbprotosetup(mwaddress_t * mwadr)
{
   mwadr->proto.attach = _mwattach_srb;
   mwadr->proto.detach = _mwdetach_srb;

   mwadr->proto.acall = _mwacall_srb;
   mwadr->proto.fetch = _mwfetch_srb;
   mwadr->proto.listsvc = _mw_notimp_listsvc;

   mwadr->proto.event = _mwevent_srb;
   mwadr->proto.recvevents = _mwrecvevens_srb;
   mwadr->proto.subscribe = _mwsubscribe_srb;
   mwadr->proto.unsubscribe = _mwunsubscribe_srb;
};
