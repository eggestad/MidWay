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
 * Revision 1.1  2000/07/20 18:49:59  eggestad
 * The SRB daemon
 *
 */

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/time.h>
#include <string.h>

#include <MidWay.h>
#include <version.h>
#include <SRBprotocol.h>

#include <urlencode.h>

#include "tcpserver.h"
#include "gateway.h"

static char messagebuffer[SRBMESSAGEMAXLEN];

/*static int srbrole = SRBNOTCONNECTED;*/

static void srbhello(int, char, char * );
static void srbterm(int, char, char * );
static void srbinit(int, char, char * );
static void srbsvccall(int, char, char * );
static void srbsvcdata(int, char, char * );


static void srbsendreject(int, char *, int, char *, char *, int );

static char * szGetReqField(int fd, char * message, urlmap * map, char * fieldname)
{
  int idx;
  idx = urlmapget(map, fieldname);
  if (idx != -1) {
    return map[idx].value;
  } 
  srbsendreject(fd, message,  -1, 
		fieldname, NULL, SRB_PROTO_FIELDMISSING);
  return NULL;
}

static char * szGetOptField(int fd, char * message, urlmap * map, char * fieldname)
{
  int idx;
  idx = urlmapget(map, fieldname);
  if (idx != -1) {
    return map[idx].value;
  } 
  return NULL;
}

static int iGetOptBinField(int fd, char * message, urlmap * map, 
			  char * fieldname, char ** value, int * valuelen)
{
  int idx;
  idx = urlmapget(map, fieldname);
  if (idx != -1) {
    *value = map[idx].value;
    *valuelen = map[idx].valuelen;
    return 1;
  } 
  *value = NULL;
  *valuelen = -1;
  return 0;
}

static int iGetReqField(int fd, char * message, urlmap * map, 
			char * fieldname, int *  value)
{
  int idx, rc;
  idx = urlmapget(map, fieldname);
  if (idx != -1) {
    rc = sscanf (map[idx].value, "%d", value);
    if (rc != 1) {
      srbsendreject(fd, message,  -1, 
		    map[idx].key, map[idx].value, SRB_PROTO_ILLEGALVALUE);
      return 0;
    };
    return 1;
    
  }
  srbsendreject(fd, message,  -1, 
		fieldname, NULL, SRB_PROTO_FIELDMISSING);
  return 0;
};

static int iGetOptField(int fd, char * message, urlmap * map, 
			char * fieldname, int *  value)
{
  int idx, rc;
  idx = urlmapget(map, fieldname);
  if (idx != -1) {
    rc = sscanf (map[idx].value, "%d", value);
    if (rc != 1) {
      srbsendreject(fd, message,  -1, 
		    map[idx].key, map[idx].value, SRB_PROTO_ILLEGALVALUE);
      return 0;
    };
    return 1;
  }
  return 1; // 0 signify error!
};

/**********************************************************************
 * Callbacks for receiving messages
 **********************************************************************/

static void srbhello(int fd, char marker, char * message)
{
  int len, idx = 0;
  struct timeval now;
  urlmap * map;
  char buffer[50];

  gettimeofday(&now, NULL);
   
  map = urlmapdecode(message+6);
  printf("Got a hello message\n");
  switch (marker) {
    
  case SRB_REQUESTMARKER:
    sprintf(buffer, "%d.%06d", now.tv_sec, now.tv_usec);
    map = urlmapadd(map, SRB_REMOTETIME, buffer);

    sprintf(messagebuffer, "%s%c", SRB_HELLO, SRB_RESPONSEMARKER);
    len = 6;
    len += urlmapnencode(messagebuffer+len, SRBMESSAGEMAXLEN-len, map);

    sendmessage(fd, messagebuffer, len);
    break;

  case SRB_RESPONSEMARKER:
     break;

  case SRB_NOTIFICATIONMARKER:  
  default:
    srbsendreject(fd, message, 6, NULL, NULL, SRB_PROTO_NOTREQUEST);
  }
  urlmapfree(map);
  return;
};

