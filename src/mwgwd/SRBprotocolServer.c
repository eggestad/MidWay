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
 * Revision 1.3  2000/09/24 14:10:42  eggestad
 * Changed a few mwlog()  messages to be DEBUG
 *
 * Revision 1.2  2000/09/21 18:57:55  eggestad
 * - Bug fix: had a lot of \n in the end of mwlog()
 * - Bug fix: srbmsg.map was not inited to NULL many placed, cased core dumps.
 *
 * Revision 1.1  2000/08/31 19:40:36  eggestad
 * We have quite som changes to mwgwd with the implementation of the SRB client
 * only API in the lib.
 * This is now the addition to the lib for the server side API.
 *
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

#include "SRBprotocolServer.h"
#include "tcpserver.h"
#include "gateway.h"


/*static int srbrole = SRBNOTCONNECTED;*/

static void srbhello(int, SRBmessage * );
static void srbterm(int, SRBmessage * );
static void srbinit(int, SRBmessage * );
static void srbsvccall(int, SRBmessage * );
static void srbsvcdata(int, SRBmessage * );

/* is it even used anymore ??? */
char * srbmessageencode(SRBmessage * srbmsg)
{
  int len;
  static char message[SRBMESSAGEMAXLEN];

  len = sprintf(message, "%s%c", srbmsg->command, srbmsg->marker);
  urlmapnencode(message+len, SRBMESSAGEMAXLEN-len, srbmsg->map);
  return message;
};

/* help functions to retrive fields in messages. */
static char * szGetReqField(int fd, SRBmessage * srbmsg, char * fieldname)
{
  int idx;
  idx = urlmapget(srbmsg->map, fieldname);
  mwlog(MWLOG_DEBUG2, "szGetReqField: index of %s is %d, errno=%d", 
	fieldname, idx, errno);
  if (idx != -1) {
    return srbmsg->map[idx].value;
  } 
  _mw_srbsendreject(fd, srbmsg, fieldname, NULL, SRB_PROTO_FIELDMISSING);
  return NULL;
}

static char * szGetOptField(int fd, SRBmessage * srbmsg, char * fieldname)
{
  int idx;
  idx = urlmapget(srbmsg->map, fieldname);
  mwlog(MWLOG_DEBUG2, "szGetOptField: index of %s is %d errno=%d", 
	fieldname, idx, errno);
  if (idx != -1) {
    return srbmsg->map[idx].value;
  } 
  return NULL;
}

static int iGetOptBinField(int fd, SRBmessage * srbmsg, 
			   char * fieldname, char ** value, int * valuelen)
{
  int idx;
  idx = urlmapget(srbmsg->map, fieldname);
  if (idx != -1) {
    *value = srbmsg->map[idx].value;
    *valuelen = srbmsg->map[idx].valuelen;
    return 1;
  } 
  *value = NULL;
  *valuelen = -1;
  return 0;
}

static int iGetReqField(int fd, SRBmessage * srbmsg, 
			char * fieldname, int *  value)
{
  int idx, rc;
  idx = urlmapget(srbmsg->map, fieldname);
  if (idx != -1) {
    rc = sscanf (srbmsg->map[idx].value, "%d", value);
    if (rc != 1) {
      _mw_srbsendreject(fd, srbmsg, srbmsg->map[idx].key, srbmsg->map[idx].value, 
			SRB_PROTO_ILLEGALVALUE);
      return 0;
    };
    return 1;
    
  }
  _mw_srbsendreject(fd, srbmsg, fieldname, NULL, SRB_PROTO_FIELDMISSING);
  return 0;
};

static int iGetOptField(int fd, SRBmessage * srbmsg,
			char * fieldname, int *  value)
{
  int idx, rc;
  idx = urlmapget(srbmsg->map, fieldname);
  if (idx != -1) {
    rc = sscanf (srbmsg->map[idx].value, "%d", value);
    if (rc != 1) {
      _mw_srbsendreject(fd, srbmsg, 
			srbmsg->map[idx].key, srbmsg->map[idx].value, 
			SRB_PROTO_ILLEGALVALUE);
      return 0;
    };
    return 1;
  }
  return 1; // 0 signify error!
};

