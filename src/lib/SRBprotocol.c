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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <MidWay.h>
#include <SRBprotocol.h>
#include <version.h>
#include <urlencode.h>
#include <mwclientapi.h>

int trace = 1;


void _mw_srb_trace(int dir_in, int fd, char * message, int messagelen)
{
  if (!trace) return;

  if (dir_in) dir_in = '>';
  else dir_in = '<';
  if (messagelen <= 0) messagelen = strlen(message);
  if (fd > -1) {
    fprintf (stdout, "%c%d (%d) %.*s\n",dir_in, fd, messagelen, messagelen, message); 
  } else {
    fprintf (stdout, "%c%d %.*s\n",dir_in, messagelen, messagelen, message); 
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

SRBmessage * _mw_srbdecodemessage(char * message)
{
  char * szTmp, * ptr;
  SRBmessage * srbmsg;
  int commandlen;

  /* message may be both \r\n terminated or, \0 terminated. */
  szTmp = strstr(message, "\r\n");
  if (szTmp != NULL) *szTmp = '\0';

  if ( (ptr = strchr(message, SRB_REQUESTMARKER)) == NULL)
    if ( (ptr = strchr(message, SRB_RESPONSEMARKER)) == NULL)
      if ( (ptr = strchr(message, SRB_NOTIFICATIONMARKER)) == NULL) {
	mwlog(MWLOG_DEBUG1, "_mw_srbdecodemessage: unable to find marker");
	      return NULL;
      }
  commandlen = ptr - message;
  srbmsg = malloc(sizeof (SRBmessage));
  
  strncpy(srbmsg->command, message, commandlen);
  srbmsg->command[commandlen] = '\0';
  srbmsg->marker = *ptr;
  
  srbmsg->map = urlmapdecode(message+commandlen+1);
  if (szTmp != NULL) *szTmp = '\r';

  mwlog(MWLOG_DEBUG3, "_mw_srbdecodemessage: command=%*.*s marker=%c", 
	commandlen, commandlen, message, *ptr);
  mwlog(MWLOG_DEBUG1, "_mw_srbdecodemessage: command=%s marker=%c", 
	srbmsg->command, srbmsg->marker);
  return srbmsg;
}

/**********************************************************************
 * Functions for Sending messages, semi public.  the functions here
 * pretty much correspond to the different messages defined in the
 * spesification fro the SRB protocol.
 **********************************************************************/
char * _mw_srbmessagebuffer = NULL;

/* _mw_sendmessage append \r\n to all messages, and traces if
   needed.  . We really assumes that message is less than
   SRBMESSAGEMAXLEN long */
int _mw_srbsendmessage(int fd, SRBmessage * srbmsg)
{
  int len;

  /* MUTEX BEGIN */
  if (_mw_srbmessagebuffer == NULL) 
    _mw_srbmessagebuffer = malloc(SRBMESSAGEMAXLEN+1);

  if (_mw_srbmessagebuffer == NULL) {
    mwlog(MWLOG_ERROR, "Failed to allocate SRB send message buffer, OUT OF MEMORY!");
    /* MUTEX ENDS */
    return -ENOMEM;
  };
  len = sprintf(_mw_srbmessagebuffer, "%.*s%c", MWMAXSVCNAME, 
		srbmsg->command, srbmsg->marker);
  len += urlmapnencode(_mw_srbmessagebuffer+len, SRBMESSAGEMAXLEN-len, 
		       srbmsg->map);
  
  _mw_srb_trace (0, fd, _mw_srbmessagebuffer, len);

  if (len < SRBMESSAGEMAXLEN - 2)
    len += sprintf(_mw_srbmessagebuffer+len, "\r\n");
  else {
    /* MUTEX ENDS */
    return -E2BIG;
  };
  errno = 0;
  len = write(fd, _mw_srbmessagebuffer, len);
  mwlog(MWLOG_DEBUG3, "_mw_srbsendmessage: write returned %d errno=%d", len, errno);
  /* MUTEX ENDS */
  if (len == -1) 
    return -errno;
  return len;
};



int _mw_srbsendreject(int fd, SRBmessage * srbmsg, 
		   char * causefield, char * causevalue, 
		   int rcode)
{
  int rc;

  if (srbmsg == NULL) return -EINVAL;

  srbmsg->marker = SRB_REJECTMARKER;
  
  srbmsg->map = urlmapaddi(srbmsg->map, SRB_REASONCODE, rcode);
  srbmsg->map = urlmapadd(srbmsg->map, SRB_REASON, _mw_srb_reason(rcode));

  if (causefield != NULL) 
    srbmsg->map = urlmapadd(srbmsg->map, SRB_CAUSEFIELD, causefield);

  if (causevalue != NULL) 
    srbmsg->map = urlmapadd(srbmsg->map, SRB_CAUSEVALUE, causevalue);

  rc = _mw_srbsendmessage(fd,srbmsg);
  return rc;
};

/* special case where a message didn't have a proper format, and we
   only have a sting to reject.*/
int _mw_srbsendreject_sz(int fd, char *message, int offset) 
{ 
  int rc; 
  SRBmessage srbmsg;

  if (message == NULL) return -EINVAL;

  strncpy(srbmsg.command, SRB_REJECT, MWMAXSVCNAME);
  srbmsg.marker = SRB_NOTIFICATIONMARKER;

  srbmsg.map = urlmapaddi(srbmsg.map, SRB_REASONCODE, SRB_PROTO_FORMAT);
  srbmsg.map = urlmapadd(srbmsg.map, SRB_REASON, _mw_srb_reason(SRB_PROTO_FORMAT));
  srbmsg.map = urlmapadd(srbmsg.map, SRB_MESSAGE, message);
  if (offset > 0) 
    srbmsg.map = urlmapaddi(srbmsg.map, SRB_OFFSET, offset);
  
  rc = _mw_srbsendmessage(fd, &srbmsg);
  urlmapfree(srbmsg.map);
  return rc;
};

int _mw_srbsendterm(int fd, int grace)
{
  int rc;
  SRBmessage srbmsg;
  srbmsg.map = NULL;

  strncpy(srbmsg.command, SRB_TERM, MWMAXSVCNAME);
  if (grace == -1) {
    srbmsg.marker = SRB_NOTIFICATIONMARKER;    
  } else {
    srbmsg.marker = SRB_REQUESTMARKER;
    srbmsg.map = urlmapaddi(srbmsg.map, SRB_GRACE, grace);
  };
  rc = _mw_srbsendmessage(fd, &srbmsg);
  urlmapfree(srbmsg.map);
  return rc;
};

int _mw_srbsendinit(int fd, char * user, char * password, 
			   char * name, char * domain)
{
  int  rc;
  char * auth = SRB_AUTH_NONE;
  SRBmessage srbmsg;

  if (name == NULL) {
    return -EINVAL;
  };

  strncpy(srbmsg.command, SRB_INIT, MWMAXSVCNAME);
  srbmsg.marker = SRB_REQUESTMARKER;
  srbmsg.map = NULL;

  /* mandatory felt */
  srbmsg.map = urlmapadd(srbmsg.map, SRB_VERSION, SRBPROTOCOLVERSION);
  srbmsg.map = urlmapadd(srbmsg.map, SRB_TYPE, SRB_TYPE_CLIENT);
  srbmsg.map = urlmapadd(srbmsg.map, SRB_NAME, name);

  /* optional */
  if (user != NULL) {
    auth = SRB_AUTH_UNIX;
    srbmsg.map = urlmapadd(srbmsg.map, SRB_USER, user);
  };

  if (password != NULL)
    srbmsg.map = urlmapadd(srbmsg.map, SRB_PASSWORD, password);

  if (domain != NULL)
    srbmsg.map = urlmapadd(srbmsg.map, SRB_DOMAIN, domain);

  srbmsg.map = urlmapadd(srbmsg.map, SRB_AUTHENTICATION, auth);

  rc = _mw_srbsendmessage(fd, &srbmsg);
  urlmapfree(srbmsg.map);
  return rc;
};

int _mw_srbsendcall(int fd, int handle, char * svcname, char * data, int datalen, 
		    int flags)
{
  char hdlbuf[9];
  int rc;
  SRBmessage srbmsg;
  int noreply, multiple;
  float timeleft;

  noreply = flags & MWNOREPLY;
  mwlog(MWLOG_DEBUG1, "_mw_srbsendcall: begins");

  /* input validation done before in mw(a)call:mwclientapi.c */
  strncpy(srbmsg.command, SRB_SVCCALL, MWMAXSVCNAME);
  srbmsg.map = NULL;

  if (noreply) {
    srbmsg.marker = SRB_NOTIFICATIONMARKER;
  } else {
    srbmsg.marker = SRB_REQUESTMARKER;
    sprintf(hdlbuf, "%8.8X", (unsigned int) handle);
    srbmsg.map = urlmapnadd(srbmsg.map, SRB_HANDLE, hdlbuf, 8);
  };
  srbmsg.map = urlmapadd(srbmsg.map, SRB_SVCNAME, svcname);
  if (data != NULL) {
    if (datalen == 0) {
      datalen = strlen(data);
    };
    if (datalen > 3000) {
      mwlog(MWLOG_ERROR, "Large data not yet implemented, datalen %d > 3000", datalen);
      urlmapfree(srbmsg.map);
      return -ENOSYS;
    };
    srbmsg.map = urlmapnadd(srbmsg.map, SRB_DATA, data, datalen);
  };
  
  if ( _mw_deadline(NULL, &timeleft)) {
    srbmsg.map = urlmapaddi(srbmsg.map, SRB_SECTOLIVE, (int) timeleft);
  };

  srbmsg.map = urlmapaddi(srbmsg.map, SRB_HOPS, 0);
  
  rc = _mw_srbsendmessage(fd, &srbmsg);
  urlmapfree(srbmsg.map);
  mwlog(MWLOG_DEBUG1, "_mw_srbsendcall: returns normally with rc=%d", rc);
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
int _mw_srb_checksrbcall(int fd, SRBmessage * srbmsg) 
{
  int idx;

  idx = urlmapget(srbmsg->map, SRB_HANDLE);
  /* if a SVCCALL mesg don't have a handle, reject it */
  if (idx == -1) {
    mwlog(MWLOG_ERROR, "_mw_srb_checksrbcall: got a call reply without a handle");
    _mw_srbsendreject(fd, srbmsg, SRB_HANDLE, NULL, 
		      SRB_PROTO_FIELDMISSING);
    return 0;
  };
  
  idx = urlmapget(srbmsg->map, SRB_RETURNCODE);
  if (idx == -1) { 
    mwlog(MWLOG_ERROR, "_mw_srb_checksrbcall: RETURNCODE missing in SRBCALL: ");
    _mw_srbsendreject(fd, srbmsg, SRB_RETURNCODE, NULL, 
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
    mwlog(MWLOG_ERROR, "_mw_srb_checksrbcall: Illegal format of RETURNCODE in SRBCALL: \"%s\"(%d)",
	  srbmsg->map[idx].value, srbmsg->map[idx].valuelen);
    _mw_srbsendreject(fd, srbmsg, SRB_RETURNCODE, srbmsg->map[idx].value, 
		      SRB_PROTO_ILLEGALVALUE);
    return 0;
  };

  idx = urlmapget(srbmsg->map, SRB_APPLICATIONRC);
  if (idx == -1) { 
    mwlog(MWLOG_ERROR, "_mw_srb_checksrbcall: APPLICATIONRC missing in SRBCALL: ");
    _mw_srbsendreject(fd, srbmsg, SRB_APPLICATIONRC, NULL, 
		      SRB_PROTO_FIELDMISSING);
    return 0;
  } 

  return 1;
};
