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


/*
 * $Log$
 * Revision 1.8  2002/08/09 20:50:16  eggestad
 * A Major update for implemetation of events and Task API
 *
 * Revision 1.7  2002/07/07 22:45:48  eggestad
 * *** empty log message ***
 *
 * Revision 1.6  2001/10/03 22:34:18  eggestad
 * - plugged mem leaks, no detected mem leak in mwgwd now
 * - switched to using _mw_srb_*field() instead of direct use of urlmap*()
 *
 * Revision 1.5  2001/09/15 23:49:38  eggestad
 * Updates for the broker daemon
 * better modulatization of the code
 *
 * Revision 1.4  2001/08/29 17:55:15  eggestad
 * fix for changed behavior of urlmapnset
 *
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
#include <stdlib.h>

#include <MidWay.h>
#include <version.h>
#include <SRBprotocol.h>
#include <ipctables.h>
#include <ipcmessages.h>

#include <urlencode.h>

#include "SRBprotocolServer.h"
#include "tcpserver.h"
#include "gateway.h"
#include "connections.h"
#include "store.h"

extern globaldata globals;

static char * RCSId UNUSED = "$Id$";


/*static int srbrole = SRBNOTCONNECTED;*/

static void srbhello(Connection *, SRBmessage * );
static void srbterm(Connection *, SRBmessage * );
static void srbinit(Connection *, SRBmessage * );
static void srbcall(Connection *, SRBmessage * );
static void srbsvcdata(Connection *, SRBmessage * );

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
static char * szGetReqField(Connection * conn, SRBmessage * srbmsg, char * fieldname)
{
  int idx;
  idx = urlmapget(srbmsg->map, fieldname);
  DEBUG2("szGetReqField: index of %s is %d, errno=%d", 
	fieldname, idx, errno);
  if (idx != -1) {
    return srbmsg->map[idx].value;
  } 
  _mw_srbsendreject(conn, srbmsg, fieldname, NULL, SRB_PROTO_FIELDMISSING);
  return NULL;
}

static char * szGetOptField(Connection * conn, SRBmessage * srbmsg, char * fieldname)
{
  int idx;
  idx = urlmapget(srbmsg->map, fieldname);
  DEBUG2("szGetOptField: index of %s is %d errno=%d", 
	fieldname, idx, errno);
  if (idx != -1) {
    return srbmsg->map[idx].value;
  } 
  return NULL;
}