static void srbreject(int fd, char marker, char * message)
{

  mwlog(MWLOG_ERROR, "Got a reject message from %s \"%s\"", 
	tcpgetconnpeername(fd), message);
  return;
};

static void srbterm(int fd, char marker, char * message)
{
  urlmap * map;
  int idxGrace, i = 0;
  int grace = -1;
  int rc, idx;
  char * tmp;
  int id;
  
  map = urlmapdecode(message+5);

  switch (marker) {
    
  case SRB_REQUESTMARKER:
    iGetOptField(fd, message, map,  SRB_GRACE, &grace);

    mwlog(MWLOG_INFO, "Got a TERM request GRACE = %d from %s", 
	  grace, tcpgetconnpeername(fd));
    srbsendterm(fd, -1);
    break;
    
  case SRB_RESPONSEMARKER:
    
  case SRB_NOTIFICATIONMARKER:
    mwlog(MWLOG_INFO, "Got a TERM notification from %s, closing\n", 
	  tcpgetconnpeername(fd));
    
    tcpcloseconnection(fd);
  };

  urlmapfree(map);
};

static void srbcall(int fd, char marker, char * message)
{
  urlmap * map;
  int i = 0, fail = 0;
  int rc, idx;
  char * tmp;
  
  CLIENTID cltid = UNASSIGNED;
 
  char * svcname;
  char * data = NULL;
  int datachunks = -1;
  char * instance = NULL;
  char * domain = NULL;
  char * clientname = NULL;
  unsigned long handle = 0xffffffff;
  long flags = 0;
  int datalen = 0;
  int noreply = 0;
  int multiple = 0;
  int more = 0;
  int hops = 0;
  int maxhops = 30;
  int sectolive = -1;
  int returncode = 0;
  int appreturncode = 0;
  
  printf("Got a svccall message %s\n", message);

  /* I don't like having to hardcode the offset into the fields in the
     message like this, but is quite simple... */
  map = urlmapdecode(message + 8);

  tcpgetconninfo(fd, &cltid, NULL, NULL, NULL);
  if (cltid == UNASSIGNED) {
    mwlog(MWLOG_WARNING, "Got a SVCCALL before SRB INIT cltid=%d", cltid);
    srbsendcallreply(fd, map, NULL, 0, 0, SRB_PROTO_NOINIT, 0);
    return;
  };

  /* first we get the field that are identical on requests as well as replies. */
  if (!(svcname =     szGetReqField(fd, message, map, SRB_SVCNAME))) {
    urlmapfree(map);
    return;
  };
  if ( strlen(svcname) > MWMAXSVCNAME) {
    srbsendreject(fd, message,  -1, 
		  SRB_SVCNAME, tmp, SRB_PROTO_ILLEGALVALUE);
    urlmapfree(map);
    return;
  };

  if (!(tmp =         szGetReqField(fd, message, map, SRB_HANDLE))) {
    urlmapfree(map);
    return;
  };

  if ( (tmp == NULL) || (sscanf(tmp,"%x", &handle) != 1) ) {
    srbsendreject(fd, message,  -1, 
		  SRB_HANDLE, tmp, SRB_PROTO_ILLEGALVALUE);
    urlmapfree(map);
    return;
  };
  if (!(iGetReqField(fd, message, map,  SRB_HOPS, &hops))) {
    urlmapfree(map);
    return;
  };


  iGetOptBinField(fd, message, map, SRB_DATA, &data, &datalen);
  if (data == NULL) datalen = 0;
  
  iGetOptField(fd, message, map, SRB_DATACHUNKS, &datachunks);
  instance =    szGetOptField(fd, message, map, SRB_INSTANCE);
  iGetOptField(fd, message, map,  SRB_SECTOLIVE, &sectolive);

  switch (marker) {
    
  case SRB_NOTIFICATIONMARKER:

    /* noreply  */
    mwlog(MWLOG_INFO, "Got a SVCCALL notification from %s, NOREPLY\n", 
	  tcpgetconnpeername(fd));
    flags |= MWNOREPLY;

  case SRB_REQUESTMARKER:

    mwlog(MWLOG_INFO, "Got a SVCCALL service=%s from %s\n", 
	  svcname, tcpgetconnpeername(fd));

    domain =      szGetOptField(fd, message, map, SRB_DOMAIN);
    clientname =  szGetOptField(fd, message, map, SRB_CLIENTNAME);

    tmp =         szGetOptField(fd, message, map, SRB_NOREPLY);
    if (tmp) 
      if  (strcasecmp(tmp, SRB_YES) == 0) noreply = 1;
      else if (strcasecmp(tmp, SRB_NO) == 0) noreply = 0;
      else {
	srbsendreject(fd, message,  -1, 
		      SRB_NOREPLY, tmp, SRB_PROTO_ILLEGALVALUE);
	break;
      };
    

    tmp =         szGetOptField(fd, message, map, SRB_MULTIPLE);
    if (tmp)
      if (strcasecmp(tmp, SRB_YES) == 0) multiple = 1;
      else if (strcasecmp(tmp, SRB_NO) == 0) multiple = 0;
      else {
	srbsendreject(fd, message,  -1, 
		      SRB_MULTIPLE, tmp, SRB_PROTO_ILLEGALVALUE);
	break;
      };

    iGetOptField(fd, message, map,  SRB_MAXHOPS, &maxhops);

    /* 
       This is the special case where we don't have any chunks.
       we can do the call directly.
    */
    if ((datachunks == -1) ) {

      mwlog(MWLOG_DEBUG, "SVCCALL was complete doing _mwacallipc");      
      _mw_set_my_clientid(cltid);

      /* we must lock since it happens that the server replies before
         we do storeSetIpcCall() */

      storeLockCall();
      storePushCall(cltid, handle, fd, map);
      rc = _mwacallipc (svcname, data, datalen, NULL,  flags);
      
      _mw_set_my_clientid(UNASSIGNED);
      
      if (rc > 0) {
	 mwlog(MWLOG_DEBUG, "_mwacallipc succeeded");  
	 if ( !(flags & MWNOREPLY)) {
	  mwlog(MWLOG_DEBUG, 
		"Storing call fd=%d nethandle=%u clientid=%d ipchandle=%d",
		fd, handle, cltid&MWINDEXMASK, rc);
	  
	  rc = storeSetIPCHandle(cltid, handle, fd, rc);
	  storeUnLockCall();

	  mwlog(MWLOG_DEBUG, "storeSetIPCHandle returned %d", rc);

	  mwlog(MWLOG_DEBUG, "srbsvccall returns good");  
	  return ; /* NB PREVENT urlmapfree() */
	};
      } else {
	storeUnLockCall();
	mwlog(MWLOG_DEBUG, "_mwacallipc failed with %d ", rc);   
	srbsendcallreply(fd, map, NULL, 0, 0, errno2srbrc(rc), 0);
	map = NULL;
      };
    };
    break;
    
  case SRB_RESPONSEMARKER:
    if (!(tmp =         szGetOptField(fd, message, map, SRB_MORE))) break;
    if (strcasecmp(tmp, SRB_YES) == 0) more = 1;
    else if (strcasecmp(tmp, SRB_NO) == 0) more = 0;
    else {
      srbsendreject(fd, message,  -1, 
		    SRB_MORE, tmp, SRB_PROTO_ILLEGALVALUE);
      break;
    };

    mwlog(MWLOG_INFO, "Got a SVCCALL reply from %s, ignoring\n", 
	  tcpgetconnpeername(fd));
    
    urlmapfree(map);
  };

  
};