/**********************************************************************
 * Callbacks for receiving messages
 **********************************************************************/

static void srbhello(int fd, SRBmessage * srbmsg)
{
  int len, idx = 0;
  struct timeval now;
  char buffer[50];

  gettimeofday(&now, NULL);
   
  printf("Got a hello message\n");
  switch (srbmsg->marker) {
    
  case SRB_REQUESTMARKER:
    sprintf(buffer, "%d.%06d", now.tv_sec, now.tv_usec);
    srbmsg->map = urlmapadd(srbmsg->map, SRB_REMOTETIME, buffer);

    srbmsg->marker = SRB_RESPONSEMARKER;
    _mw_srbsendmessage(fd, srbmsg);
    break;

  case SRB_RESPONSEMARKER:
     break;

  case SRB_NOTIFICATIONMARKER:  
  default:
    _mw_srbsendreject(fd, srbmsg, NULL, NULL, SRB_PROTO_NOTREQUEST);
  }
  return;
};

static void srbreject(int fd, SRBmessage * srbmsg)
{

  mwlog(MWLOG_ERROR, "Got a reject message from %s \"%s\"", 
	tcpgetconnpeername(fd), srbmessageencode(srbmsg));
  return;
};

static void srbterm(int fd, SRBmessage * srbmsg)
{
  int idxGrace, i = 0;
  int grace = -1;
  int rc, idx;
  char * tmp;
  int id;
  
  switch (srbmsg->marker) {
    
  case SRB_REQUESTMARKER:
    iGetOptField(fd, srbmsg, SRB_GRACE, &grace);

    mwlog(MWLOG_INFO, "Got a TERM request GRACE = %d from %s", 
	  grace, tcpgetconnpeername(fd));
    _mw_srbsendterm(fd, -1);
    break;
    
  case SRB_RESPONSEMARKER:
    
  case SRB_NOTIFICATIONMARKER:
    mwlog(MWLOG_INFO, "Got a TERM notification from %s, closing", 
	  tcpgetconnpeername(fd));
    
    tcpcloseconnection(fd);
  };

};

