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


/*
 * $Log$
 * Revision 1.19  2004/12/29 19:57:56  eggestad
 * Large data  protocol fix up
 *
 * Revision 1.18  2004/11/17 20:39:42  eggestad
 * MIssmatch of EVENT & EVENTID SRBP fieldnames between implementation and spec
 *
 * Revision 1.17  2004/04/12 22:55:17  eggestad
 * - fix of bug where readmessage returned error while actually read a message
 *
 * Revision 1.16  2004/04/08 10:34:05  eggestad
 * introduced a struct with pointers to the functions implementing the midway functions
 * for a given protocol.
 * This is in preparation for be able to do configure with/without spesific protocol.
 * This creates a new internal API each protocol must addhere to.
 *
 * Revision 1.15  2004/03/20 18:57:47  eggestad
 * - Added events for SRB clients and proppagation via the gateways
 * - added a mwevent client for sending and subscribing/watching events
 * - fix some residial bugs for new mwfetch() api
 *
 * Revision 1.14  2004/03/01 12:56:14  eggestad
 * added event API for SRB client
 *
 * Revision 1.13  2003/09/28 10:45:59  eggestad
 * duplicate initalizer
 *
 * Revision 1.12  2003/09/25 19:36:17  eggestad
 * - had a serious bug in the input handling of SRB messages in the Connection object, resulted in lost messages
 * - also improved logic in blocking/nonblocking of reading on Connection objects
 *
 * Revision 1.11  2003/08/06 23:16:18  eggestad
 * Merge of client and mwgwd recieving SRB messages functions.
 *
 * Revision 1.10  2003/07/13 22:40:59  eggestad
 * - mwfetch(,,,MWMULTPILE) returned queued replies in the wrong order
 * - added timepegs
 *
 * Revision 1.9  2003/01/07 08:26:41  eggestad
 * added TCP_NODELAY
 *
 * Revision 1.8  2002/09/05 23:25:33  eggestad
 * ipaddres in  mwaddress_t is now a union of all possible sockaddr_*
 * MWURL is now used in addition to MWADDRESS
 *
 * Revision 1.7  2002/08/09 20:50:15  eggestad
 * A Major update for implemetation of events and Task API
 *
 * Revision 1.6  2002/07/07 22:35:20  eggestad
 * *** empty log message ***
 *
 * Revision 1.5  2001/10/16 16:18:09  eggestad
 * Fixed for ia64, and 64 bit in general
 *
 * Revision 1.4  2001/09/16 00:05:56  eggestad
 * Wrong Licence header
 *
 * Revision 1.3  2000/09/21 18:21:55  eggestad
 * Major work, added acall/fetch and got it to work
 *
 * Revision 1.2  2000/08/31 21:46:51  eggestad
 * Major rework
 *
 * Revision 1.1  2000/07/20 19:12:13  eggestad
 * The SRB protcol
 *
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

static char * RCSId UNUSED = "$Id$";

Connection cltconn = { 
   fd:            -1, 
   rejects:        0,
   domain:        NULL, 
   version:       0.0, 
   messagebuffer: NULL,
   role:          -1
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
   int datachunksleft;
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
   memset(crelm, sizeof(struct _srb_callreplyqueue_element), '\0');

   crelm->map = map;
   crelm->handle = map[idx].value;
   DEBUG3("pushCRqueue: handle = %s map=%p", crelm->handle, map);
 
   idx = urlmapget(map, SRB_DATATOTAL);
   if (idx != -1) {
      // FIXME
      crelm->datachunksleft = atoi(map[idx].value);
   } else {
      crelm->datachunksleft = 0;
   };
   DEBUG3("pushCRqueue: SVCCALL reply message has %d datachunks to go", 
	  crelm->datachunksleft);

   /* used only to hold the rest of the data iff more data */
   crelm->data = NULL;

   /* push it, on wait queue if more data chunks comming. */
   if (crelm->datachunksleft > 0) {
      crelm->next = callreplyqueue_waiting;
      callreplyqueue_waiting = crelm;
      DEBUG3(	  "pushCRqueue: reply with handle %s placed in waiting queue, datachunks = %d",
		  crelm->handle, crelm->datachunksleft);
   } else {
      crelm->next = callreplyqueue;
      callreplyqueue = crelm;
      DEBUG3("pushCRqueue: complete!");
   } 
   TIMEPEGNOTE("end");
   return 1;
};

static int pushCRqueueDATA(urlmap * map);
static urlmap * popCRqueue(char * handle)
{
   struct _srb_callreplyqueue_element * crethis, * creprev, **creprevmatch;
   urlmap * map;

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
      map = callreplyqueue->map;
      free(callreplyqueue);
      callreplyqueue = NULL;
      DEBUG3("popCRqueue: poped the only (and correct) element");
      TIMEPEGNOTE("end one and only");
      return map;
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
      map = crethis->map;
      free (crethis);
      DEBUG3("popCRqueue: poped the oldest");
      TIMEPEGNOTE("end any");
      return map;
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

   map = crethis->map;
   free(crethis);
   DEBUG3("popCRqueue: poped the reply for handle %s map=%p", handle, map);
   TIMEPEGNOTE("end found");
   return map;
};


