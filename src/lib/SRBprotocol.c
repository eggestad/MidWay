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

  if (dir_in) dir_in = '>';
  else dir_in = '<';

  if (messagelen <= 0) messagelen = strlen(message);

  if (tracefile) {
    if (conn->fd > -1) {
      fprintf (tracefile, "%c%d %s (%d) %.*s",
	       dir_in, conn->fd, conn->peeraddr_string, messagelen, messagelen, message); 
    } else {
      fprintf (tracefile, "%c (%d) %.*s", dir_in, messagelen, messagelen, message); 
    };
    fflush(tracefile);
  };

  /* we don't print the trailing \r\n in the log file */
  while ( (message[messagelen-1] == '\r') || (message[messagelen-1] == '\n') ) 
    messagelen--;


  if (mwsetloglevel(-1) >= MWLOG_DEBUG) {
    if (conn->fd > -1) {
      DEBUG("TRACE %c%d %s (%d) %.*s",
	       dir_in, conn->fd, conn->peeraddr_string, messagelen, messagelen, message); 
    } else {
      DEBUG("TRACE %c (%d) %.*s", dir_in, messagelen, messagelen, message); 
    };
   
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

SRBmessage * _mw_srbdecodemessage(char * message)
{
  char * szTmp, * ptr;
  SRBmessage * srbmsg;
  int commandlen;

  /* message may be both end (\r\n) terminated or, \0 terminated.  if
     we find the end marker, we temporary set it to \0 */
  szTmp = strstr(message, "\r\n");
  if (szTmp != NULL) *szTmp = '\0';

  if ( (ptr = strchr(message, SRB_REQUESTMARKER)) == NULL)
    if ( (ptr = strchr(message, SRB_RESPONSEMARKER)) == NULL)
      if ( (ptr = strchr(message, SRB_NOTIFICATIONMARKER)) == NULL) {
	DEBUG1("_mw_srbdecodemessage: unable to find marker");
	errno = EBADMSG;
	return NULL;
      }
  commandlen = ptr - message;
  srbmsg = malloc(sizeof (SRBmessage));

  strncpy(srbmsg->command, message, commandlen);
  srbmsg->command[commandlen] = '\0';
  srbmsg->marker = *ptr;
  
  srbmsg->map = urlmapdecode(message+commandlen+1);
  /* replacing end marker */
  if (szTmp != NULL) *szTmp = '\r';

  DEBUG3("_mw_srbdecodemessage: command=%*.*s marker=%c", 
	commandlen, commandlen, message, *ptr);
  DEBUG1("_mw_srbdecodemessage: command=%s marker=%c", 
	srbmsg->command, srbmsg->marker);
  return srbmsg;
}

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

  if (conn == NULL) {
    Error("Internal error, "__FILE__ ":%d called with conn == NULL", __LINE__);
    return -EFAULT;
  };


  /* MUTEX BEGIN */
  if (_mw_srbmessagebuffer == NULL) 
    _mw_srbmessagebuffer = malloc(SRBMESSAGEMAXLEN+1);

  if (_mw_srbmessagebuffer == NULL) {
    Error("Failed to allocate SRB send message buffer, OUT OF MEMORY!");
    /* MUTEX ENDS */
    return -ENOMEM;
  };

  len = _mw_srbencodemessage(srbmsg, _mw_srbmessagebuffer, SRBMESSAGEMAXLEN+1);
  if (len == -1) return -errno;

  _mw_srb_trace (SRB_TRACE_OUT, conn, _mw_srbmessagebuffer, len);
  
  errno = 0;
  len = write(conn->fd, _mw_srbmessagebuffer, len);
  DEBUG3("_mw_srbsendmessage: write returned %d errno=%d", len, errno);
  /* MUTEX ENDS */
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

int _mw_srbsendinit(Connection * conn, char * user, char * password, 
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
  if (user != NULL) {
    auth = SRB_AUTH_PASS;
    _mw_srb_setfield(&srbmsg, SRB_USER, user);
  };

  if (password != NULL)
    _mw_srb_setfield(&srbmsg, SRB_PASSWORD, password);

  if (domain != NULL)
    _mw_srb_setfield(&srbmsg, SRB_DOMAIN, domain);

  _mw_srb_setfield (&srbmsg, SRB_AUTHENTICATION, auth);

  rc = _mw_srbsendmessage(conn, &srbmsg);
  urlmapfree(srbmsg.map);
  return rc;
};

int _mw_srbsendcall(Connection * conn, int handle, char * svcname, char * data, int datalen, 
		    int flags)
{
  char hdlbuf[9];
  int rc;
  SRBmessage srbmsg;
  int noreply;
  float timeleft;

  noreply = flags & MWNOREPLY;
  DEBUG1("_mw_srbsendcall: begins");

  if (conn == NULL) return -EINVAL;

  /* input validation done before in mw(a)call:mwclientapi.c */
  strncpy(srbmsg.command, SRB_SVCCALL, MWMAXSVCNAME);
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
    if (datalen > 3000) {
      Error("Large data not yet implemented, datalen %d > 3000", datalen);
      urlmapfree(srbmsg.map);
      return -ENOSYS;
    };
    _mw_srb_nsetfield(&srbmsg, SRB_DATA, data, datalen);
  };
  
  if ( _mw_deadline(NULL, &timeleft)) {
    _mw_srb_setfieldi(&srbmsg, SRB_SECTOLIVE, (int) timeleft);
  };

  _mw_srb_setfieldi(&srbmsg, SRB_HOPS, 0);
  
  rc = _mw_srbsendmessage(conn, &srbmsg);
  urlmapfree(srbmsg.map);
  DEBUG1("_mw_srbsendcall: returns normally with rc=%d", rc);
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