static void srbcall(int fd, SRBmessage * srbmsg)
{
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

  /* I don't like having to hardcode the offset into the fields in the
     message like this, but is quite simple... */
  tcpgetconninfo(fd, &cltid, NULL, NULL, NULL);
  if (cltid == UNASSIGNED) {
    mwlog(MWLOG_WARNING, "Got a SVCCALL before SRB INIT cltid=%d", cltid&MWINDEXMASK);
    _mw_srbsendcallreply(fd, srbmsg, NULL, 0, 0, SRB_PROTO_NOINIT, 0);
    return;
  };

  mwlog(MWLOG_DEBUG, "srbcall: beginning SVCCALL from cltid=%d", cltid&MWINDEXMASK);

  /* first we get the field that are identical on requests as well as
     replies. */
  if (!(svcname =     szGetReqField(fd, srbmsg, SRB_SVCNAME))) {
    return;
  };
  if ( strlen(svcname) > MWMAXSVCNAME) {
    _mw_srbsendreject(fd, srbmsg, SRB_SVCNAME, tmp, SRB_PROTO_ILLEGALVALUE);
    return;
  };

  
  if (!(iGetReqField(fd, srbmsg,  SRB_HOPS, &hops))) {
    return;
  };


  iGetOptBinField(fd, srbmsg, SRB_DATA, &data, &datalen);
  if (data == NULL) datalen = 0;
  
  iGetOptField(fd, srbmsg, SRB_DATACHUNKS, &datachunks);
  instance =    szGetOptField(fd, srbmsg, SRB_INSTANCE);
  iGetOptField(fd, srbmsg,  SRB_SECTOLIVE, &sectolive);

  switch (srbmsg->marker) {
    
  case SRB_NOTIFICATIONMARKER:
  case SRB_REQUESTMARKER:
    
    if (srbmsg->marker == SRB_NOTIFICATIONMARKER) {
      /* noreply  */
      mwlog(MWLOG_DEBUG, "Got a SVCCALL notification from %s, NOREPLY", 
	    tcpgetconnpeername(fd));
      flags |= MWNOREPLY;
      noreply = 1;
    }
    
    if (srbmsg->marker == SRB_REQUESTMARKER) {
      mwlog(MWLOG_DEBUG, "Got a SVCCALL service=%s from %s", 
	    svcname, tcpgetconnpeername(fd));

      /* handle is optional iff noreply */
      if (!(tmp =         szGetReqField(fd, srbmsg, SRB_HANDLE))) {
	return;
      };
      if ( (tmp == NULL) || (sscanf(tmp,"%x", &handle) != 1) ) {
	_mw_srbsendreject(fd, srbmsg, SRB_HANDLE, tmp, SRB_PROTO_ILLEGALVALUE);
	return;
      };
    };

    domain =      szGetOptField(fd, srbmsg, SRB_DOMAIN);
    clientname =  szGetOptField(fd, srbmsg, SRB_CLIENTNAME);

    tmp =         szGetOptField(fd, srbmsg, SRB_MULTIPLE);
    if (tmp)
      if (strcasecmp(tmp, SRB_YES) == 0) multiple = 1;
      else if (strcasecmp(tmp, SRB_NO) == 0) multiple = 0;
      else {
	_mw_srbsendreject(fd, srbmsg,
		      SRB_MULTIPLE, tmp, SRB_PROTO_ILLEGALVALUE);
	break;
      };

    iGetOptField(fd, srbmsg,  SRB_MAXHOPS, &maxhops);

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
      rc = _mwacallipc (svcname, data, datalen, NULL,  flags);

      _mw_set_my_clientid(UNASSIGNED);
      
      if (rc > 0) {
	storePushCall(cltid, handle, fd, srbmsg->map);
	srbmsg->map = NULL;
	mwlog(MWLOG_DEBUG, "_mwacallipc succeeded");  
	if ( ! noreply) {
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
	mwlog(MWLOG_DEBUG, "_mwacallipc failed with %d ", rc);   
	_mw_srbsendcallreply(fd, srbmsg, NULL, 0, 0, _mw_errno2srbrc(rc), 0);
      };
      storeUnLockCall();
    };
    break;
    
  case SRB_RESPONSEMARKER:
    if (!(tmp =         szGetOptField(fd, srbmsg, SRB_MORE))) break;
    if (strcasecmp(tmp, SRB_YES) == 0) more = 1;
    else if (strcasecmp(tmp, SRB_NO) == 0) more = 0;
    else {
      _mw_srbsendreject(fd, srbmsg, SRB_MORE, tmp, SRB_PROTO_ILLEGALVALUE);
      break;
    };

    mwlog(MWLOG_DEBUG, "Got a SVCCALL reply from %s, ignoring", 
	  tcpgetconnpeername(fd));
    
  };

  
};