#if 0
static char * recvbuffer = NULL;
static int recvbufferoffset = 0;
static int recvbufferend = 0;

/* the logic in this function is a bit confusing. The reason is that
   we try to get as much as possible from the TCP queue into the
   recvbuffer before returning. THis is just to be nice to the
   mwgwd.*/
static SRBmessage * readmessage(Connection * conn, int blocking)
{
   int rc; 
   static int count = 0;
   char * szTmp;
   SRBmessage * srbmsg = NULL;

   TIMEPEGNOTE("begin");

   if (recvbuffer == NULL) recvbuffer = malloc(SRBMESSAGEMAXLEN);
   if (recvbuffer == NULL) return NULL;

   DEBUG3("client readmessage: STARTING count=%d recvbufferoffset=%d recvbufferend=%d",
	  count, recvbufferoffset,  recvbufferend);
   szTmp = recvbuffer+recvbufferoffset;
   DEBUG1("client readmessage(%s)", blocking?"blocking":"nonblocking");

   /* we're called blocking and there is atleast a message in the
      buffer, switch to non blocking. If we didn't we would be blocking
      in read() while the message we're seeking is in the buffer. (I
      just spent 2 hours last night trying to figure that one out (HEY!
      is was 1 am!) */
   if ((count > 0) && (blocking)) blocking = 0;
   if (blocking) blockingmode(cltconn.fd);

   /* while there is more to come, we set rc = 1 to "fake" that we just
      read somthing */
   rc = 1;
   while (rc > 0) {
      DEBUG3("client readmessage: calling read(fd=%d) with %d bytes in the buffer",
	     cltconn.fd, recvbufferend-recvbufferoffset);

      TIMEPEGNOTE("doing read");

      rc = read(cltconn.fd, 
		recvbuffer+recvbufferend,
		9000-recvbufferend);

      TIMEPEGNOTE("done");

      if (rc == -1) {
	 if (errno == EINTR) break;
	 if (errno == EAGAIN) break;
	 /* we really need to check to see if the last message was a
	    TERM, if it is still in the recvbuffer */
	 Error(	    "client readmessage: connection broken when reading SRB message errno=%d",
		    errno);
	 close(cltconn.fd);
	 cltconn.fd = -1;
	 errno = EPIPE;
	 TIMEPEGNOTE("end NULL");
	 return NULL;
      };
      DEBUG3("client readmessage: read %d bytes: \"%s\"", 
	     rc, recvbuffer); 
      if (rc > 0)
	 recvbufferend += rc;

      /* count all *new* message terminators */
      while( (szTmp = strstr(szTmp, "\r\n")) != 0) {
	 DEBUG3("found a message terminator at %ld", 
		(long) ((void*)szTmp - (void*)(recvbuffer))); 
	 szTmp += 2;
	 count++;
      };
      if (count == 0) DEBUG3("No complete message, blocking = %d", blocking);

      /* if blocking eq 0, get all, and we're happy with none, if !0, get
	 atleast one, */
      DEBUG3("(blocking = %d && count=%d) == %d", 
	     blocking, count, (blocking && !count));
      if (blocking && count) {
	 DEBUG3("blocking mode and %d messages, breaking out", count);
	 rc = 0;
      };
    
      /* if buffer is full, and no complete message. */
      if ( (recvbufferend >= SRBMESSAGEMAXLEN) && !count) {
      
	 /* if there is free space at the beginning, move the full buffer
	    (this better not happen too often */
	 if (recvbufferoffset > 0) {
	    DEBUG1("client readmessage: Moving %d bytes to the beginning of the recvbuffer", 
		   SRBMESSAGEMAXLEN - recvbufferoffset);
	    memmove(recvbuffer, recvbuffer + recvbufferoffset, 
		    SRBMESSAGEMAXLEN - recvbufferoffset);
	 } else {
	    Error(	      "client readmessage: SRB message not complete within 9000 bytes, closing");
	    close(cltconn.fd);
	    cltconn.fd = -1;
	    recvbufferoffset = recvbufferend = 0;
	    errno = EPROTO;
	    TIMEPEGNOTE("end broken mesg");
	    return NULL;
	 };
      };
   };

   if (blocking) nonblockingmode(cltconn.fd);

   DEBUG3("client readmessage: has %d messages ready", count);

   /* now if we have messages in the buffer */
   if (count) {
      DEBUG3("client readmessage: picking message, offset = %d, end = %d",
	     recvbufferoffset, recvbufferend);
      szTmp = strstr(recvbuffer + recvbufferoffset, "\r\n");
      /* szTmp can't be NULL*/
      * szTmp = '\0';

      _mw_srb_trace(1, &cltconn, recvbuffer + recvbufferoffset, 0);
      srbmsg = _mw_srbdecodemessage(recvbuffer + recvbufferoffset);
      count--;
      DEBUG3("client readmessage: recvbufferoffset = szTmp - recvbuffer + 2: %d = %p - %p +2 (=%ld)", 
	     recvbufferoffset, szTmp, recvbuffer, (long)((void*)szTmp - (void*)recvbuffer + 2));
      recvbufferoffset = (long) ((void*)szTmp - (void*)recvbuffer + 2);
      if (recvbufferoffset == recvbufferend) {
	 recvbufferoffset = recvbufferend = 0;
	 recvbuffer[0] = '\0';
	 DEBUG3("client readmessage: recvbuffer now empty with count = %d", count);
	 count = 0; /* just in case */
      };
   }
   TIMEPEGNOTE("end good messgae");
   return srbmsg ;
};
#endif

  
/**********************************************************************
 * PUBLIC API
 **********************************************************************/
