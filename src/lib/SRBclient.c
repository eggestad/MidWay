/*
  MidWay
  Copyright (C) 2000 Terje Eggestad

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


static char * RCSId = "$Id$";
static char * RCSName = "$Name$"; /* CVS TAG */

/*
 * $Log$
 * Revision 1.2  2000/08/31 21:46:51  eggestad
 * Major rework
 *
 * Revision 1.1  2000/07/20 19:12:13  eggestad
 * The SRB protcol
 *
 */
#include <sys/types.h>
#include <sys/socket.h>
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

struct state {
  int fd ;
  int rejects;
  char * domain;
  float version;
} ;
static struct state connectionstate = { -1, 1, NULL, 0.0 };

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

static void cltcloseconnect(void)
{
  connectionstate.fd = -1;
  if (connectionstate.domain != NULL) {
    free(connectionstate.domain);
    connectionstate.domain = NULL;
  };
  connectionstate.rejects = 1;
  connectionstate.version = 0.0;
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

  mwlog(MWLOG_DEBUG3, "pushCRqueue starts");

  if (map == NULL) return 0;
  idx = urlmapget(map, SRB_HANDLE);
  if (idx == -1) {
    mwlog(MWLOG_WARNING, "Got a SVCCALL reply message from gateway without handle");
    return 0;
  };
  /* create the queue entry and enter the members */
  crelm = malloc(sizeof(struct _srb_callreplyqueue_element));
  /* just to be safe */
  memset(crelm, sizeof(struct _srb_callreplyqueue_element), '\0');

  crelm->map = map;
  crelm->handle = map[idx].value;
  mwlog(MWLOG_DEBUG3, "pushCRqueue: handle = %s", crelm->handle);
 
  idx = urlmapget(map, SRB_DATACHUNKS);
  if (idx != -1) {
    crelm->datachunksleft = atoi(map[idx].value);
  } else {
    crelm->datachunksleft = 0;
  };
  mwlog(MWLOG_DEBUG3, "pushCRqueue: SVCCALL reply message has %d datachunks to go", 
	crelm->datachunksleft);

  /* used only to hold the rest of the data iff more data */
  crelm->data = NULL;

  /* push it, on wait queue if more data chunks comming. */
  if (crelm->datachunksleft > 0) {
    crelm->next = callreplyqueue_waiting;
    callreplyqueue_waiting = crelm;
    mwlog(MWLOG_DEBUG3, 
	  "pushCRqueue: reply with handle %s placed in waiting queue, datachunks = %d",
	  crelm->handle, crelm->datachunksleft);
  } else {
    crelm->next = callreplyqueue;
    callreplyqueue = crelm;
    mwlog(MWLOG_DEBUG3, "pushCRqueue: complete!");
  } 
  return 1;
};

static int pushCRqueueDATA(urlmap * map);
static urlmap * popCRqueue(char * handle)
{
  struct _srb_callreplyqueue_element * crethis, * creprev;
  urlmap * map;

  if (callreplyqueue == NULL) {
    mwlog(MWLOG_DEBUG3, "popCRqueue: queue empty");
    return NULL;
  };
  /* we treat the case of only one special, make clearer code, and is
     the most usual case. */
  if (callreplyqueue->next == NULL) {
    mwlog(MWLOG_DEBUG3, "popCRqueue: only one element in queue");
    if (handle != NULL) {
      /* the only queue entry, and handle != NULL, and handle
	 don't match, return */
      if (strcasecmp(callreplyqueue->handle,handle) != 0)
	return NULL;
    }
    map = callreplyqueue->map;
    free(callreplyqueue);
    callreplyqueue = NULL;
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
    mwlog(MWLOG_DEBUG3, "popCRqueue: poped the oldest");
    return map;
  };
  /* search thru for the handle */
  crethis = callreplyqueue;
  creprev = NULL;
  while (crethis != NULL) {
    if (strcasecmp(crethis->handle,handle) == 0) 
      break;
    
    creprev = crethis;
    crethis = crethis->next;
  };

  if (crethis == NULL) {
    mwlog(MWLOG_DEBUG3, "popCRqueue: no entry to pop for handle %s", handle);
    return NULL;
  };

  if (creprev == NULL) {
    callreplyqueue = crethis->next;
  } else {
    creprev->next = crethis->next;
  };
  map = crethis->map;
  free(crethis);
  mwlog(MWLOG_DEBUG3, "popCRqueue: poped the reply for handle %s", handle);
  return map;
};