static void srbinit(int fd, char marker, char * message)
{
  urlmap * map;
  int reverse = 0, rejects = 1, auth = 0, role;
  char * type = NULL, * version= NULL, * tmp = NULL;
  char * domain = NULL, * name = NULL, * user = NULL, * passwd = NULL; 
  char * agent = NULL, * agentver = NULL, * os = NULL;
  int idx, rc, connrole, id;
  char * szptr, * peername; 
  CLIENTID cid;

  map = urlmapdecode(message+9);

  tcpgetconninfo(fd, &cid, &connrole, NULL, NULL);    

  if (cid != UNASSIGNED) {
    srbsendinitreply(fd, map, SRB_PROTO_ISCONNECTED, NULL);
    return;
  };
  switch (marker) {
    
  case SRB_REQUESTMARKER:
  /* VERSION - REQ */  

    if (!(version = szGetReqField(fd, message, map, SRB_VERSION))) break;
    if ( strcmp(SRBPROTOCOLVERSION, version) != 0) {
      srbsendinitreply(fd, map, SRB_PROTO_VERNOTSUPP, SRB_VERSION);
      break;
    };
    
    /* TYPE - REQ */
    if (!(type = szGetReqField(fd, message, map, SRB_TYPE))) break;

    if (strcasecmp(type, SRB_CLIENT) == 0) role = SRB_ROLE_CLIENT;
    else if (strcasecmp(type, SRB_GATEWAY) == 0) role = SRB_ROLE_GATEWAY;
    else {
      srbsendinitreply(fd, map, SRB_PROTO_ILLEGALVALUE, SRB_TYPE);
      break;
    };

    /* if requested type is not expected, decline */
    if (!(role & connrole)) {
      if (role == SRB_ROLE_CLIENT)
	srbsendinitreply(fd, map, SRB_PROTO_NOCLIENTS, SRB_TYPE);
      else 
	srbsendinitreply(fd, map, SRB_PROTO_NOGATEWAYS, SRB_TYPE);
      break;;
    };
    
    /* NAME - REQ */
    if (!(name = szGetReqField(fd, message, map, SRB_NAME))) break; 

    /* DOMAIN - OPT */
    domain = szGetOptField(fd, message, map, SRB_DOMAIN);
    
    /* REVERSE - OPT */
    tmp = szGetOptField(fd, message, map, SRB_REVERSE);
    if (tmp != NULL) {
      if (strcasecmp(tmp, SRB_YES) == 0) reverse = 1;
      else if (strcasecmp(tmp, SRB_NO) == 0) reverse = 0;
      else {
	srbsendinitreply(fd, map, SRB_PROTO_ILLEGALVALUE, SRB_REVERSE);
	break;
      };
    };
    /* REJECT - OPT */
    tmp = szGetOptField(fd, message, map, SRB_REJECTS);
    if (tmp != NULL) {
      if (strcasecmp(tmp, SRB_YES) == 0) rejects = 1;
      else if (strcasecmp(tmp, SRB_NO) == 0) rejects = 0;
      else {
	srbsendinitreply(fd, map, SRB_PROTO_ILLEGALVALUE, SRB_REJECTS);
	break;
      };
    }
    /* AUTHENTICATION - OPT */
    tmp = szGetOptField(fd, message, map, SRB_AUTHENTICATION);
    if (tmp != NULL) {
      if (strcasecmp(tmp, SRB_AUTH_NONE) == 0) ;
      else if (strcasecmp(tmp, SRB_AUTH_UNIX) == 0) ;
      else if (strcasecmp(tmp, SRB_AUTH_X509) == 0) ;
      else {
	srbsendinitreply(fd, map, SRB_PROTO_ILLEGALVALUE, SRB_AUTHENTICATION);
	break;
      };
    };
    /* USER, PASSWORD, AGENT, AGENTVERSION, OS - OPT */
    user     = szGetOptField(fd, message, map, SRB_USER);
    passwd   = szGetOptField(fd, message, map, SRB_PASSWORD);
    agent    = szGetOptField(fd, message, map, SRB_AGENT);
    agentver = szGetOptField(fd, message, map, SRB_AGENTVERSION);
    os       = szGetOptField(fd, message, map, SRB_OS);
    
    /* Now we we have a valid request, all thats left if to attache
       the client or register the gateway */
    mwlog(MWLOG_DEBUG, "SRB INIT correctly formated role=%d ", role); 
    peername = tcpgetconnpeername(fd);

    /* we use user and name after gwattach() but anytime after that
       the other thread may have gotten back from mwd and done
       urlmapfre() */

    if (user)
      user = strdup(user);
    if (name)
      name = strdup(name);

    /* if client */
    if (role == SRB_ROLE_CLIENT) {
      rc = gwattachclient(-1, fd, name, user, passwd, map);
      if (rc >= 0) {
 	mwlog(MWLOG_INFO, "Client %s %s%s ID=%#u connected from %s", 
	      name, user?"Username=":"", user?user:"",
	      rc , peername);
	rc = rc | MWCLIENTMASK;
	tcpsetconninfo(fd, NULL, &role, NULL, NULL);
      } else {
	rc = errno2srbrc(-rc);
	szptr = strreason(rc);
 	mwlog(MWLOG_INFO, "Client %s %s%s from %s was rejected: ", 
	      name, user?"Username=":"", user?user:"",
	      peername);
	rc = errno2srbrc(-rc);
	srbsendinitreply(fd, map, rc, NULL);
      }
      free(user);
    } else {
      /* if (type == SRB_ROLE_GATEWAY) */
      free(user);
      srbsendinitreply(fd, map, SRB_PROTO_NOGATEWAYS, NULL);
      break;
    };
    
  case SRB_RESPONSEMARKER:
    /* just in case of initiating gateway. NYI */
    break;
      
  case SRB_NOTIFICATIONMARKER:  
  default:
    srbsendinitreply(fd, map, SRB_PROTO_NOTREQUEST, NULL);
  };
  
  return;
};