static void srbinit(int fd, SRBmessage * srbmsg)
{
  int reverse = 0, rejects = 1, auth = 0, role;
  char * type = NULL, * version= NULL, * tmp = NULL;
  char * domain = NULL, * name = NULL, * user = NULL, * passwd = NULL; 
  char * agent = NULL, * agentver = NULL, * os = NULL;
  int idx, rc, connrole, id;
  char * szptr, * peername; 
  CLIENTID cid;

  if (srbmsg->map == NULL) {
    _mw_srbsendreject (fd, srbmsg, NULL, NULL, SRB_PROTO_FORMAT);
    return;
  };
  tcpgetconninfo(fd, &cid, &connrole, NULL, NULL);    

  if (cid != UNASSIGNED) {
    _mw_srbsendinitreply(fd, srbmsg, SRB_PROTO_ISCONNECTED, NULL);
    return;
  };
  switch (srbmsg->marker) {
    
  case SRB_REQUESTMARKER:
  /* VERSION - REQ */  

    if (!(version = szGetReqField(fd, srbmsg, SRB_VERSION))) break;
    if ( strcasecmp(SRBPROTOCOLVERSION, version) != 0) {
      _mw_srbsendinitreply(fd, srbmsg, SRB_PROTO_VERNOTSUPP, SRB_VERSION);
      break;
    };
    
    /* TYPE - REQ */
    if (!(type = szGetReqField(fd, srbmsg, SRB_TYPE))) break;

    if (strcasecmp(type, SRB_TYPE_CLIENT) == 0) role = SRB_ROLE_CLIENT;
    else if (strcasecmp(type, SRB_TYPE_GATEWAY) == 0) role = SRB_ROLE_GATEWAY;
    else {
      _mw_srbsendinitreply(fd, srbmsg, SRB_PROTO_ILLEGALVALUE, SRB_TYPE);
      break;
    };

    /* if requested type is not expected, decline */
    if (!(role & connrole)) {
      if (role == SRB_ROLE_CLIENT)
	_mw_srbsendinitreply(fd, srbmsg, SRB_PROTO_NOCLIENTS, SRB_TYPE);
      else 
	_mw_srbsendinitreply(fd, srbmsg, SRB_PROTO_NOGATEWAYS, SRB_TYPE);
      break;;
    };
    
    /* NAME - REQ */
    if (!(name = szGetReqField(fd, srbmsg, SRB_NAME))) break; 

    /* DOMAIN - OPT */
    domain = szGetOptField(fd, srbmsg, SRB_DOMAIN);
    
    /* REVERSE - OPT */
    tmp = szGetOptField(fd, srbmsg, SRB_REVERSE);
    if (tmp != NULL) {
      if (strcasecmp(tmp, SRB_YES) == 0) reverse = 1;
      else if (strcasecmp(tmp, SRB_NO) == 0) reverse = 0;
      else {
	_mw_srbsendinitreply(fd, srbmsg, SRB_PROTO_ILLEGALVALUE, SRB_REVERSE);
	break;
      };
    };
    /* REJECT - OPT */
    tmp = szGetOptField(fd, srbmsg, SRB_REJECTS);
    if (tmp != NULL) {
      if (strcasecmp(tmp, SRB_YES) == 0) rejects = 1;
      else if (strcasecmp(tmp, SRB_NO) == 0) rejects = 0;
      else {
	_mw_srbsendinitreply(fd, srbmsg, SRB_PROTO_ILLEGALVALUE, SRB_REJECTS);
	break;
      };
    }
    /* AUTHENTICATION - OPT */
    tmp = szGetOptField(fd, srbmsg, SRB_AUTHENTICATION);
    if (tmp != NULL) {
      if (strcasecmp(tmp, SRB_AUTH_NONE) == 0) ;
      else if (strcasecmp(tmp, SRB_AUTH_UNIX) == 0) ;
      else if (strcasecmp(tmp, SRB_AUTH_X509) == 0) ;
      else {
	_mw_srbsendinitreply(fd, srbmsg, SRB_PROTO_ILLEGALVALUE, SRB_AUTHENTICATION);
	break;
      };
    };
    /* USER, PASSWORD, AGENT, AGENTVERSION, OS - OPT */
    user     = szGetOptField(fd, srbmsg, SRB_USER);
    passwd   = szGetOptField(fd, srbmsg, SRB_PASSWORD);
    agent    = szGetOptField(fd, srbmsg, SRB_AGENT);
    agentver = szGetOptField(fd, srbmsg, SRB_AGENTVERSION);
    os       = szGetOptField(fd, srbmsg, SRB_OS);
    
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
      rc = gwattachclient(-1, fd, name, user, passwd, srbmsg->map);
      if (rc >= 0) {
 	srbmsg->map = NULL;
	mwlog(MWLOG_INFO, "Client %s %s%s ID=%#u connected from %s", 
	      name, user?"Username=":"", user?user:"",
	      rc , peername);
	rc = rc | MWCLIENTMASK;
	tcpsetconninfo(fd, NULL, &role, NULL, NULL);
      } else {
	rc = _mw_errno2srbrc(-rc);
	szptr = _mw_srb_reason(rc);
 	mwlog(MWLOG_INFO, "Client %s %s%s from %s was rejected: ", 
	      name, user?"Username=":"", user?user:"",
	      peername);
	rc = _mw_errno2srbrc(-rc);
	_mw_srbsendinitreply(fd, srbmsg, rc, NULL);
      }
      free(user);
    } else {
      /* if (type == SRB_ROLE_GATEWAY) */
      free(user);
      _mw_srbsendinitreply(fd, srbmsg, SRB_PROTO_NOGATEWAYS, NULL);
      break;
    };
    
  case SRB_RESPONSEMARKER:
    /* just in case of initiating gateway. NYI */
    break;
      
  case SRB_NOTIFICATIONMARKER:  
  default:
    _mw_srbsendinitreply(fd, srbmsg, SRB_PROTO_NOTREQUEST, NULL);
  };
  
  return;
};


