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
 * Revision 1.18  2005/10/11 22:17:35  eggestad
 * fix for large data
 *
 * Revision 1.17  2005/06/25 12:08:24  eggestad
 * - added large data
 *
 * Revision 1.16  2004/11/17 20:47:57  eggestad
 * message map leak fix
 *
 * Revision 1.15  2004/11/08 10:51:48  eggestad
 * fix for possible recv buffer corruption when moving remains to beginning
 *
 * Revision 1.14  2004/04/08 10:34:06  eggestad
 * introduced a struct with pointers to the functions implementing the midway functions
 * for a given protocol.
 * This is in preparation for be able to do configure with/without spesific protocol.
 * This creates a new internal API each protocol must addhere to.
 *
 * Revision 1.13  2004/03/20 18:57:47  eggestad
 * - Added events for SRB clients and proppagation via the gateways
 * - added a mwevent client for sending and subscribing/watching events
 * - fix some residial bugs for new mwfetch() api
 *
 * Revision 1.12  2004/03/01 12:56:14  eggestad
 * added event API for SRB client
 *
 * Revision 1.11  2003/09/25 19:36:17  eggestad
 * - had a serious bug in the input handling of SRB messages in the Connection object, resulted in lost messages
 * - also improved logic in blocking/nonblocking of reading on Connection objects
 *
 * Revision 1.10  2003/08/06 23:16:18  eggestad
 * Merge of client and mwgwd recieving SRB messages functions.
 *
 * Revision 1.9  2003/03/16 23:53:53  eggestad
 * bug fixes
 *
 * Revision 1.8  2003/01/07 08:26:48  eggestad
 * * major fixup of trace, added desc propper and fixup direction indicator.
 * * added newline before SRB message in DBEUG for readability on log.
 * * added timepegs
 *
 * Revision 1.7  2002/10/29 23:56:24  eggestad
 * added peer IP adress to srb trace
 *
 * Revision 1.6  2002/10/17 22:02:05  eggestad
 * - added _mw_srb_setfieldx()
 * - changed a debug() to Error() (if remote didn't want rejects we now log the reject as error locally)
 * - added a debug so that  reject is always logged
 *
 * Revision 1.5  2002/07/07 22:35:20  eggestad
 * added urlmapdup
 *
 * Revision 1.4  2001/10/03 22:43:59  eggestad
 * added api for manipulate SRBmessage's, before urlmap* had to be used directly
 *
 * Revision 1.3  2001/09/16 00:07:14  eggestad
 * * Licence header was missing
 * * trace api changes inorder to trace explicitly and to a separate file.
 * * the encodeing part of SRBsendmessage taken out since SRBsendmessage
 *   couldn't be used for UDP
 *
 * Revision 1.2  2000/09/21 18:24:25  eggestad
 * - added _mw_srb_checksrbcall()
 * - deadline now set correctly in srbsendcall()
 * - A few other minor bug fixed
 *
 * Revision 1.1  2000/08/31 19:37:30  eggestad
 * This file is used for implementing SRB client side
 *
 * Revision 1.1  2000/07/20 19:12:13  eggestad
 * The SRB protcol
 *
 */

#include <sys/types.h>
#include <errno.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#include <MidWay.h>
#include <SRBprotocol.h>
#include <version.h>
#include <urlencode.h>
#include <mwclientapi.h>

static char * RCSId UNUSED = "$Id$";

static char * tracefilename = NULL;
static FILE * tracefile  = NULL;
int _mw_srbdata_per_message = 3000;

int _mw_srb_traceonfile(FILE * fp)
{
  if (fp == NULL) return -1;
  if (tracefilename && tracefile) {
    free(tracefilename);
    fclose(tracefile);
  }
  tracefile = fp;
  return 0;
};
/* one little trick; if the filename is set but not open,
   _mw_srb_trace() will try to open. */

int _mw_srb_traceon(char * path)
{
  if ( (tracefilename)  && (tracefilename != path) ) {
    free(tracefilename);
    tracefilename = NULL;
  };

  if (path == NULL) {
    tracefilename = getenv("SRB_TRACE_FILE");
  } else {
    if (tracefilename != path)
      tracefilename = strdup(path);
  };
  
  if (tracefilename  != NULL) {
    tracefile = fopen(tracefilename, "a");
  } else {
    errno = EINVAL;
    return -1;
  };
  
  if (!tracefile) {
    Error("Failed to open srb trace file \"%s\"", tracefilename);
    return -1;
  };
  return 0;
};


int _mw_srb_traceoff(void )
{
  if (tracefilename != NULL) {
    tracefilename = NULL;
  };

  if (tracefile != NULL) {
    fclose(tracefile);
    tracefile = NULL;
  };
  return 0;
};

void _mw_srb_trace(int dir_in, Connection * conn, char * message, int messagelen)
{
  if (tracefilename && !tracefile)
    _mw_srb_traceon(tracefilename);

  if (conn == NULL) {
    Error("Internal error, "__FILE__ ":%d called with conn == NULL", __LINE__);
    return;
  };

  if (messagelen <= 0) messagelen = strlen(message);

  if (tracefile) {
    fprintf (tracefile, "%s%d %s (%d) %.*s",
	     dir_in?">=":"<=", conn->fd, conn->peeraddr_string, messagelen, messagelen, message); 
    fflush(tracefile);
  };

  /* we don't print the trailing \r\n in the log file */
  while ( (message[messagelen-1] == '\r') || (message[messagelen-1] == '\n') ) 
    messagelen--;


  if (mwsetloglevel(-1) >= MWLOG_DEBUG) {   
    DEBUG("TRACE %s fd=%d %s (%d) \n%.*s",
	  dir_in?">=":"<=", conn->fd, conn->peeraddr_string, messagelen, messagelen, message);        
  };
};

/* automation of error strings */

/* just a map between cause codes and a short description */
struct reasonmap 
{
  int rcode; 
  char * reason;
};

static struct reasonmap srbreasons[] = { 
  { SRB_PROTO_OK, 		"Success" },
  { SRB_PROTO_ISCONNECTED,	"301: You are already connected" },
  { SRB_PROTO_UNEXPECTED,	"302: The message was unexpected" },
  { SRB_PROTO_FORMAT,  		"400: General Format Error" }, 
  { SRB_PROTO_FIELDMISSING,  	"401: Required Field Missing" }, 
  { SRB_PROTO_ILLEGALVALUE,  	"402: Field has Illegal Value" }, 
  { SRB_PROTO_CLIENT,  		"403: Request Legal only for Gateways" }, 
  { SRB_PROTO_NOREVERSE,  	"404: Do not accept reverse service calls" }, 
  { SRB_PROTO_AUTHREQUIRED,	"405: Autorization fileds are required" },
  { SRB_PROTO_AUTHFAILED,	"406: Authiozation fialed" },
  { SRB_PROTO_VERNOTSUPP,  	"410: Protocol Version Not Supported" },
  { SRB_PROTO_NOINIT,  		"411: Missing Init Request" }, 
  { SRB_PROTO_NOTNOTIFICATION,	"412: This message command is not accepted as a notification" },
  { SRB_PROTO_NOTREQUEST,	"413: This message command is not accepted as a request" },
  { SRB_PROTO_NO_SUCH_SERVICE,	"430: There are no service wich this given name" },
  { SRB_PROTO_NOGATEWAY_AVAILABLE, "440: No instance with given name available" }, 
  { SRB_PROTO_SERVICE_FAILED,	"431: The service routine returned NOT OK" },
  { SRB_PROTO_GATEWAY_ERROR,	"500: The Gateway had an internal error" },
  { SRB_PROTO_DISCONNECTED,	"501: You are already disconnected" },
  { SRB_PROTO_NOCLIENTS,  	"520: Not accepting connections from Clients" }, 
  { SRB_PROTO_NOGATEWAYS,  	"521: Not accepting connections from Gateways" }, 
  { -1, 			NULL }
};
  
char * _mw_srb_reason(int rc)
{
  static char buffer [100];
  int i = 0;

  while(srbreasons[i].rcode != -1) {
    if (srbreasons[i].rcode == rc)
      return srbreasons[i].reason;
    i++;
  };
  sprintf (buffer, "Error: %d", rc);
  return buffer;
};

int _mw_errno2srbrc(int err)
{
  /* just make sure we have it postive */
  if (err < 0) err = - err;
  switch (err) {
  case ENOENT:
    return SRB_PROTO_NO_SUCH_SERVICE;
  case EFAULT:
    return SRB_PROTO_SERVICE_FAILED;
  case ENOTCONN:
    return SRB_PROTO_GATEWAY_ERROR;
  };
  return 600 + err;
};

/************************************************************************
 * these functions deal vith manipulation of SRBmessages 
 ************************************************************************/
static void vfill_map(SRBmessage * srbmsg, va_list ap) 
{
  char * key, * value;
  urlmap * map;

  for (key = va_arg(ap, char *); key != NULL; key = va_arg(ap, char *)) {
    value = va_arg(ap, char *);
    map = urlmapadd(srbmsg->map, key, value);
    if (map == NULL) {
      urlmapset(srbmsg->map, key, value);
    };
    srbmsg->map = map;
  };
  return;
};  

void _mw_srb_init(SRBmessage * srbmsg, char * command, char marker, ...)
{
  va_list ap;

  if (srbmsg == NULL) return;
  
  if (command != NULL) {
    strncpy(srbmsg->command, command, MWMAXSVCNAME);
    if (strlen(command) == MWMAXSVCNAME) 
      srbmsg->command[MWMAXSVCNAME] = '\0';
  };
  srbmsg->marker = marker;
  srbmsg->map = NULL;

  va_start(ap, marker);
  vfill_map(srbmsg, ap);
  va_end(ap);
  
  return;
};


SRBmessage * _mw_srb_create(char * command, char marker, ...)
{
  SRBmessage * srbmsg;
  va_list ap;

  srbmsg = (SRBmessage *) malloc(sizeof(SRBmessage));
  _mw_srb_init(srbmsg, command, marker, NULL);
  
  va_start(ap, marker);
  vfill_map(srbmsg, ap);
  va_end(ap);

  return srbmsg;
};

void _mw_srb_destroy (SRBmessage * srbmsg)
{
  if (srbmsg == NULL) return;

  DEBUG1("releasing a srbmsg");
  urlmapfree(srbmsg->map);
  free(srbmsg);
};

void _mw_srb_setfieldi (SRBmessage * srbmsg, char * key, int value)
{
  urlmap * map;

  if (srbmsg == NULL) return;
  if (key == NULL) return;
  
  map = urlmapaddi(srbmsg->map, key, value);
  if (map == NULL) 
    urlmapseti(srbmsg->map, key, value);
  else 
    srbmsg->map = map;

  return;
};

#ifdef FAST_ENHEX
static char _hex_digit [16] = { '0', '1', '2', '3', '4', '5', '6', '7', 
				'8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
#endif
void _mw_srb_setfieldx (SRBmessage * srbmsg, char * key, unsigned int value)
{
  urlmap * map;
  char hex[9];

  if (srbmsg == NULL) return;
  if (key == NULL) return;
  
#ifdef FAST_ENHEX
  // this would be faster, but, I'm really unsure how much it matters,
  // besides, There would need to be a big-endian version of this
  for (i = 0; i < 8; i++) {
    hex[7-i] = _hex_digit[value&0xF];
    value >>= 4;
  };
#else
  sprintf (hex, "%8.8x", value);
#endif

  DEBUG1("value %x = > %s", value, hex);
  map = urlmapnadd(srbmsg->map, key, hex, 8);
  if (map == NULL) 
    urlmapnset(srbmsg->map, key, hex, 8);
  else 
    srbmsg->map = map;

  return;
};

void _mw_srb_setfield (SRBmessage * srbmsg, char * key, char * value)
{
  urlmap * map;

  if (srbmsg == NULL) return;
  if (key  == NULL) return;
  
  map = urlmapadd(srbmsg->map, key, value);
  if (map == NULL) 
    urlmapset(srbmsg->map, key, value);
  else 
    srbmsg->map = map;

  return;
};

void _mw_srb_nsetfield (SRBmessage * srbmsg, char * key, void * value, int vlen)
{
  urlmap * map;

  if (srbmsg == NULL) return;
  if (key  == NULL) return;
 
  map = urlmapnadd(srbmsg->map, key, value, vlen);
  if (map == NULL) 
    urlmapnset(srbmsg->map, key, value, vlen);
  else 
    srbmsg->map = map;
  return;
};

char * _mw_srb_getfield (SRBmessage * srbmsg, char * key)
{
  if (srbmsg == NULL) return NULL;
  if (key  == NULL) return NULL;

  return urlmapgetvalue(srbmsg->map,key);
};

void _mw_srb_delfield (SRBmessage * srbmsg, char * key)
{
  urlmapdel(srbmsg->map, key);
  return;
};

/* encode decode functions */

SRBmessage * _mw_srbdecodemessage(Connection * conn, char * message)
{
  char * szTmp, * ptr;
  SRBmessage * srbmsg;
  int commandlen;

  TIMEPEGNOTE("begin");

  DEBUG3("starting on %s(%d) ", conn->peeraddr_string, conn->fd);

  /* message may be both end (\r\n) terminated or, \0 terminated.  if
     we find the end marker, we temporary set it to \0 */
  szTmp = strstr(message, "\r\n");
  if (szTmp != NULL) *szTmp = '\0';

  if ( (ptr = strchr(message, SRB_REQUESTMARKER)) == NULL)
    if ( (ptr = strchr(message, SRB_RESPONSEMARKER)) == NULL)
      if ( (ptr = strchr(message, SRB_NOTIFICATIONMARKER)) == NULL) {
	if ( (ptr = strchr(message, SRB_REJECTMARKER)) != NULL) {
	   DEBUG3("no legal marker found");
	   errno = EBADMSG;
	   return NULL;
	} else { 
	   Warning ("rejected message due to missing srb marker: message \"%s\"", message);
	   _mw_srbsendreject_sz(conn, message, -1);
	   errno = EBADMSG;
	   return NULL;
	}
	
      }
  TIMEPEG();

  commandlen = ptr - message;
  if (commandlen > 32) {
     Warning ("rejected message due to too long srb command: message \"%s\"", message);
    _mw_srbsendreject_sz(conn, message, commandlen);
    errno = EBADMSG;
    return NULL;
  };
  DEBUG3("command = %*.*s marker = %c", commandlen, commandlen, message, *ptr);

  TIMEPEG();

  srbmsg = malloc(sizeof (SRBmessage));

  strncpy(srbmsg->command, message, commandlen);
  srbmsg->command[commandlen] = '\0';
  srbmsg->marker = *ptr;

  TIMEPEG();  
  srbmsg->map = urlmapdecode(message+commandlen+1);
  TIMEPEG();

  if (srbmsg->map == NULL) {
     Warning ("rejected message due to error in decode: message \"%s\"", message);
     _mw_srbsendreject_sz(conn, message, -1);
     _mw_srb_destroy(srbmsg);
     errno = EBADMSG;
     return NULL;
  };     

  TIMEPEG();

  /* replacing end of mesg marker */
  if (szTmp != NULL) *szTmp = '\r';
  
  DEBUG1("good return on command=%s marker=%c", 
	srbmsg->command, srbmsg->marker);
  dbg_srbprintmap(srbmsg); 
  TIMEPEGNOTE("end");
  return srbmsg;
}

void conn_read_fifo_enqueue(Connection *conn);
#pragma weak conn_read_fifo_enqueue

void conn_read_fifo_enqueue(Connection *conn)
{
   return;
};
/**********************************************************************
 * Function for receiving messages 
 **********************************************************************/

SRBmessage * _mw_srb_recvmessage(Connection * conn, int flags)
{  
   int i, n, end, start = 0;
   int eof = 0, err = 0;
   char * msgptr;
   SRBmessage * srbmsg;
   int blocking, recurse;
   
   blocking = !(flags & MWNOBLOCK);
   recurse = 0;

   TIMEPEGNOTE("begin");
   DEBUG3("starting conn=%p blocking=%d", conn, blocking);
   if (!conn) {
      errno = EINVAL;
      return NULL;
   };
   if (conn->messagebuffer == NULL) {
      conn->messagebuffer = malloc(SRBMESSAGEMAXLEN+8);
      conn->possible_message_in_buffer = 0;
      conn->leftover = 0;
   };

   if (blocking && (conn->possible_message_in_buffer)) {
      DEBUG3("blockng but there may be a message in buffer, doing non block, and recurse");
      blocking = 0;
      recurse = 1;
   };

   n = _mw_conn_read(conn, blocking);

   TIMEPEG();
   DEBUG3("read a message from fd=%d returned %d errno=%d", conn->fd, n, errno);

   if (n == 0) {
      eof = 1;
      n = 0;
   } else  if (n == -1) {
      if ( (errno == EAGAIN) || (errno = EINTR) ) {
	 eof = 0;
      } else {
	 eof = 1;
      };
      n = 0;
   };
   
   end = conn->leftover + n;
   DEBUG3("readmessage(fd=%d) read %d bytes + %d leftover buffer now:%.*s", 
	  conn->fd, n, conn->leftover, end, conn->messagebuffer);

   TIMEPEG();

   for (i = 0 ; i < end; i++) {
      
      /* if we find a message end, process it */
      if ( (conn->messagebuffer[i] == '\r') && 
	   (conn->messagebuffer[i+1] == '\n') ) {

	 conn->messagebuffer[i] = '\0';

	 DEBUG3("got a message end marker i = %d end = %d", i, end);
	 TIMEPEG();
	 _mw_srb_trace(SRB_TRACE_IN, conn, conn->messagebuffer+start, i);
	 

	 i += 2;
	 start = i;
	 conn->lastrx = time(NULL);
	 TIMEPEG();
	 
	 msgptr = conn->messagebuffer+start;
	 TIMEPEG();
	 
	 if ( (end - start) >= SRBMESSAGEMAXLEN) {
	    Warning("readmessage(fd=%d) message length was exceeded, "
		    "rejecting it, next message should be rejected", conn->fd);
	    conn->leftover = 0;
	    errno = EBADMSG;
	    return NULL;	    
	    
	 } else {
	    errno = 0;
	    srbmsg = _mw_srbdecodemessage(conn, conn->messagebuffer);
	    err = errno;
	 };

	 if (start == end) { 
	    conn->leftover = 0;
	    conn->possible_message_in_buffer= 0;
	 } else {
	    conn->leftover = end - start;
	    conn->possible_message_in_buffer= 1;
	    DEBUG2("memcpy %d leftover %p => %p", 
		   conn->leftover, msgptr,conn->messagebuffer);
	    memmove(conn->messagebuffer, msgptr, conn->leftover);	    
	    msgptr[conn->leftover] = '\0';
	    conn_read_fifo_enqueue(conn);
	 };

	 if (err) {
	    DEBUG3("Got error %d", err);
	    errno = err;
	    TIMEPEGNOTE("end error");
	    return NULL;
	 };

	 /* if inline data we must do something to get it now */
	 TIMEPEGNOTE("end msg");
	 DEBUG3("good end");
	 return srbmsg;
	 
      };
   }
   DEBUG3("no message in the %d bytes in message buffer", conn->leftover);

   conn->possible_message_in_buffer= 0;
   if (eof)
      errno = EPIPE;
   else
      errno = EAGAIN;

   if (recurse) {
      TIMEPEGNOTE("end recurse");
      DEBUG3("end recurse");
      return _mw_srb_recvmessage(conn, flags);
   };

   TIMEPEGNOTE("end no msg");
   DEBUG3("end no message");

   return NULL;
};


/**********************************************************************
 * Functions for Sending messages, semi public.  the functions here
 * pretty much correspond to the different messages defined in the
 * spesification fro the SRB protocol.
 **********************************************************************/
char * _mw_srbmessagebuffer = NULL;

/* _mw_srbencodemessage encodes messages and append \r\n, return actuall length
   or -1 and errno. usually E2BIG if the buffer is to small  */ 
int _mw_srbencodemessage(SRBmessage * srbmsg, char * buffer, int bufflen)
{
  int len1, len2;

  if (bufflen < MWMAXSVCNAME+2) {
    errno = EMSGSIZE;
    return -1;
  };
  
  len1 = sprintf(buffer, "%.*s%c", MWMAXSVCNAME, 
		srbmsg->command, srbmsg->marker);
  
  len2 = urlmapnencode(buffer+len1, SRBMESSAGEMAXLEN-len1, 
		       srbmsg->map);
  
  if (len2 == -1) return -1;
  len1 += len2;
  
  if (len1 > bufflen - 2) {
    errno = EMSGSIZE;
    return -1;
  };
  
  len1 += sprintf(buffer+len1, "\r\n");
  return len1;
};

/* _mw_sendmessage traces if needed.  . We really assumes that message
   is less than SRBMESSAGEMAXLEN long */
int _mw_srbsendmessage(Connection * conn, SRBmessage * srbmsg)
{
  int len;

  TIMEPEG();

  if (conn == NULL) {
    Error("Internal error, "__FILE__ ":%d called with conn == NULL", __LINE__);
    return -EFAULT;
  };

  TIMEPEG();

  /* MUTEX BEGIN */
  if (_mw_srbmessagebuffer == NULL) 
    _mw_srbmessagebuffer = malloc(SRBMESSAGEMAXLEN+1);

  TIMEPEG();

  if (_mw_srbmessagebuffer == NULL) {
    Error("Failed to allocate SRB send message buffer, OUT OF MEMORY!");
    /* MUTEX ENDS */
    return -ENOMEM;
  };

  len = _mw_srbencodemessage(srbmsg, _mw_srbmessagebuffer, SRBMESSAGEMAXLEN+1);
  if (len == -1) return -errno;

  TIMEPEG();

  _mw_srb_trace (SRB_TRACE_OUT, conn, _mw_srbmessagebuffer, len);

  TIMEPEG();  
  errno = 0;
  len = _mw_conn_write(conn, _mw_srbmessagebuffer, len, 0);
  TIMEPEGNOTE("just did write(2)");
  DEBUG3("_mw_srbsendmessage: write returned %d errno=%d", len, errno);
  /* MUTEX ENDS */

  TIMEPEG();

  if (len == -1) 
    return -errno;
  return len;
};



int _mw_srbsendreject(Connection * conn, SRBmessage * srbmsg, 
		   char * causefield, char * causevalue, 
		   int rcode)
{
  int rc;

  if ( (srbmsg == NULL) || (conn == NULL) ) {
    Error("not sending a reject since either conn %p or srbmessage %p is NULL", conn, srbmsg);
    return -EINVAL;
  };

  DEBUG("sending a reject causefield \"%s\" causevalue \"%s\", rcode = %d", 
	causefield?causefield:"", 
	causevalue?causevalue:"", 
	rcode);

  srbmsg->marker = SRB_REJECTMARKER;
  
  if (conn->rejects == 0) {
    DEBUG("peer has notified that no rejects should be sent");
    return 0;
  };

  _mw_srb_setfieldi(srbmsg, SRB_REASONCODE, rcode);
  _mw_srb_setfield(srbmsg, SRB_REASON, _mw_srb_reason(rcode));

  if (causefield != NULL) 
    _mw_srb_setfield(srbmsg, SRB_CAUSEFIELD, causefield);
  
  if (causevalue != NULL) 
    _mw_srb_setfield(srbmsg, SRB_CAUSEVALUE, causevalue);

  rc = _mw_srbsendmessage(conn,srbmsg);
  return rc;
};

/* special case where a message didn't have a proper format, and we
   only have a sting to reject.*/
int _mw_srbsendreject_sz(Connection * conn, char *message, int offset) 
{ 
  int rc; 
  SRBmessage srbmsg;

  if ( (message == NULL) || (conn == NULL) ) {
    DEBUG("not sending a reject since either conn %p or message %p is NULL", conn, message);
    return -EINVAL;
  };

  if (conn->rejects == 0) {
    DEBUG("peer has notified that no rejects should be sent");
    return 0;
  };

  _mw_srb_init(&srbmsg, SRB_REJECT,SRB_NOTIFICATIONMARKER, 
	       SRB_REASON, _mw_srb_reason(SRB_PROTO_FORMAT), 
	       SRB_MESSAGE, message, 
	       NULL);

  _mw_srb_setfieldi(&srbmsg, SRB_REASONCODE, SRB_PROTO_FORMAT);
  if (offset > 0) 
    _mw_srb_setfieldi(&srbmsg, SRB_OFFSET, offset);
  rc = _mw_srbsendmessage(conn, &srbmsg);
  urlmapfree(srbmsg.map);
  return rc;
};

/************************************************************************/ 
// EVENTS

int _mw_srbsendevent(Connection * conn, 
		     char * event,  char * data, int datalen, 
		     char * username, char * clientname)
{
  int rc;
  SRBmessage srbmsg;
  srbmsg.map = NULL;

  if (conn == NULL) return -EINVAL;

  strncpy(srbmsg.command, SRB_EVENT, MWMAXSVCNAME);
  srbmsg.marker = SRB_NOTIFICATIONMARKER;    

  _mw_srb_setfield(&srbmsg, SRB_NAME, event);
  if (data) {
     if (datalen > srbdata_per_message()) {
	Warning("not yet capable to send event with more than 3000 octets or data");
	return -ENOSYS;
     };
     _mw_srb_nsetfield(&srbmsg, SRB_DATA, data, datalen);
  };
  if (username) {     
     _mw_srb_setfield(&srbmsg, SRB_USER, username);
  };

  if (clientname) {     
     _mw_srb_setfield(&srbmsg, SRB_CLIENTNAME, clientname);
  };

  rc = _mw_srbsendmessage(conn, &srbmsg);
  urlmapfree(srbmsg.map);
  return rc;
};



int _mw_srbsendsubscribe(Connection * conn, char * pattern, int subid, int flags)
{
   int rc;
   SRBmessage srbmsg;
   srbmsg.map = NULL;
   
   if (conn == NULL) return -EINVAL;
   
   strncpy(srbmsg.command, SRB_SUBSCRIBE, MWMAXSVCNAME);
   
   srbmsg.marker = SRB_NOTIFICATIONMARKER;    

   _mw_srb_setfield(&srbmsg, SRB_PATTERN, pattern);
   _mw_srb_setfieldi(&srbmsg, SRB_SUBSCRIPTIONID, subid);

   if (flags & MWEVGLOB)
      _mw_srb_setfield(&srbmsg, SRB_MATCH, SRB_SUBSCRIBE_GLOB);
   else if (flags & MWEVREGEXP)
      _mw_srb_setfield(&srbmsg, SRB_MATCH, SRB_SUBSCRIBE_REGEXP);
   else if (flags & MWEVEREGEXP)
      _mw_srb_setfield(&srbmsg, SRB_MATCH, SRB_SUBSCRIBE_EXTREGEXP);
   else 
      _mw_srb_setfield(&srbmsg, SRB_MATCH, SRB_SUBSCRIBE_STRING);
   
   rc = _mw_srbsendmessage(conn, &srbmsg);
   urlmapfree(srbmsg.map);
   return rc;
};

int _mw_srbsendunsubscribe(Connection * conn, int subid)
{
   int rc;
   SRBmessage srbmsg;
   srbmsg.map = NULL;
   
   if (conn == NULL) return -EINVAL;
      
   strncpy(srbmsg.command, SRB_UNSUBSCRIBE, MWMAXSVCNAME);
   
   srbmsg.marker = SRB_NOTIFICATIONMARKER;    
   
   _mw_srb_setfieldi(&srbmsg, SRB_SUBSCRIPTIONID, subid);
   
   rc = _mw_srbsendmessage(conn, &srbmsg);
   urlmapfree(srbmsg.map);
   return rc;
};

/************************************************************************/ 
// Session control 

int _mw_srbsendterm(Connection * conn, int grace)
{
  int rc;
  SRBmessage srbmsg;
  srbmsg.map = NULL;

  if (conn == NULL) return -EINVAL;

  strncpy(srbmsg.command, SRB_TERM, MWMAXSVCNAME);
  if (grace == -1) {
    srbmsg.marker = SRB_NOTIFICATIONMARKER;    
  } else {
    srbmsg.marker = SRB_REQUESTMARKER;
    _mw_srb_setfieldi(&srbmsg, SRB_GRACE, grace);
  };
  rc = _mw_srbsendmessage(conn, &srbmsg);
  urlmapfree(srbmsg.map);
  return rc;
};

int _mw_srbsendinit(Connection * conn, mwcred_t *cred, 
			   char * name, char * domain)
{
  int  rc;
  char * auth = SRB_AUTH_NONE;
  SRBmessage srbmsg;

  if (name == NULL)   return -EINVAL;
  if (conn == NULL) return -EINVAL;

  _mw_srb_init(&srbmsg, SRB_INIT, SRB_REQUESTMARKER, 
	       SRB_VERSION, SRBPROTOCOLVERSION, 
	       SRB_TYPE, SRB_TYPE_CLIENT, 
	       SRB_NAME, name, 
	       NULL);
  /* optional */
  if (cred && cred->username != NULL) {
    auth = SRB_AUTH_PASS;
    _mw_srb_setfield(&srbmsg, SRB_USER, cred->username);
  };

  if (cred && cred->cred.password != NULL)
    _mw_srb_setfield(&srbmsg, SRB_PASSWORD, cred->cred.password);

  if (domain != NULL)
    _mw_srb_setfield(&srbmsg, SRB_DOMAIN, domain);

  _mw_srb_setfield (&srbmsg, SRB_AUTHENTICATION, auth);

  rc = _mw_srbsendmessage(conn, &srbmsg);
  urlmapfree(srbmsg.map);
  return rc;
};

/************************************************************************/ 
// SERVICE call/reply 

/**
 * Send additional data with a service call/reply. This function will
 * divide out into the needed number of messages, so call this only
 * once per additional data.
 *
 * @todo for really large data we need to place the meessages on a
 * queue on the Connection, giving priority to other traffic. The only
 * thing that has lower priority is event data.
 *
 * @param conn The Connection to send on
 * @param call handle
 * @param data a pointer to the data, may not be NULL
 * @param datalen may not be 0 (or neg)
 * @return the total number of octets written on the Connection or -errno
 */
int _mw_srbsenddata(Connection * conn, char * handle, char * data, int datalen)
{
   int l, rc, rclen = 0;
   DEBUG1("begins");
   SRBmessage srbmsg;

   _mw_srb_init(&srbmsg, SRB_SVCDATA, SRB_NOTIFICATIONMARKER, SRB_HANDLE, handle, NULL);
   
   do {
      l = min(srbdata_per_message(), datalen);
      _mw_srb_nsetfield(&srbmsg, SRB_DATA, data, l);
      
      DEBUG1(" sending chunk with len %d", l);
      rc =  _mw_srbsendmessage(conn, &srbmsg);
      data += l;
      datalen -= l;
      rclen += rc;
   } while (datalen > 0);

   DEBUG1("returns normally with rc=%d", rclen);
   return rclen;
};

/**
  send a SVC CALL srb message. 

  @param conn The Connection to send on
  @param call handle
  @param svcname the service name
  @param data a pointer to the data, may be NULL
  @param datalen the number of octets in data, if 0 data is nul terminated, must be 0 if data == NULL. 
  @param flags 
  @return the total number of octets written on the Connection or -errno
 */
int _mw_srbsendcall(Connection * conn, int handle, char * svcname, char * data, int datalen, 
		    int flags)
{
  char hdlbuf[9];
  int rc, datarem = 0;
  SRBmessage srbmsg;
  int noreply;
  float timeleft;

  noreply = flags & MWNOREPLY;
  DEBUG1("begins");

  if (conn == NULL) return -EINVAL;

  /* input validation done before in mw(a)call:mwclientapi.c */
  srbmsg.map = NULL;
  if (noreply) {
    _mw_srb_init(&srbmsg, SRB_SVCCALL, SRB_NOTIFICATIONMARKER, NULL);
  } else {
    _mw_srb_init(&srbmsg, SRB_SVCCALL, SRB_REQUESTMARKER, NULL);
    sprintf(hdlbuf, "%8.8X", (unsigned int) handle);
    _mw_srb_nsetfield(&srbmsg, SRB_HANDLE, hdlbuf, 8);
  };
  _mw_srb_setfield(&srbmsg, SRB_SVCNAME, svcname);
  if (data != NULL) {
    if (datalen == 0) {
      datalen = strlen(data);
    };
    DEBUG1("datalen = %d max is %d", datalen,  srbdata_per_message());
    if (datalen > srbdata_per_message()) {
       _mw_srb_setfieldi(&srbmsg, SRB_DATATOTAL, datalen);
       datarem = datalen - srbdata_per_message() ;
       datalen = srbdata_per_message();
    } else {
       datarem = 0;
    };
    _mw_srb_nsetfield(&srbmsg, SRB_DATA, data, datalen);
  };
  
  if ( _mw_deadline(NULL, &timeleft)) {
    _mw_srb_setfieldi(&srbmsg, SRB_SECTOLIVE, (int) timeleft);
  };

  _mw_srb_setfieldi(&srbmsg, SRB_HOPS, 0);
  
  rc = _mw_srbsendmessage(conn, &srbmsg);
  urlmapfree(srbmsg.map);
  DEBUG1("rc=%d data remainding=%d", rc, datarem);
  if ((rc >= 0) && (datarem > 0)) {
     rc +=  _mw_srbsenddata(conn, hdlbuf, data+datalen, datarem);
  }
  DEBUG1("returns normally with rc=%d", rc);
  return rc;
};

/**********************************************************************
 * some helper functions
 **********************************************************************/

/* verify that the returncode in SRB INIT and SVCCALL are only digits,
 * and is either 3 og 1 digis, then convert to int, and return, if
 * return code is not in the map, return -1, return -2 if the value is
 * illegal */
int _mw_get_returncode(urlmap * map)
{
  int idx, i;
  char buf[4];
  idx = urlmapget(map, SRB_RETURNCODE);
  if (idx == -1) return -1;
  map = map + idx;
  if ( (map->valuelen == 1) || (map->valuelen == 3) ) { 
    for (i=0; i < map->valuelen ; i++) {
      if (! isdigit(map->value[i])) return -2;
      buf[i] = map->value[i];
    }
    buf[i] = '\0';
    return atol(buf);
  } else {
    return -2;
  };
};

/**********************************************************************
 * now we must verify that the messages has a propper format
 * i.e. field checks
 **********************************************************************/
int _mw_srb_checksrbcall(Connection * conn, SRBmessage * srbmsg) 
{
  int idx;

  if (conn == NULL) return -EINVAL;
  if (srbmsg == NULL) return -EINVAL;

  idx = urlmapget(srbmsg->map, SRB_HANDLE);
  /* if a SVCCALL mesg don't have a handle, reject it */
  if (idx == -1) {
    Error("_mw_srb_checksrbcall: got a call reply without a handle");
    _mw_srbsendreject(conn, srbmsg, SRB_HANDLE, NULL, 
		      SRB_PROTO_FIELDMISSING);
    return 0;
  };
  
  idx = urlmapget(srbmsg->map, SRB_RETURNCODE);
  if (idx == -1) { 
    Error("_mw_srb_checksrbcall: RETURNCODE missing in SRBCALL: ");
    _mw_srbsendreject(conn, srbmsg, SRB_RETURNCODE, NULL, 
		      SRB_PROTO_FIELDMISSING);
    return 0;
  } 

  if ( (srbmsg->map[idx].valuelen == 1) && isdigit(srbmsg->map[idx].value[0]) )
    ; /* OK */
  else if ((srbmsg->map[idx].valuelen == 3) 
	   && isdigit(srbmsg->map[idx].value[0])
	   && isdigit(srbmsg->map[idx].value[1])
	   && isdigit(srbmsg->map[idx].value[2])) 
    ; /* OK */
  else {
    Error("_mw_srb_checksrbcall: Illegal format of RETURNCODE in SRBCALL: \"%s\"(%d)",
	  srbmsg->map[idx].value, srbmsg->map[idx].valuelen);
    _mw_srbsendreject(conn, srbmsg, SRB_RETURNCODE, srbmsg->map[idx].value, 
		      SRB_PROTO_ILLEGALVALUE);
    return 0;
  };

  idx = urlmapget(srbmsg->map, SRB_APPLICATIONRC);
  if (idx == -1) { 
    Error("_mw_srb_checksrbcall: APPLICATIONRC missing in SRBCALL: ");
    _mw_srbsendreject(conn, srbmsg, SRB_APPLICATIONRC, NULL, 
		      SRB_PROTO_FIELDMISSING);
    return 0;
  } 

  return 1;
};

static void init(void) __attribute__((constructor));

static void init(void) 
{
   char * envp;
   int i;

   envp = getenv ("SRB_MAX_DATA");
   if (envp) {
      i = atoi(envp);
      if (i > 10) {
	 _mw_srbdata_per_message = i;
      };
   };
   return;
};
   