/**********************************************************************
 * entru function for receiving messages, basic syntax check and 
 * dispatch to API above.
 **********************************************************************/

  
int srbDoMessage(int fd, char * message)
{
  int commandlen;
  int messagetype;
  char * ptr;

  if ( (ptr = strchr(message, SRB_REQUESTMARKER)) == NULL)
    if ( (ptr = strchr(message, SRB_RESPONSEMARKER)) == NULL)
      if ( (ptr = strchr(message, SRB_NOTIFICATIONMARKER)) == NULL) {
	srbsendreject(fd, message, 0, NULL, NULL, SRB_PROTO_FORMAT);
	return -1;
      }
  commandlen = ptr - message;

  mwlog(MWLOG_DEBUG, "srbDoMessage: command=%*.*s marker=%c", 
	commandlen, commandlen, message, *ptr);
  switch(commandlen) {
  case 4:
    if ( strncasecmp(message, SRB_TERM, commandlen) == 0) {
      srbterm(fd, *ptr, message);
      return ;
    };
    break;
  case 5:
    if ( strncasecmp(message, SRB_HELLO, commandlen) == 0) {
      srbhello(fd, *ptr, message);
      return;
    }
    break;
  case 6:
    if ( strncasecmp(message, SRB_REJECT, commandlen) == 0) {
      srbreject(fd, *ptr, message);
      return;
    }
    break;
  case 7:
    if ( strncasecmp(message, SRB_SVCCALL, commandlen) == 0) {
      srbcall(fd, *ptr, message);
      return;
    }
  case 8:
     if ( strncasecmp(message, SRB_INIT, commandlen) == 0) {
      srbinit(fd, *ptr, message);
      return;
     }
  case 9:

  };

  srbsendreject(fd, message, 0, NULL, NULL, SRB_PROTO_FORMAT);
  return SRB_PROTO_FORMAT;
};