/**********************************************************************
 * entry function for receiving messages, basic syntax check and 
 * dispatch to API above.
 **********************************************************************/

  
int srbDoMessage(int fd, char * message)
{
  int commandlen;
  int messagetype;
  char * ptr;
  SRBmessage srbmsg;
  
  if ( (ptr = strchr(message, SRB_REQUESTMARKER)) == NULL)
    if ( (ptr = strchr(message, SRB_RESPONSEMARKER)) == NULL)
      if ( (ptr = strchr(message, SRB_NOTIFICATIONMARKER)) == NULL) {
	if ( (ptr = strchr(message, SRB_REJECTMARKER)) != NULL) {
	  return 0;
	} else { 
	  _mw_srbsendreject_sz(fd, message, -1);
	  return -1;
	}
      }
  commandlen = ptr - message;
  if (commandlen > 32) {
    _mw_srbsendreject_sz(fd, message, commandlen);
    return -1;
  };
  strncpy(srbmsg.command, message, commandlen);
  srbmsg.command[commandlen] = '\0';
  srbmsg.marker = *ptr;
  srbmsg.map = urlmapdecode(message+commandlen+1);
  
  mwlog(MWLOG_DEBUG2, "srbDoMessage: command=%*.*s marker=%c", 
	commandlen, commandlen, message, *ptr);
  mwlog(MWLOG_DEBUG, "srbDoMessage: command=%s marker=%c", 
	srbmsg.command, srbmsg.marker);

  switch(commandlen) {
  case 4:
    if ( strcasecmp(srbmsg.command, SRB_TERM) == 0) {
      srbterm(fd, &srbmsg);
      break ;
    };
    break;
  case 5:
    if ( strcasecmp(srbmsg.command, SRB_HELLO) == 0) {
      srbhello(fd, &srbmsg);
      break;
    }
    break;
  case 6:
    if ( strcasecmp(srbmsg.command, SRB_REJECT) == 0) {
      srbreject(fd, &srbmsg);
      break;
    }
    break;
  case 7:
    if ( strcasecmp(srbmsg.command, SRB_SVCCALL) == 0) {
      srbcall(fd, &srbmsg);
      break;
    }
  case 8:
    if ( strcasecmp(srbmsg.command, SRB_INIT) == 0) {
      srbinit(fd, &srbmsg);
      break;
    }
  case 9:
    
  default:
    _mw_srbsendreject_sz(fd, message, 0);
  };
  
  if (srbmsg.map != NULL)
    urlmapfree(srbmsg.map);

  return 0;
};

/**********************************************************************
 * API for sending messages only servers shall send
 **********************************************************************/