int _mwattach_srb(int type, mwaddress_t *mwadr, char * name, mwcred_t * cred, int flags)
{
   int s, rc;
   int val = 1, len;
   SRBmessage * srbmsg;
   if (mwadr == NULL) return -EINVAL;

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
   rc = setsockopt(s, SOL_TCP, TCP_NODELAY, &val, len);
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

   if ( (srbmsg  == NULL) || (srbmsg->command == NULL) || (srbmsg->map == NULL) ) {
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

int _mwacall_srb(char * svcname, char * data, int datalen, int flags)
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
	 DEBUG1("got a SRBCALL message\"%s%c\"",  
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
      } 

      /* this is a message other than SVCCALL, the only one we
	 recognize is TERM */
      else if (strcmp(srbmsg->command, SRB_TERM) == 0) {
	 DEBUG1("got a TERM message\"%s\"",  srbmsg->command);
	 cltcloseconnect();
	 return -EPIPE;
      } else {
	 Warning("an unexpected SRB message\"%s\"", srbmsg->command);
	 /* TODO: we should reject it */
	 continue;
      };
   } while(1);
   
   TIMEPEGNOTE("end");
   return rc;
};

  
int _mwfetch_srb(int *hdl, char ** data, int * len, int * appreturncode, int flags)
{
   urlmap * map = NULL;
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
   map = popCRqueue(szHdl);

  
   DEBUG1("pop in the CR queue for handle \"%s\"(%d) %s", 
	  szHdl, handle, map?"found a match":"had no match");

   // if nonblocking and no reply at this point we return
   if ( (map == NULL) && (flags&MWNOBLOCK) ) return -EAGAIN;

   while (map == NULL) {
      DEBUG1("no SVCCALL with right handle waiting and MWNOBLOCK=%d",
	     (flags&MWNOBLOCK));
      
      rc = _mw_drain_socket(1);
      if (rc < 0) return rc;
      if (rc == 0) continue;
      map = popCRqueue(szHdl);
   } 
   
   rc = _mw_get_returncode(map);

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
   DEBUG1("about to return %d bytes of data and RC=%d", * len, rc);
   TIMEPEGNOTE("getting return params");

   idx = urlmapget(map, SRB_DATA);
   if (idx != -1) {
      *data = map[idx].value;
      map[idx].value = NULL;
      *len = map[idx].valuelen;
      map[idx].valuelen = 0;
   } else {
      *data = NULL;
      *len = 0;
   };
  
   idx = urlmapget(map, SRB_APPLICATIONRC);
   if (idx != -1) {
      /* negative?? */
      *appreturncode = atoi(map[idx].value);
   } else {
      *appreturncode = 0;
   };

   idx = urlmapget(map, SRB_APPLICATIONRC);
   if (idx != -1) {
      /* negative?? */
      *appreturncode = atoi(map[idx].value);
   } else {
      *appreturncode = 0;
   };

   idx = urlmapget(map, SRB_HANDLE);
   if (idx == -1) {
      Fatal("failed to find HANDLE in a SRB CALL reply");
   };
   handle = strtol(map[idx].value, NULL, 16);
   DEBUG1("handle = %s => %#x", map[idx].value, handle);
   *hdl = handle;

 errout:
   if (map) urlmapfree(map);
   TIMEPEGNOTE("end");
   DEBUG1("LEAVE ");
   return rc; 

};

int _mwevent_srb(char * evname, char * data, int datalen, char * username, char * clientname, MWID fromid, int remoteflag)
{
   return _mw_srbsendevent(&cltconn, evname, data, datalen, username, clientname);
};

int _mwsubscribe_srb(char * pattern, int id, int flags) 
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