/**********************************************************************
 * Functions for receiving messages 
 **********************************************************************/
static char * recvbuffer = NULL;
static int recvbufferoffset = 0;
static int recvbufferend = 0;

static SRBmessage * readmessage(int blocking)
{
  int rc; 
  static int count = 0;
  char * szTmp;
  SRBmessage * srbmsg = NULL;

  if (recvbuffer == NULL) recvbuffer = malloc(SRBMESSAGEMAXLEN);
  if (recvbuffer == NULL) return NULL;

  szTmp = recvbuffer+recvbufferoffset;
  mwlog(MWLOG_DEBUG1, "client readmessage(%s)", blocking?"blocking":"nonblocking");

  if (blocking) blockingmode(connectionstate.fd);

  /* while there is more to come, we set rc = 1 to "fake" that we just
     read somthing */
  rc = 1;
  while (rc > 0) {
    mwlog(MWLOG_DEBUG3, "client readmessage: calling read() with %d bytes in the buffer",
	  recvbufferend-recvbufferoffset);
    rc = read(connectionstate.fd, 
	      recvbuffer+recvbufferend, 
	      9000-recvbufferend);
    if (rc == -1) {
      if (errno == EINTR) break;
      if (errno == EAGAIN) break;
      /* we really need to check to see if the last message was a
         TERM, if it is still in the recvbuffer */
      mwlog(MWLOG_ERROR, 
	    "client readmessage: connection broken when reading SRB message errno=%d",
	    errno);
      close(connectionstate.fd);
      connectionstate.fd = -1;
      errno = EPIPE;
      return NULL;
    };
    mwlog(MWLOG_DEBUG3, "client readmessage: read %d bytes: \"%s\"", 
	  rc, recvbuffer); 
    if (rc > 0)
      recvbufferend += rc;

    /* count all new message terminators */
    while( (szTmp = strstr(szTmp, "\r\n")) != 0) count++;
	  
    /* if blocking eq 0, get all, and we're happy with none, if !0, get
       atleast one, */
    if (blocking && !count) {
      rc = 1;
    };
    
    /* if buffer is full, and no complete message. */
    if ( (recvbufferend >= SRBMESSAGEMAXLEN) && !count) {
      
      /* if there is free space at the beginning, move the full buffer
	 (this better not happen too often */
      if (recvbufferoffset > 0) {
	mwlog(MWLOG_DEBUG1, "client readmessage: Moving %d bytes to the beginning of the recvbuffer", 
	      SRBMESSAGEMAXLEN - recvbufferoffset);
	memmove(recvbuffer, recvbuffer + recvbufferoffset, 
		SRBMESSAGEMAXLEN - recvbufferoffset);
      } else {
	mwlog(MWLOG_ERROR, 
	      "client readmessage: SRB message not complete within 9000 bytes, closing");
	close(connectionstate.fd);
	connectionstate.fd = -1;
	recvbufferoffset = recvbufferend = 0;
	errno = EPROTO;
	return NULL;
      };
    };
  };
  
  if (blocking) nonblockingmode(connectionstate.fd);
  mwlog(MWLOG_DEBUG3, "client readmessage: has %s messages ready", count);
  if (count) {
    mwlog(MWLOG_DEBUG3, "client readmessage: picking message, offset = %d, end = %d",
	  recvbufferoffset, recvbufferend);
    szTmp = strstr(recvbuffer + recvbufferoffset, "\r\n");
    /* szTmp can't be NULL*/
    * szTmp = '\0';

    _mw_srb_trace(1, connectionstate.fd, recvbuffer + recvbufferoffset, 0);
    srbmsg = _mw_srbdecodemessage(recvbuffer + recvbufferoffset);
    recvbufferoffset += (int)szTmp - (int)recvbuffer + 2;
    if (recvbufferoffset == recvbufferend) {
      recvbufferoffset = recvbufferend = 0;
      mwlog(MWLOG_DEBUG3, "client readmessage: recvbuffer now empty");
    };
  }
  return srbmsg ;
};
  
/**********************************************************************
 * PUBLIC API
 **********************************************************************/