int _mw_srbsendinitreply(int fd, SRBmessage * srbmsg, int rcode, char * field)
{
  int  rc;

  if (srbmsg == NULL) return -EINVAL;
  if (srbmsg->map == NULL) return -EBADMSG;

  srbmsg->marker = SRB_RESPONSEMARKER;
  
  srbmsg->map = urlmapaddi(srbmsg->map, SRB_RETURNCODE, rcode);
  if (rcode != 0) {
    srbmsg->map = urlmapadd(srbmsg->map, SRB_CAUSE, _mw_srb_reason(rcode));
    if (field != NULL)
      srbmsg->map = urlmapadd(srbmsg->map, SRB_CAUSEFIELD, field);
  };
  
  rc = _mw_srbsendmessage (fd, srbmsg);
  return rc;;
};

int _mw_srbsendcallreply(int fd, SRBmessage * srbmsg, char * data, int len, 
			     int apprcode, int rcode, int flags)
{
  int rc;

  mwlog(MWLOG_DEBUG2, "_mw_srbsendcallreply: begins fd=%d appcode=%d, rcode=%d", 
	fd, apprcode, rcode);
  if (srbmsg == NULL) return -EINVAL;
  if (srbmsg->map == NULL) return -EBADMSG;

  srbmsg->marker = SRB_RESPONSEMARKER;

  /* first update th edata field, or remove as appropreate */
  if ( (data != NULL) && (len > 0) ) {
    rc = urlmapnset(srbmsg->map, SRB_DATA, data, len);
    if (rc == -1)
      srbmsg->map = urlmapnadd(srbmsg->map, SRB_DATA, data, len);
  } else {
    urlmapdel(srbmsg->map, SRB_DATA);
  };

  /* it really is the senders fault if it had set RETURNCODE fields when sending
   * but just to be nice...*/
  urlmapdel(srbmsg->map, SRB_APPLICATIONRC);
  urlmapdel(srbmsg->map, SRB_RETURNCODE);
  /* returncodes, add */
  srbmsg->map = urlmapaddi(srbmsg->map, SRB_APPLICATIONRC, apprcode);
  srbmsg->map = urlmapaddi(srbmsg->map, SRB_RETURNCODE, rcode);

  urlmapdel(srbmsg->map, SRB_MORE);
  if (flags & MWMORE) 
    srbmsg->map = urlmapadd(srbmsg->map, SRB_MORE, SRB_YES);
  else
    srbmsg->map = urlmapadd(srbmsg->map, SRB_MORE, SRB_NO);

  urlmapdel(srbmsg->map, SRB_INSTANCE);
  urlmapdel(srbmsg->map, SRB_CLIENTNAME);
  urlmapdel(srbmsg->map, SRB_NOREPLY);
  urlmapdel(srbmsg->map, SRB_MULTIPLE);
  urlmapdel(srbmsg->map, SRB_MAXHOPS);

  mwlog(MWLOG_DEBUG2, "_mw_srbsendcallreply: sends message");
  rc = _mw_srbsendmessage(fd, srbmsg);
  return rc;
};

int _mw_srbsendready(int fd, char * domain)
{
  int rc;
  SRBmessage srbmsg;

  strncpy(srbmsg.command, SRB_READY, 32);
  srbmsg.marker = SRB_NOTIFICATIONMARKER;
  srbmsg.map = NULL;

  srbmsg.map = urlmapadd(srbmsg.map, SRB_VERSION, SRBPROTOCOLVERSION);
  srbmsg.map = urlmapadd(srbmsg.map, SRB_AGENT, "MidWay");
  srbmsg.map = urlmapadd(srbmsg.map, SRB_AGENTVERSION, (char *) mwversion());
  if (domain != NULL)
    srbmsg.map = urlmapadd(srbmsg.map, SRB_DOMAIN, domain);

  rc = _mw_srbsendmessage(fd, &srbmsg);
  urlmapfree(srbmsg.map);
  return rc;
};