static int iGetOptBinField(Connection * conn, SRBmessage * srbmsg, 
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

static int iGetReqField(Connection * conn, SRBmessage * srbmsg, 
			char * fieldname, int *  value)
{
  int idx, rc;
  idx = urlmapget(srbmsg->map, fieldname);
  if (idx != -1) {
    rc = sscanf (srbmsg->map[idx].value, "%d", value);
    if (rc != 1) {
      _mw_srbsendreject(conn, srbmsg, srbmsg->map[idx].key, srbmsg->map[idx].value, 
			SRB_PROTO_ILLEGALVALUE);
      return 0;
    };
    return 1;
    
  }
  _mw_srbsendreject(conn, srbmsg, fieldname, NULL, SRB_PROTO_FIELDMISSING);
  return 0;
};

static int iGetOptField(Connection * conn, SRBmessage * srbmsg,
			char * fieldname, int *  value)
{
  int idx, rc;
  idx = urlmapget(srbmsg->map, fieldname);
  if (idx != -1) {
    rc = sscanf (srbmsg->map[idx].value, "%d", value);
    if (rc != 1) {
      _mw_srbsendreject(conn, srbmsg, 
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

static void srbhello(Connection * conn, SRBmessage * srbmsg)
{
  struct timeval now;
  char buffer[50];

  gettimeofday(&now, NULL);
   
  DEBUG("Got a hello message");
  switch (srbmsg->marker) {
    
  case SRB_REQUESTMARKER:
    sprintf(buffer, "%d.%06d", now.tv_sec, now.tv_usec);
    _mw_srb_setfield(srbmsg, SRB_REMOTETIME, buffer);

    srbmsg->marker = SRB_RESPONSEMARKER;
    _mw_srbsendmessage(conn, srbmsg);
    break;

  case SRB_RESPONSEMARKER:
     break;

  case SRB_NOTIFICATIONMARKER:  
  default:
    _mw_srbsendreject(conn, srbmsg, NULL, NULL, SRB_PROTO_NOTREQUEST);
  }
  return;
};

static void srbreject(Connection * conn, SRBmessage * srbmsg)
{
  char buff[128];
  conn_getpeername(conn, buff, 128);
  Error("Got a reject message from %s \"%s\"", 
	buff, srbmessageencode(srbmsg));
  return;
};

static void srbready(Connection * conn, SRBmessage * srbmsg)
{
  char buff[128];
  char * domain;
  char * instance;
  int rc;
  struct gwpeerinfo * peerinfo;
  // if multicast reply, this must actually be a reply
  if (conn->type & CONN_TYPE_MCAST) {
    DEBUG("Got a ready message on the multicast socket");

    if (srbmsg->marker != SRB_RESPONSEMARKER) return;
    if (!(domain = szGetReqField(conn, srbmsg, SRB_DOMAIN))) return;
    if (!(instance = szGetReqField(conn, srbmsg, SRB_INSTANCE))) return;

    DEBUG("srb_ready with domain = %s insance = %s", domain, instance);

    /* if the ready reply is not the reply from your own broker about us */
    if (strcmp(globals.myinstance, instance) ==  0) {
      DEBUG("srb_ready was about myself ignoring");
      return;
    };
    
    // here addknownpeer must succeed, unless sonmeone is
    // deliberating thowing us junk
    rc = gw_addknownpeer(instance, domain, &conn->peeraddr.sa, &peerinfo);
    if (rc != 0)
      Warning("gw_addknownpeer failed with mcastreply");
    else 
      gw_connectpeer(peerinfo);

    return;
  };
  
  /* this the welcome from a just connected GW */
  if (conn->type & CONN_TYPE_GATEWAY) {
    int l;
    if (srbmsg->marker != SRB_NOTIFICATIONMARKER) {
      goto error;
    };       
    DEBUG("Got a srb_ready message from a gateway");    
    l = sizeof( conn->peeraddr);
    getpeername(conn->fd, &conn->peeraddr.sa, &l);
    gw_setipc(conn->peerinfo);
    _mw_srbsendgwinit(conn);
    return;  
  };

 error:  
  conn_getpeername(conn, buff, 128);
  Error("Got an unintelligible ready message from %s \"%s\"", 
	buff, srbmessageencode(srbmsg));
  return;

  
};

static void srbterm(Connection * conn, SRBmessage * srbmsg)
{
  int grace;
  char buff[128];

  switch (srbmsg->marker) {
    
  case SRB_REQUESTMARKER:
    iGetOptField(conn, srbmsg, SRB_GRACE, &grace);

    conn_getpeername(conn, buff, 128);
    Info("Got a TERM request GRACE = %d from %s", 
	  grace, buff);
    _mw_srbsendterm(conn, -1);
    break;
    
  case SRB_RESPONSEMARKER:
    
  case SRB_NOTIFICATIONMARKER:
    conn_getpeername(conn, buff, 128);
    Info("Got a TERM notification from %s, closing", 
	  buff);
    
    tcpcloseconnection(conn);
  };

};

static void srbprovide(Connection * conn, SRBmessage * srbmsg)
{
  int cost, rc;
  char * svcname;
  char buff[128];

  conn_getpeername(conn, buff, 128);

  switch (srbmsg->marker) {
    
  case SRB_REQUESTMARKER:
    Error("got a SRB PROVIDE request from %s rejecting", buff);
    _mw_srbsendreject(conn, srbmsg, NULL, NULL, SRB_PROTO_NOTNOTIFICATION);
    break;
    
  case SRB_RESPONSEMARKER:
    Error("got a SRB PROVIDE reply from %s  but we never sent a request: rejecting", buff);
    _mw_srbsendreject(conn, srbmsg, NULL, NULL, SRB_PROTO_UNEXPECTED);
    break;
    
  case SRB_NOTIFICATIONMARKER:
    DEBUG("Got a provide notification from %s", buff);
    
    if (!(iGetReqField(conn, srbmsg,  SRB_COST, &cost))) {
      return;
    };
    
    if (!(svcname =     szGetReqField(conn, srbmsg, SRB_SVCNAME))) {
      return;
    };

    /* this is the only place the cost is increased */
    cost += conn->cost;

    DEBUG("importing service %s with cost %d", svcname, cost);
    rc = importservice(svcname, cost, conn->peerinfo);
    
    if (rc == 0) Info ("imported service %s with cost %d", svcname, cost);
    else Error("while importijng service  %s with cost %d, errno=%d", svcname, cost, errno);
  };

};

static void srbcall(Connection * conn, SRBmessage * srbmsg)
{
  int rc;
  char * tmp;
  
  CLIENTID cltid = UNASSIGNED;
  
  char buff[128];

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

  /* I don't like having to hardcode the offset into the fields in the
     message like this, but is quite simple... */
  conn_getinfo(conn->fd, &cltid, NULL, NULL, NULL);
  if (cltid == UNASSIGNED) {
    Warning("Got a SVCCALL before SRB INIT cltid=%d", cltid&MWINDEXMASK);
    _mw_srbsendcallreply(conn, srbmsg, NULL, 0, 0, SRB_PROTO_NOINIT, 0);
    return;
  };

  DEBUG("srbcall: beginning SVCCALL from cltid=%d", cltid&MWINDEXMASK);

  /* first we get the field that are identical on requests as well as
     replies. */
  if (!(svcname =     szGetReqField(conn, srbmsg, SRB_SVCNAME))) {
    return;
  };
  if ( strlen(svcname) > MWMAXSVCNAME) {
    _mw_srbsendreject(conn, srbmsg, SRB_SVCNAME, tmp, SRB_PROTO_ILLEGALVALUE);
    return;
  };

  
  if (!(iGetReqField(conn, srbmsg,  SRB_HOPS, &hops))) {
    return;
  };


  iGetOptBinField(conn, srbmsg, SRB_DATA, &data, &datalen);
  if (data == NULL) datalen = 0;
  
  iGetOptField(conn, srbmsg, SRB_DATACHUNKS, &datachunks);
  instance =    szGetOptField(conn, srbmsg, SRB_INSTANCE);
  iGetOptField(conn, srbmsg,  SRB_SECTOLIVE, &sectolive);

  conn_getpeername(conn, buff, 128);

  switch (srbmsg->marker) {
    
  case SRB_NOTIFICATIONMARKER:
  case SRB_REQUESTMARKER:
    
    if (srbmsg->marker == SRB_NOTIFICATIONMARKER) {
      /* noreply  */
      DEBUG("Got a SVCCALL notification from %s, NOREPLY", 
	    buff);
      flags |= MWNOREPLY;
      noreply = 1;
    }
    
    if (srbmsg->marker == SRB_REQUESTMARKER) {
      DEBUG("Got a SVCCALL service=%s from %s", 
	    svcname, buff);

      /* handle is optional iff noreply */
      if (!(tmp =         szGetReqField(conn, srbmsg, SRB_HANDLE))) {
	return;
      };
      if ( (tmp == NULL) || (sscanf(tmp,"%x", &handle) != 1) ) {
	_mw_srbsendreject(conn, srbmsg, SRB_HANDLE, tmp, SRB_PROTO_ILLEGALVALUE);
	return;
      };
    };

    domain =      szGetOptField(conn, srbmsg, SRB_DOMAIN);
    clientname =  szGetOptField(conn, srbmsg, SRB_CLIENTNAME);

    tmp =         szGetOptField(conn, srbmsg, SRB_MULTIPLE);
    if (tmp)
      if (strcasecmp(tmp, SRB_YES) == 0) multiple = 1;
      else if (strcasecmp(tmp, SRB_NO) == 0) multiple = 0;
      else {
	_mw_srbsendreject(conn, srbmsg,
		      SRB_MULTIPLE, tmp, SRB_PROTO_ILLEGALVALUE);
	break;
      };

    iGetOptField(conn, srbmsg,  SRB_MAXHOPS, &maxhops);

    /* 
       This is the special case where we don't have any chunks.
       we can do the call directly.
    */
    if ((datachunks == -1) ) {

      DEBUG("SVCCALL was complete doing _mwacallipc");      
      _mw_set_my_clientid(cltid);

      /* we must lock since it happens that the server replies before
         we do storeSetIpcCall() */

      storeLockCall();
      rc = _mwacallipc (svcname, data, datalen, flags);

      _mw_set_my_clientid(UNASSIGNED);
      
      if (rc > 0) {
	storePushCall(cltid, handle, conn->fd, srbmsg->map);
	srbmsg->map = NULL;
	DEBUG("_mwacallipc succeeded");  
	if ( ! noreply) {
	  DEBUG(
		"Storing call fd=%d nethandle=%u clientid=%d ipchandle=%d",
		conn->fd, handle, cltid&MWINDEXMASK, rc);
	  
	  rc = storeSetIPCHandle(cltid, handle, conn->fd, rc);
	  storeUnLockCall();
	  
	  DEBUG("storeSetIPCHandle returned %d", rc);

	  DEBUG("srbsvccall returns good");  
	  return ; /* NB PREVENT urlmapfree() */
	};
      } else {
	DEBUG("_mwacallipc failed with %d ", rc);   
	_mw_srbsendcallreply(conn, srbmsg, NULL, 0, 0, _mw_errno2srbrc(rc), 0);
      };
      storeUnLockCall();
    };
    break;
    
  case SRB_RESPONSEMARKER:
    if (!(tmp =         szGetOptField(conn, srbmsg, SRB_MORE))) break;
    if (strcasecmp(tmp, SRB_YES) == 0) more = 1;
    else if (strcasecmp(tmp, SRB_NO) == 0) more = 0;
    else {
      _mw_srbsendreject(conn, srbmsg, SRB_MORE, tmp, SRB_PROTO_ILLEGALVALUE);
      break;
    };

    DEBUG("Got a SVCCALL reply from %s, ignoring", buff);    
  };

  
};

static void srbinit(Connection * conn, SRBmessage * srbmsg)
{
  int reverse = 0, rejects = 1, auth = 0, role; 
  char * type = NULL, * version= NULL, * tmp = NULL;
  char * domain = NULL, * name = NULL, * user = NULL, * passwd = NULL; 
  char * agent = NULL, * agentver = NULL, * os = NULL;
  int  rc, l;
  char * szptr, peername[128]; 
  char * peerdomain = NULL, * instance = NULL;

  if (srbmsg->map == NULL) {
    _mw_srbsendreject (conn, srbmsg, NULL, NULL, SRB_PROTO_FORMAT);
    return;
  };

  switch (srbmsg->marker) {
    
  case SRB_REQUESTMARKER:
    
    /* se set this in the first message, either here on an INIT req or
       above on e READY notification. */
    l = sizeof( conn->peeraddr);
    getpeername(conn->fd, &conn->peeraddr.sa, &l);


    /* VERSION - REQ */  
    
    if (!(version = szGetReqField(conn, srbmsg, SRB_VERSION))) break;
    if ( strcasecmp(SRBPROTOCOLVERSION, version) != 0) {
      _mw_srbsendinitreply(conn, srbmsg, SRB_PROTO_VERNOTSUPP, SRB_VERSION);
      break;
    };
    
    /* TYPE - REQ */
    if (!(type = szGetReqField(conn, srbmsg, SRB_TYPE))) break;
    
    if (strcasecmp(type, SRB_TYPE_CLIENT) == 0) role = SRB_ROLE_CLIENT;
    else if (strcasecmp(type, SRB_TYPE_GATEWAY) == 0) role = SRB_ROLE_GATEWAY;
    else {
      _mw_srbsendinitreply(conn, srbmsg, SRB_PROTO_ILLEGALVALUE, SRB_TYPE);
      break;
    };

    /* if requested type is not expected, decline */
    if (!(role & conn->role)) {
      if (role == SRB_ROLE_CLIENT)
	_mw_srbsendinitreply(conn, srbmsg, SRB_PROTO_NOCLIENTS, SRB_TYPE);
      else 
	_mw_srbsendinitreply(conn, srbmsg, SRB_PROTO_NOGATEWAYS, SRB_TYPE);
      break;;
    };
    
    /* NAME - REQ */
    if (!(name = szGetReqField(conn, srbmsg, SRB_NAME))) break; 

    /* DOMAIN - OPT */
    domain = szGetOptField(conn, srbmsg, SRB_DOMAIN);
    
    /* REVERSE - OPT */
    tmp = szGetOptField(conn, srbmsg, SRB_REVERSE);
    if (tmp != NULL) {
      if (strcasecmp(tmp, SRB_YES) == 0) reverse = 1;
      else if (strcasecmp(tmp, SRB_NO) == 0) reverse = 0;
      else {
	_mw_srbsendinitreply(conn, srbmsg, SRB_PROTO_ILLEGALVALUE, SRB_REVERSE);
	break;
      };
    };
    /* REJECT - OPT */
    tmp = szGetOptField(conn, srbmsg, SRB_REJECTS);
    if (tmp != NULL) {
      if (strcasecmp(tmp, SRB_YES) == 0) rejects = 1;
      else if (strcasecmp(tmp, SRB_NO) == 0) rejects = 0;
      else {
	_mw_srbsendinitreply(conn, srbmsg, SRB_PROTO_ILLEGALVALUE, SRB_REJECTS);
	break;
      };
    }
    /* AUTHENTICATION - OPT */
    tmp = szGetOptField(conn, srbmsg, SRB_AUTHENTICATION);
    if (tmp != NULL) {
      if (strcasecmp(tmp, SRB_AUTH_NONE) == 0) ;
      else if (strcasecmp(tmp, SRB_AUTH_PASS) == 0) ;
      else if (strcasecmp(tmp, SRB_AUTH_X509) == 0) ;
      else {
	_mw_srbsendinitreply(conn, srbmsg, SRB_PROTO_ILLEGALVALUE, SRB_AUTHENTICATION);
	break;
      };
    };
    /* USER, PASSWORD, AGENT, AGENTVERSION, OS - OPT */
    user     = szGetOptField(conn, srbmsg, SRB_USER);
    passwd   = szGetOptField(conn, srbmsg, SRB_PASSWORD);
    agent    = szGetOptField(conn, srbmsg, SRB_AGENT);
    agentver = szGetOptField(conn, srbmsg, SRB_AGENTVERSION);
    os       = szGetOptField(conn, srbmsg, SRB_OS);
    
    /* Now we we have a valid request, all thats left if to attache
       the client or register the gateway */
    DEBUG("SRB INIT correctly formated role=%d ", role); 

    conn_getpeername(conn, peername, 128);

    /* we use user and name after gwattach() but anytime after that
       the other thread may have gotten back from mwd and done
       urlmapfre() */

    if (user)
      user = strdup(user);
    if (name)
      name = strdup(name);


    l = sizeof( conn->peeraddr);
    getpeername(conn->fd, &conn->peeraddr.sa, &l);


    /* if client */
    if (role == SRB_ROLE_CLIENT) {
      
      if (conn->cid != UNASSIGNED) {
	_mw_srbsendinitreply(conn, srbmsg, SRB_PROTO_ISCONNECTED, NULL);
	break;
      };

      rc = gwattachclient(conn, name, user, passwd, srbmsg->map);
      if (rc >= 0) {
 	srbmsg->map = NULL;
	Info("Client %s %s%s ID=%#u connected from %s", 
	      name, user?"Username=":"", user?user:"",
	      rc , peername);
	rc = rc | MWCLIENTMASK;
	conn->role = role;
	conn_set(conn, SRB_ROLE_CLIENT, CONN_TYPE_CLIENT);
      } else {
	rc = _mw_errno2srbrc(-rc);
	szptr = _mw_srb_reason(rc);
 	Info("Client %s %s%s from %s was rejected: ", 
	      name, user?"Username=":"", user?user:"",
	      peername);
	rc = _mw_errno2srbrc(-rc);
	_mw_srbsendinitreply(conn, srbmsg, rc, NULL);
      }
    } else if (role == SRB_ROLE_GATEWAY) {
      if (!(instance     = szGetReqField(conn, srbmsg, SRB_INSTANCE))) break;
      if (!(peerdomain   = szGetReqField(conn, srbmsg, SRB_PEERDOMAIN))) break;
      
      if (strcmp(instance, globals.myinstance)  != 0) {
	Error("Hmm got an INIT from another gateway but it tried to " 
	      "connect to instance %s and I'm %s, looks like a broker problem", 
	      instance, globals.myinstance);
	_mw_srbsendinitreply(conn, srbmsg, SRB_PROTO_GATEWAY_ERROR, NULL);
	conn_del(conn->fd);
	break;
      };
	
      rc = gw_peerconnected(name, peerdomain, conn);
      if (rc == -1 ) {
	_mw_srbsendterm(conn, 0);
	gw_closegateway(conn->gwid);
	break;
      };

      // here is OK
      conn_set(conn, SRB_ROLE_GATEWAY, CONN_TYPE_GATEWAY);
      conn->state = CONNECT_STATE_UP;
      _mw_srbsendinitreply(conn, srbmsg, 0, NULL);      
      gw_provideservices_to_peer(conn->gwid);     
      break;

    } else {
      _mw_srbsendreject (conn, srbmsg, SRB_TYPE, type, SRB_PROTO_FORMAT);
      break;
    };
    
  case SRB_RESPONSEMARKER:
    if ( ! iGetReqField(conn, srbmsg, SRB_RETURNCODE, &rc)) break;
    
    if (rc == 0) {
      conn->state = CONNECT_STATE_UP;
      gw_provideservices_to_peer(conn->gwid);
    } else 
      gw_closegateway(conn->gwid);
    
    break;
      
  case SRB_NOTIFICATIONMARKER:  
  default:
    _mw_srbsendinitreply(conn, srbmsg, SRB_PROTO_NOTREQUEST, NULL);
  };
  
  if (user) free(user);
  if (name) free(name);

  return;
};


/**********************************************************************
 * entry function for receiving messages, basic syntax check and 
 * dispatch to API above.
 **********************************************************************/

  
int srbDoMessage(Connection * conn, char * message)
{
  int commandlen;
  char * ptr;
  SRBmessage srbmsg;
  
  if ( (ptr = strchr(message, SRB_REQUESTMARKER)) == NULL)
    if ( (ptr = strchr(message, SRB_RESPONSEMARKER)) == NULL)
      if ( (ptr = strchr(message, SRB_NOTIFICATIONMARKER)) == NULL) {
	if ( (ptr = strchr(message, SRB_REJECTMARKER)) != NULL) {
	  return 0;
	} else { 
	  _mw_srbsendreject_sz(conn, message, -1);
	  return -1;
	}
      }
  commandlen = ptr - message;
  if (commandlen > 32) {
    _mw_srbsendreject_sz(conn, message, commandlen);
    return -1;
  };
  strncpy(srbmsg.command, message, commandlen);
  srbmsg.command[commandlen] = '\0';
  srbmsg.marker = *ptr;
  srbmsg.map = urlmapdecode(message+commandlen+1);
  
  DEBUG2("srbDoMessage: command=%*.*s marker=%c on fd=%d", 
	commandlen, commandlen, message, *ptr, conn->fd);

  /* switch on command length in order to speed up (avoiding excessive
     strcmp()'s */
  switch(commandlen) {
  case 4:
    if ( strcasecmp(srbmsg.command, SRB_TERM) == 0) {
      srbterm(conn, &srbmsg);
      break ;
    };
    break;

  case 5:
    if ( strcasecmp(srbmsg.command, SRB_HELLO) == 0) {
      srbhello(conn, &srbmsg);
      break;
    }
    break;

  case 6:
    if ( strcasecmp(srbmsg.command, SRB_REJECT) == 0) {
      srbreject(conn, &srbmsg);
      break;
    }
    break;

  case 7:
    if ( strcasecmp(srbmsg.command, SRB_SVCCALL) == 0) {
      srbcall(conn, &srbmsg);
      break;
    }
    if ( strcasecmp(srbmsg.command, SRB_PROVIDE) == 0) {
      srbprovide(conn, &srbmsg);
      break;
    }
    break;

  case 8:
    if ( strcasecmp(srbmsg.command, SRB_INIT) == 0) {
      srbinit(conn, &srbmsg);
      break;
    }
    break;

  case 9:
    if ( strcasecmp(srbmsg.command, SRB_READY) == 0) {
      srbready(conn, &srbmsg);
      break;
    }
    break;

  default:
    _mw_srbsendreject_sz(conn, message, 0);
  };
  
  if (srbmsg.map != NULL)
    urlmapfree(srbmsg.map);

  return 0;
};

/**********************************************************************
 * API for sending messages only servers shall send
 **********************************************************************/

int _mw_srbsendinitreply(Connection * conn, SRBmessage * srbmsg, int rcode, char * field)
{
  int  rc;

  if (srbmsg == NULL) return -EINVAL;
  if (srbmsg->map == NULL) return -EBADMSG;

  srbmsg->marker = SRB_RESPONSEMARKER;
  
  _mw_srb_setfieldi(srbmsg, SRB_RETURNCODE, rcode);
  if (rcode != 0) {
    _mw_srb_setfield(srbmsg, SRB_CAUSE, _mw_srb_reason(rcode));
    if (field != NULL)
      _mw_srb_setfield(srbmsg, SRB_CAUSEFIELD, field);
  };
  
  rc = _mw_srbsendmessage (conn, srbmsg);

  /* if an error we donæt send provides */
  if (!( (rc <= 0) || (rcode != 0) ))   
    gw_provideservices_to_peer(conn->gwid);
  
  return rc;;
};

int _mw_srbsendcallreply(Connection * conn, SRBmessage * srbmsg, char * data, int len, 
			     int apprcode, int rcode, int flags)
{
  int rc;

  DEBUG2("_mw_srbsendcallreply: begins fd=%d appcode=%d, rcode=%d", 
	conn->fd, apprcode, rcode);
  if (srbmsg == NULL) return -EINVAL;
  if (srbmsg->map == NULL) return -EBADMSG;

  srbmsg->marker = SRB_RESPONSEMARKER;

  /* first update the data field, or remove as appropreate */
  if ( (data != NULL) && (len > 0) ) {
    _mw_srb_nsetfield(srbmsg, SRB_DATA,data, len);
  } else {
    _mw_srb_delfield(srbmsg, SRB_DATA);
  };

  /* it really is the senders fault if it had set RETURNCODE fields when sending
   * but just to be nice...*/
  _mw_srb_delfield(srbmsg, SRB_APPLICATIONRC);
  _mw_srb_delfield(srbmsg, SRB_RETURNCODE);
  /* returncodes, add */
  _mw_srb_setfieldi(srbmsg, SRB_APPLICATIONRC, apprcode);
  _mw_srb_setfieldi(srbmsg, SRB_RETURNCODE, rcode);

  _mw_srb_delfield(srbmsg, SRB_INSTANCE);
  _mw_srb_delfield(srbmsg, SRB_CLIENTNAME);
  _mw_srb_delfield(srbmsg, SRB_NOREPLY);
  _mw_srb_delfield(srbmsg, SRB_MULTIPLE);
  _mw_srb_delfield(srbmsg, SRB_MAXHOPS);

  DEBUG2("_mw_srbsendcallreply: sends message");
  rc = _mw_srbsendmessage(conn, srbmsg);
  return rc;
};

int _mw_srbsendready(Connection * conn, char * domain)
{
  int rc;
  SRBmessage srbmsg;


  _mw_srb_init(&srbmsg, SRB_READY, SRB_NOTIFICATIONMARKER, 
	       SRB_VERSION, SRBPROTOCOLVERSION, 
	       SRB_AGENT, "MidWay", 
	       SRB_AGENTVERSION, (char *) mwversion(), 
	       NULL);

  if (domain != NULL)
    _mw_srb_setfield(&srbmsg, SRB_DOMAIN, domain);

  rc = _mw_srbsendmessage(conn, &srbmsg);
  urlmapfree(srbmsg.map);
  return rc;
};

int _mw_srbsendprovide(Connection * conn, char * service, int cost)
{
  int rc;
  SRBmessage srbmsg;
  char _cost[16];

  DEBUG2("begin");
  if (cost < 0) cost = gw_getcostofservice(service);
  if (cost < 0) {
    Error("cost of service %s is %d, which is illegal, maybe the mwd() is updateing the IPC tables", 
	  service, cost);    
    return -1;
  };

  if (cost > 999999) cost = 999999;

  sprintf (_cost, "%d", cost);
  _mw_srb_init(&srbmsg, SRB_PROVIDE, SRB_NOTIFICATIONMARKER, 
	       SRB_SVCNAME, service,
	       SRB_COST, _cost,
	       NULL);

  rc = _mw_srbsendmessage(conn, &srbmsg);
  urlmapfree(srbmsg.map);
    DEBUG2("send returned %d", rc);
  return rc;
};

/* this is a GW to >GW init message, NOT to be confsed with client INIT messages. */
int _mw_srbsendgwinit(Connection * conn)
{
  int  rc, domainid;
  char * domain;
  char * auth = SRB_AUTH_NONE;
  SRBmessage srbmsg;
  struct gwpeerinfo * peerinfo;
  if (conn == NULL) return -EINVAL;

  peerinfo = (struct gwpeerinfo * ) conn->peerinfo;

  if (peerinfo == NULL) {
    Fatal( "Inetrnal error, in " __FUNCTION__ " conn->peerinfo is NULL");
  };
  
  if (peerinfo->domainid == 0) {
    DEBUG("_mw_srbsendgwinit() to GW in my own domain");
    domain = globals.mydomain;
  } else if (peerinfo->domainid < 0) {
    Fatal("Attemped to connect to a peer with domainid = %d < 0", 
	  peerinfo->domainid);
  } else {
    DEBUG("_mw_srbsendgwinit() to foreign domain");
    Fatal("foreign domains NYI");
  };

  _mw_srb_init(&srbmsg, SRB_INIT, SRB_REQUESTMARKER, 
	       SRB_VERSION, SRBPROTOCOLVERSION, 
	       SRB_TYPE, SRB_TYPE_GATEWAY, 
	       SRB_NAME, globals.myinstance, 
	       SRB_PEERDOMAIN, globals.mydomain, 
	       SRB_INSTANCE, peerinfo->instance, 
	       SRB_DOMAIN, domain, 
	       NULL);

  
  /* optional */
  if (peerinfo->password != NULL) {
    DEBUG("connecting with password = \"%s\"", peerinfo->password);
    _mw_srb_setfield (&srbmsg, SRB_AUTHENTICATION, SRB_AUTH_PASS);
    _mw_srb_setfield(&srbmsg, SRB_PASSWORD, peerinfo->password);
  } else {
    DEBUG("connecting with no authentication", peerinfo->password);
    _mw_srb_setfield (&srbmsg, SRB_AUTHENTICATION, SRB_AUTH_NONE);
  };

  conn->state = CONNECT_STATE_INITWAIT;
  rc = _mw_srbsendmessage(conn, &srbmsg);
  
  urlmapfree(srbmsg.map);
  return rc;
};