int _mwattach_srb(mwaddress_t *mwadr, char * name, 
		  char * username, char * password, int flags)
{
  int s, rc;
  SRBmessage * srbmsg;
  if (mwadr == NULL) return -EINVAL;

  /* connect */
  if (mwadr->ipaddress_v4 != NULL) {
    mwlog(MWLOG_DEBUG3, "_mwattach_srb: connecting to IP4 address");
    s = socket (AF_INET, SOCK_STREAM, 0);    
    if (s == -1) {
      mwlog(MWLOG_ERROR, "_mwattach_srb: creation of socket failed with errno=%d", 
	    errno);
      return -errno;
    };
    connectionstate.fd = connect(s,mwadr->ipaddress_v4, 
				 sizeof(struct sockaddr_in));
    if (connectionstate.fd == -1) {
      mwlog(MWLOG_ERROR, "_mwattach_srb: TCP/IP socket connect failed with errno=%d", 
	    errno);
      return -errno;
    };
  } else {
    mwlog(MWLOG_ERROR, "_mwattach_srb: unsupported IP version");
    return -EPROTONOSUPPORT;
  };
  
  if (connectionstate.domain != NULL) free(connectionstate.domain);
  connectionstate.domain = mwadr->domain;
  
  /* we alloc the recv buffer the first time srbp are used, just to save some
     space incase srb is not used by the client. */
  if (recvbuffer == NULL) recvbuffer = malloc(SRBMESSAGEMAXLEN+1);
  if (recvbuffer == NULL) {
    mwlog(MWLOG_ERROR, "_mwattach_srb: out of memory errno = %d", 
	    errno);
    cltcloseconnect();
    return -errno;
  };

  alarm(10);
  /* read SRB READY */
  mwlog(MWLOG_DEBUG3, "_mwattach_srb: receiving a srb ready message, blocking mode, 10 sec deadline.");
  srbmsg = readmessage(1);
  alarm (0);

  if (srbmsg == NULL) {
    mwlog(MWLOG_ERROR, "_mwattach_srb: readmessage failed with %d", errno);
    cltcloseconnect();
    return -errno;
  };
  mwlog(MWLOG_DEBUG3, "_mwattach_srb: a message was received");

  /* check SRB READY */
  if (strncasecmp(srbmsg->command, SRB_READY, 32) != 0) {
    mwlog(MWLOG_ERROR, "_mwattach_srb: Didn't receive %s but %s, closing", 
	  SRB_READY,srbmsg->command);
    cltcloseconnect();
  };
  
  /* here we should check domain and version (see also below) */
  mwlog(MWLOG_DEBUG3, "_mwattach_srb: srb ready OK!");
  urlmapfree(srbmsg->map);
  free(srbmsg);


  /* send init */  
  rc = _mw_srbsendinit(connectionstate.fd, username, password, name, connectionstate.domain);
  if (rc != 0){
    mwlog(MWLOG_ERROR, "_mwattach_srb: send srb init failed rc=%d", rc);
    cltcloseconnect();
    return rc;
  };

  /* get reply */
  alarm(10);
  mwlog(MWLOG_DEBUG3, "_mwattach_srb: receiving a srb init reply, blocking mode, 10 sec deadline.");
  srbmsg = readmessage(1);
  alarm (0);
  if (rc < 1) {
    mwlog(MWLOG_ERROR, "_mwattach_srb: readmessage failed with %d", rc);
    return rc;
  };
  mwlog(MWLOG_DEBUG3, "_mwattach_srb: a message was received");

  if (strncasecmp(srbmsg->command, SRB_INIT, 32) != 0) {
    mwlog(MWLOG_ERROR, "_mwattach_srb: Didn't receive %s but %s, closing", 
	  SRB_INIT,srbmsg->command);
    cltcloseconnect();
    return -EPROTO;
  };

  rc = _mw_get_returncode(srbmsg->map);
  if (rc < 0) {
    mwlog(MWLOG_ERROR, "_mwattach_srb: unable to get the return code from the SRB INIT reply, rc = %d", rc);
    cltcloseconnect();
    return -EPROTO;
  };
  if (rc > 0) {
    mwlog(MWLOG_ERROR, "_mwattach_srb: SRB INIT was rejected, reason = %s(%d)", 
	  _mw_srb_reason(rc), rc);;
    cltcloseconnect();
    return -EPROTO;
  };

  /* we call it OK here */
  return 0;
  /* if rejected for version, attempt to negotioate protocol version */
  
};

int _mwdetach_srb()
{
  if (connectionstate.fd == -1) return -ENOTCONN;

  _mw_srbsendterm(connectionstate.fd, -1);
  cltcloseconnect();
  
  return 0;
};

  
int _mwacall_srb(int fd, int handle, char * svcname, char * data, int datalen, int flags)
{
  
int _mwfetch_srb(int handle, char ** data, int * len, int * appreturncode, int flags) 