/**********************************************************************
 * API for Sending messages
 **********************************************************************/

void srbsendreject(int fd, char * message, int offset, 
			  char * causefield, char * causevalue, 
			  int rc)
{
  int len;
  urlmap * map = NULL;

  map = urlmapaddi(map, SRB_REASONCODE, rc);
  map = urlmapadd(map, SRB_REASON, strreason(rc));

  len = sprintf(messagebuffer, "%s%c", SRB_REJECT, SRB_NOTIFICATIONMARKER);

  if (offset > -1) 
    map = urlmapaddi(map, SRB_OFFSET, offset);
  
  map = urlmapadd(map, SRB_MESSAGE, message);

  if (causefield != NULL) 
    map = urlmapadd(map, SRB_CAUSEFIELD, causefield);

  if (causevalue != NULL) 
    map = urlmapadd(map, SRB_CAUSEVALUE, causevalue);

  len += urlmapnencode(messagebuffer+len, SRBMESSAGEMAXLEN-len, map);
  urlmapfree(map);

  sendmessage(fd, messagebuffer, len);
};

void srbsendinitreply(int fd, urlmap * map, int rc, char * field)
{
  char rcode[4];
  int len, ml;

  if (map == NULL) return;

  len = sprintf(messagebuffer, "%s%c", SRB_INIT, SRB_RESPONSEMARKER);
  
  map = urlmapaddi(map, SRB_RETURNCODE, rc);
  if (rc != 0) {
    map = urlmapadd(map, SRB_CAUSE, strreason(rc));
    if (field != NULL)
      map = urlmapadd(map, SRB_CAUSEFIELD, field);
  };
  
  ml = urlmapnencode(messagebuffer+len, SRBMESSAGEMAXLEN-len, map);

  if (ml == -1) {
    mwlog(MWLOG_ERROR, "This cant happen, reply to SRB INIT too long: %s\n", 
	  messagebuffer);
    return;
  };

  len += ml;
  sendmessage (fd, messagebuffer, len);
  urlmapfree(map);
  return;
};

void srbsendcallreply(int fd, urlmap *map, char * data, int len, 
			     int apprcode, int rcode, int flags)
{
  urlmap * m;
  int rc;

  if (map == NULL) return;

  /* first update th edata field, or remove as appropreate */
  if ( (data != NULL) && (len > 0) ) {
    rc = urlmapnset(map, SRB_DATA, data, len);
    if (rc == -1)
      map = urlmapnadd(map, SRB_DATA, data, len);
  } else {
    urlmapdel(map, SRB_DATA);
  };

  /* it really is the senders fault if it had set RETURNCODE fields when sending
   * but just to be nice...*/
  urlmapdel(map, SRB_APPLICATIONRC);
  urlmapdel(map, SRB_RETURNCODE);
  /* returncodes, add */
  map = urlmapaddi(map, SRB_APPLICATIONRC, apprcode);
  map = urlmapaddi(map, SRB_RETURNCODE, rcode);

  urlmapdel(map, SRB_MORE);
  if (flags & MWMORE) 
    map = urlmapadd(map, SRB_MORE, SRB_YES);
  else
    map = urlmapadd(map, SRB_MORE, SRB_NO);

  urlmapdel(map, SRB_INSTANCE);
  urlmapdel(map, SRB_CLIENTNAME);
  urlmapdel(map, SRB_NOREPLY);
  urlmapdel(map, SRB_MULTIPLE);
  urlmapdel(map, SRB_MAXHOPS);
  
  len = sprintf(messagebuffer, "%s%c", SRB_SVCCALL, SRB_RESPONSEMARKER);
  len += urlmapnencode(messagebuffer+len, SRBMESSAGEMAXLEN-len, map);
  
  sendmessage(fd, messagebuffer, len);
  urlmapfree(map);
};

void srbsendterm(int fd, int grace)
{
  int len;

  if (grace == -1) {
    len = sprintf(messagebuffer, "%s%c", SRB_TERM, SRB_NOTIFICATIONMARKER);
  }else 
    len = sprintf(messagebuffer, "%s%c%s=%d", 
		  SRB_TERM, SRB_REQUESTMARKER, SRB_GRACE, grace);

  sendmessage(fd, messagebuffer, len);

#ifdef SRBSERVER
  if (grace <= 0)
    srvcloseconnection(fd);
#endif
};

void srbsendgreeting(int fd)
{
  int len;
  urlmap * map = NULL;

  len = sprintf(messagebuffer, "%s%c", SRB_READY, SRB_NOTIFICATIONMARKER);
  map = urlmapadd(map, SRB_VERSION, SRBPROTOCOLVERSION);
  map = urlmapadd(map, SRB_AGENT, "MidWay");
  map = urlmapadd(map, SRB_AGENTVERSION, (char *) mwversion());
  len += urlmapnencode (messagebuffer+len, SRBMESSAGEMAXLEN, map);
  sendmessage(fd, messagebuffer, len);
  urlmapfree(map);
};
