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
 * Revision 1.17  2003/03/16 23:50:23  eggestad
 * Major fixups
 *
 * Revision 1.16  2003/01/07 08:27:40  eggestad
 * * Major fixes to get three mwgwd working correctly with one service
 * * and other general fixed for suff found on the way
 *
 * Revision 1.15  2002/11/19 12:43:55  eggestad
 * added attribute printf to mwlog, and fixed all wrong args to mwlog and *printf
 *
 * Revision 1.14  2002/11/18 00:14:59  eggestad
 * - when a connection is established not not UP in SRB context, we need
 *   to handle SRB INIT special, and reject everything else
 *
 * Revision 1.13  2002/10/22 21:58:20  eggestad
 * Performace fix, the connection peer address, is now set when establised, we did a getnamebyaddr() which des a DNS lookup several times when processing a single message in the gateway (Can't believe I actually did that...)
 *
 * Revision 1.12  2002/10/20 18:15:42  eggestad
 * major rework for sending calls. srbcall now split into srbcall_req, and srbcall_rpl
 *
 * Revision 1.11  2002/10/17 22:10:43  eggestad
 * - iGetOptBinField renamed vGetOptBinField
 * - added xGetOptField for hex encoded field
 * - changed to handle gateways as well as clients (not complete)
 *
 * Revision 1.10  2002/09/29 17:41:06  eggestad
 * added srbunprovide() handler
 *
 * Revision 1.9  2002/09/22 22:51:50  eggestad
 * added srbsendunprovide()
 *
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
#include "impexp.h"
#include "shmalloc.h"

extern globaldata globals;

static char * RCSId UNUSED = "$Id$";

static void srbhello(Connection *, SRBmessage * );
static void srbterm(Connection *, SRBmessage * );
static void srbinit(Connection *, SRBmessage * );
static void srbcall_req(Connection *, SRBmessage * );
static void srbcall_rpl(Connection *, SRBmessage * );
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
  DEBUG2("index of %s is %d, errno=%d", 
	fieldname, idx, errno);
  if (idx != -1) {
    if (srbmsg->map[idx].value == NULL) {
      _mw_srbsendreject(conn, srbmsg, fieldname, NULL, SRB_PROTO_ILLEGALVALUE);
      return NULL;
    };
    return srbmsg->map[idx].value;
  } 
  _mw_srbsendreject(conn, srbmsg, fieldname, NULL, SRB_PROTO_FIELDMISSING);
  return NULL;
}

static char * szGetOptField(Connection * conn, SRBmessage * srbmsg, char * fieldname)
{
  int idx;
  idx = urlmapget(srbmsg->map, fieldname);
  DEBUG2("index of %s is %d errno=%d", 
	fieldname, idx, errno);
  if (idx != -1) {
    return srbmsg->map[idx].value;
  } 
  return NULL;
}

static int vGetOptBinField(Connection * conn, SRBmessage * srbmsg, 
			   char * fieldname, char ** value, int * valuelen)
{
  int idx;
  idx = urlmapget(srbmsg->map, fieldname);
  if (idx != -1) {
    *value = srbmsg->map[idx].value;
    *valuelen = srbmsg->map[idx].valuelen;
    DEBUG2("found opt bin field %s %d bytes of data", fieldname, *valuelen);
    return 1;
  } 
  *value = NULL;
  *valuelen = -1;
  DEBUG2("no opt bin field %s found", fieldname);
  return 0;
}

static int iGetOptField(Connection * conn, SRBmessage * srbmsg,
			char * fieldname, int *  value)
{
  int idx, rc;
  idx = urlmapget(srbmsg->map, fieldname);
  if (idx != -1) {
    if (srbmsg->map[idx].value == NULL) {
      _mw_srbsendreject(conn, srbmsg, fieldname, NULL, SRB_PROTO_ILLEGALVALUE);
      return -1;
    };
    rc = sscanf (srbmsg->map[idx].value, "%d", value);
    if (rc != 1) {
      _mw_srbsendreject(conn, srbmsg, 
			srbmsg->map[idx].key, srbmsg->map[idx].value, 
			SRB_PROTO_ILLEGALVALUE);
      return -1;
    };
    DEBUG2("found opt int field %s  %s %d", fieldname, srbmsg->map[idx].value, *value);
    return 1;
  }
  DEBUG2("opt int field %s not found", fieldname);
  return 0; 
};

static int iGetReqField(Connection * conn, SRBmessage * srbmsg, 
			char * fieldname, int *  value)
{
  int rc;
  rc = iGetOptField(conn, srbmsg, fieldname, value);
  if (rc == 0) {
    _mw_srbsendreject(conn, srbmsg, fieldname, NULL, SRB_PROTO_FIELDMISSING);
    return -1;
  };
  return rc;
};

static int xGetOptField(Connection * conn, SRBmessage * srbmsg,
			char * fieldname, int *  value)
{
  int idx, rc;
  idx = urlmapget(srbmsg->map, fieldname);
  if (idx != -1) {
    rc = sscanf (srbmsg->map[idx].value, "%x", value);
    if (rc != 1) {
      _mw_srbsendreject(conn, srbmsg, 
			srbmsg->map[idx].key, srbmsg->map[idx].value, 
			SRB_PROTO_ILLEGALVALUE);
      return -1;
    };
    DEBUG2("found opt hex field %s  %s %x(%d)", fieldname, srbmsg->map[idx].value, *value, *value);
    return 1;
  }
  DEBUG2("opt hex field %s not found", fieldname);
  return 0; 
};

static int xGetReqField(Connection * conn, SRBmessage * srbmsg,
			char * fieldname, int *  value)
{
  int rc;
  
  rc = xGetOptField(conn, srbmsg, fieldname, value);
  if (rc == 0) {
    _mw_srbsendreject(conn, srbmsg, fieldname, NULL, SRB_PROTO_FIELDMISSING);
    return -1;
  };  
  return rc;
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
    sprintf(buffer, "%ld.%06ld", now.tv_sec, now.tv_usec);
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
  Error("Got a reject message from %s \"%s\"", 
	conn->peeraddr_string, srbmessageencode(srbmsg));
  return;
};

static void srbready(Connection * conn, SRBmessage * srbmsg)
{
  char * domain;
  char * instance;
  int rc;
  struct gwpeerinfo * peerinfo = NULL;
  // if multicast reply, this must actually be a reply
  if (conn->type == CONN_TYPE_MCAST) {
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
    DEBUG2("####  addknownpeer returned %d with peerinfo = %p conn=%p", 
	   rc, peerinfo, peerinfo->conn);
    if (rc != 0)
      Warning("gw_addknownpeer failed with mcastreply");
    else {
       if (peerinfo->conn == NULL) {
	  gw_connectpeer(peerinfo);
       } else {
	  DEBUG("instance %s is already connected on fd=%d with gwid %d", 
		peerinfo->instance, peerinfo->conn->fd, GWID2IDX(peerinfo->gwid));
       }
    };
    return;
  };
  
  /* this the welcome from a just connected GW */
  if (conn->type == CONN_TYPE_GATEWAY) {
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
  Error("Got an unintelligible ready message from %s \"%s\"", 
	conn->peeraddr_string, srbmessageencode(srbmsg));
  return;

  
};

static void srbterm(Connection * conn, SRBmessage * srbmsg)
{
  int grace;
  
  switch (srbmsg->marker) {
    
  case SRB_REQUESTMARKER:
    iGetOptField(conn, srbmsg, SRB_GRACE, &grace);

    Info("Got a TERM request GRACE = %d from %s", 
	  grace, conn->peeraddr_string);
    _mw_srbsendterm(conn, -1);
    break;
    
  case SRB_RESPONSEMARKER:
    
  case SRB_NOTIFICATIONMARKER:
    Info("Got a TERM notification from %s, closing", 
	  conn->peeraddr_string);
    
    tcpcloseconnection(conn);
  };

};

static void srbprovide(Connection * conn, SRBmessage * srbmsg)
{
  int cost, rc;
  char * svcname;
  
  switch (srbmsg->marker) {
    
  case SRB_REQUESTMARKER:
    Error("got a SRB PROVIDE request from %s rejecting", conn->peeraddr_string);
    _mw_srbsendreject(conn, srbmsg, NULL, NULL, SRB_PROTO_NOTNOTIFICATION);
    break;
    
  case SRB_RESPONSEMARKER:
    Error("got a SRB PROVIDE reply from %s  but we never sent a request: rejecting", 
	  conn->peeraddr_string);
    _mw_srbsendreject(conn, srbmsg, NULL, NULL, SRB_PROTO_UNEXPECTED);
    break;
    
  case SRB_NOTIFICATIONMARKER:
    DEBUG("Got a provide notification from %s", conn->peeraddr_string);
    
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

static void srbunprovide(Connection * conn, SRBmessage * srbmsg)
{
  int cost, rc;
  char * svcname;

  switch (srbmsg->marker) {
    
  case SRB_REQUESTMARKER:
    Error("got a SRB PROVIDE request from %s rejecting", conn->peeraddr_string);
    _mw_srbsendreject(conn, srbmsg, NULL, NULL, SRB_PROTO_NOTNOTIFICATION);
    break;
    
  case SRB_RESPONSEMARKER:
    Error("got a SRB PROVIDE reply from %s  but we never sent a request: rejecting", 
	  conn->peeraddr_string);
    _mw_srbsendreject(conn, srbmsg, NULL, NULL, SRB_PROTO_UNEXPECTED);
    break;
    
  case SRB_NOTIFICATIONMARKER:
    DEBUG("Got a provide notification from %s", conn->peeraddr_string);
    
    if (!(svcname =     szGetReqField(conn, srbmsg, SRB_SVCNAME))) {
      return;
    };

    DEBUG("unimporting service %s", svcname);
    rc = unimportservice(svcname, conn->peerinfo);
    
    if (rc == 0) Info ("imported service %s with cost %d", svcname, cost);
    else Error("while importijng service  %s with cost %d, errno=%d", svcname, cost, errno);
  };
  return;
};

static void srbcall_rpl(Connection * conn, SRBmessage * srbmsg)
{
  int rc;
  Call cmsg = { 0 };
  Connection * cltconn;
  MWID mwid = UNASSIGNED; 

  char * svcname;
  char * data = NULL;
  int datachunks = -1;
  char * instance = NULL;
  int datalen = 0;
  int sectolive = -1;
  char * domain = NULL;
  int hops = 0;
  int maxhops = 30;

  /* I don't like having to hardcode the offset into the fields in the
     message like this, but is quite simple... */
  conn_getinfo(conn->fd, &mwid, NULL, NULL, NULL);
  if (mwid == UNASSIGNED) {
    Warning("Got a SVCCALL before SRB INIT cltid=%d gwid=%d", CLTID(mwid), GWID(mwid));
    _mw_srbsendcallreply(conn, srbmsg, NULL, 0, 0, SRB_PROTO_NOINIT, 0);
    return;
  };

  DEBUG("Got a SVCCALL reply from %s", conn->peeraddr_string);    

  dbg_srbprintmap(srbmsg); 

  cmsg.mtype = SVCREPLY;

  /* first we get the field that are identical on requests as well as
     replies. */
  if (!(svcname =     szGetReqField(conn, srbmsg, SRB_SVCNAME))) {
      return;
  };
  
  if ( strlen(svcname) > MWMAXSVCNAME) {
    _mw_srbsendreject(conn, srbmsg, SRB_SVCNAME, svcname, SRB_PROTO_ILLEGALVALUE);
    return;
  };
  
  strncpy(cmsg.service, svcname,MWMAXSVCNAME ); 
  
  
  if (!(instance =     szGetReqField(conn, srbmsg, SRB_INSTANCE))) {
    return;
  };

  if (strcmp(instance, globals.myinstance) != 0) {
    Error(" got a call reply but's not for my instance %s != %s", 
	  instance, globals.myinstance);
    return;
  };
  strncpy(cmsg.instance, instance, MWMAXNAMELEN);
  
  domain =    szGetOptField(conn, srbmsg, SRB_DOMAIN);
  if (domain) {
    strncpy(cmsg.domainname, domain, MWMAXNAMELEN);
  };

  if (!(iGetReqField(conn, srbmsg,  SRB_HOPS, &hops))) {
    return;
  };

  // this is where we inc hops counter. This means that SRB client svc
  // calls has always hops=1 and that replies has hops 0 when the
  // gateway receive the IPC message from the repliting server.
  hops++;
  cmsg.hops = hops;
  
  mwid = UNASSIGNED;
  if (iGetOptField(conn, srbmsg,  SRB_CALLERID, &mwid) == -1) {
    return;
  };

  cmsg.cltid = CLTID(mwid);
  cmsg.srvid = SRVID(mwid);
  cmsg.callerid = mwid;
  //TODO paassing on to a gateway not a client

  if (xGetReqField(conn, srbmsg,  SRB_HANDLE, &cmsg.handle) == -1) {
    return;
  };

  if (iGetReqField(conn, srbmsg,  SRB_RETURNCODE, &cmsg.returncode) == -1) {
    return;
  };

  if (iGetOptField(conn, srbmsg,  SRB_APPLICATIONRC, &cmsg.appreturncode) == -1) 
    return;

  DEBUG("got a service reply instance %s, callerid %#x", 
	instance, mwid);

  if (strcmp(instance, globals.myinstance) == 0) {
    DEBUG("got a service reply that originated in this instance");
    cltconn = gwlocalclient(mwid);
    if (cltconn != NULL) {
      SRBmessage srborigmsg;
      char * handle;
      int freemap = 0;

      DEBUG("got a call reply to a local client %d. shortcutting", CLTID2IDX(mwid));

      /* Now we must get the stored original srb message from client
	 to get the clients handle. The handle we get in return from
	 the gateway is our ipchandle */

      storeLockCall();
      if (cmsg.returncode == MWMORE) {
	rc = storeGetCall(mwid, cmsg.handle, NULL, &srborigmsg.map);
	DEBUG2("storeGetCall returned %d, address of old map is %#x", 
	       rc, srborigmsg.map);
      } else {
	rc = storePopCall(mwid, cmsg.handle, NULL, &srborigmsg.map);
	DEBUG2("storePopCall returned %d, address of old map is %#x", 
	       rc, srborigmsg.map);
	freemap = 1;
      };
      if (rc != 1) {
	Warning("failed to get call message from client %d", cmsg.cltid);
      } else {
	handle = urlmapgetvalue(srborigmsg.map, SRB_HANDLE);
	if (handle == NULL) {
	  Error("got original SRB  call message from client %d, but it has no Handle!", cmsg.cltid);
	} else {
	  handle = strdup(handle);
	};
      };
      storeUnLockCall();

      DEBUG("replacing handle %s with %s", urlmapgetvalue(srbmsg->map, SRB_HANDLE), handle);
      urlmapset(srbmsg->map, SRB_HANDLE, handle);

      _mw_srbsendmessage(cltconn, srbmsg);
      if (freemap) {
	DEBUG("freeing map after pop");
	urlmapfree(srborigmsg.map);
      };
      return;

    } else {
      DEBUG("got a call reply to ipc client %d", CLTID2IDX(mwid));
      cmsg.cltid = mwid;
    };
  } else {
    DEBUG("got a service reply that originated in  instance %s, routing to gw %d", 
	  instance, GWID2IDX(mwid));
    cmsg.gwid = mwid;
  };
  
  vGetOptBinField(conn, srbmsg, SRB_DATA, &data, &datalen);
  if (data == NULL) datalen = 0;
  else {
    _mw_putbuffer_to_call(&cmsg, data, datalen);
  };
  

  // TODO: datachunks 
  /*
  iGetOptField(conn, srbmsg, SRB_DATACHUNKS, &datachunks);
  */
  //  iGetOptField(conn, srbmsg,  SRB_SECTOLIVE, &sectolive);

  rc = _mw_ipc_putmessage(mwid, (void *) &cmsg, sizeof(Call), IPC_NOWAIT);
  if (rc != 0) {
    Error ("_mw_ipc_putmessage(%x, , , ,) failed rc=%d", mwid,rc);
    if (data) {
      _mwfree(_mwoffset2adr(cmsg.data));
    };
  };
  return;
     
};

static void srbcall_req(Connection * conn, SRBmessage * srbmsg)
{
  int rc;
  char * tmp;
  
  MWID mwid = UNASSIGNED; 

  char * svcname;
  char * data = NULL;
  int datachunks = -1;
  char * instance = NULL;
  char * clientname = NULL;
  unsigned long handle = 0xffffffff;
  long flags = 0;
  int datalen = 0;
  int noreply = 0;
  int multiple = 0;
  int sectolive = -1;
  char * domain = NULL;
  int callerid;
  int hops = 0;
  int maxhops = 30;

  TIMEPEG();

  conn_getinfo(conn->fd, &mwid, NULL, NULL, NULL);
  if (mwid == UNASSIGNED) {
    Warning("Got a SVCCALL before SRB INIT cltid=%d gwid=%d", CLTID(mwid), GWID(mwid));
    _mw_srbsendcallreply(conn, srbmsg, NULL, 0, 0, SRB_PROTO_NOINIT, 0);
    return;
  };

  DEBUG("srbcall: beginning SVCCALL from cltid=%d gwid=%d", CLTID(mwid), GWID(mwid));

  TIMEPEG();
  dbg_srbprintmap(srbmsg); 
  TIMEPEG();

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

  // this is where we inc hops counter. This means that SRB client svc calls has always hops=1 
  hops++;

  vGetOptBinField(conn, srbmsg, SRB_DATA, &data, &datalen);

  if (data == NULL) datalen = 0;
  
  iGetOptField(conn, srbmsg, SRB_DATACHUNKS, &datachunks);
  instance =    szGetOptField(conn, srbmsg, SRB_INSTANCE);
  iGetOptField(conn, srbmsg,  SRB_SECTOLIVE, &sectolive);

  if (srbmsg->marker == SRB_NOTIFICATIONMARKER) {
    /* noreply  */
    DEBUG("Got a SVCCALL notification from %s, NOREPLY", 
	  conn->peeraddr_string);
    flags |= MWNOREPLY;
    noreply = 1;
  }

  if (srbmsg->marker == SRB_REQUESTMARKER) {
    DEBUG("Got a SVCCALL service=%s from %s", 
	  svcname, conn->peeraddr_string);
    
    /* handle is optional iff noreply */
    if (!(tmp =         szGetReqField(conn, srbmsg, SRB_HANDLE))) {
      return;
    };
    if ( (tmp == NULL) || (sscanf(tmp,"%lx", &handle) != 1) ) {
      _mw_srbsendreject(conn, srbmsg, SRB_HANDLE, tmp, SRB_PROTO_ILLEGALVALUE);
      return;
    };
  };
  
  /* TODO: are these req if peer is a gw? */
  domain =      szGetOptField(conn, srbmsg, SRB_DOMAIN);
  callerid = UNASSIGNED;
  iGetOptField(conn, srbmsg, SRB_CALLERID, &callerid);
  
  clientname =  szGetOptField(conn, srbmsg, SRB_CLIENTNAME);
  
  tmp =         szGetOptField(conn, srbmsg, SRB_MULTIPLE);
  if (tmp)
    if (strcasecmp(tmp, SRB_YES) == 0) multiple = 1;
    else if (strcasecmp(tmp, SRB_NO) == 0) multiple = 0;
    else {
      _mw_srbsendreject(conn, srbmsg,
			SRB_MULTIPLE, tmp, SRB_PROTO_ILLEGALVALUE);

      return;
    };
  
  iGetOptField(conn, srbmsg,  SRB_MAXHOPS, &maxhops);
  TIMEPEG();

  /* 
     This is the special case where we don't have any chunks.
     we can do the call directly.
  */
  if ((datachunks == -1) ) {

    DEBUG("SVCCALL was complete doing _mwacallipc");      
    
    /* we must lock since it happens that the server replies before
       we do storeSetIpcCall() */
    TIMEPEG();    
    storeLockCall();
    TIMEPEG();

    // TODO: (?)
    // We do actually send to ourselves if we're the only one
    // providing the service. It's inefficient, but clean, and it's
    // only _mwacallipc() theat do the _mw_get_services_byname() call
    // which is heavy.

    rc = _mwacallipc (svcname, data, datalen, flags, mwid, instance, domain, callerid, hops);
    TIMEPEG();
    if (rc > 0) {
      DEBUG("_mwacallipc succeeded");  

      if ( ! noreply) {
	storePushCall(mwid, handle, conn->fd, srbmsg->map);
	srbmsg->map = NULL;
	DEBUG(
	      "Storing call fd=%d nethandle=%u mwid=%#x ipchandle=%d",
	      conn->fd, handle, mwid, rc);
	TIMEPEG();
	
	rc = storeSetIPCHandle(mwid, handle, conn->fd, rc);
	  
	DEBUG("storeSetIPCHandle returned %d", rc);
	DEBUG("srbsvccall returns good");  
      };
    } else {
      DEBUG("_mwacallipc failed with %d ", rc);   
      _mw_srbsendcallreply(conn, srbmsg, NULL, 0, 0, _mw_errno2srbrc(rc), 0);
    };
    TIMEPEG();
    storeUnLockCall();
    TIMEPEG();
  };
  return;  
};

static void srbinit(Connection * conn, SRBmessage * srbmsg)
{
  int reverse = 0, rejects = 1, auth = 0, role; 
  char * type = NULL, * version= NULL, * tmp = NULL;
  char * domain = NULL, * name = NULL, * user = NULL, * passwd = NULL; 
  char * agent = NULL, * agentver = NULL, * os = NULL;
  int  rc, l;
  char * szptr;
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
	Info("Client %s %s%s ID=%d connected from %s", 
	     name, user?"Username=":"", user?user:"",
	     rc , conn->peeraddr_string);
	rc = rc | MWCLIENTMASK;
	conn->role = role;
	conn_set(conn, SRB_ROLE_CLIENT, CONN_TYPE_CLIENT);
	conn->state = CONNECT_STATE_UP;
	conn_setpeername(conn);
      } else {
	rc = _mw_errno2srbrc(-rc);
	szptr = _mw_srb_reason(rc);
 	Info("Client %s %s%s from %s was rejected: ", 
	      name, user?"Username=":"", user?user:"",
	      conn->peeraddr_string);
	rc = _mw_errno2srbrc(-rc);
	_mw_srbsendinitreply(conn, srbmsg, rc, NULL);
      }
    } else if (role == SRB_ROLE_GATEWAY) {
      if (!(instance     = szGetReqField(conn, srbmsg, SRB_INSTANCE))) break;
      if (!(peerdomain   = szGetReqField(conn, srbmsg, SRB_PEERDOMAIN))) break;
      
      if (strcmp(instance, globals.myinstance)  != 0) {
	Error("Hmm got an INIT from another gateway, but it tried to " 
	      "connect to instance %s and I'm %s, looks like a broker problem", 
	      instance, globals.myinstance);
	_mw_srbsendinitreply(conn, srbmsg, SRB_PROTO_GATEWAY_ERROR, NULL);
	conn_del(conn->fd);
	break;
      };
	
      rc = gw_peerconnected(name, peerdomain, conn);
      if (rc == -1 ) {
	 _mw_srbsendterm(conn, 0);
	 if (conn->gwid != UNASSIGNED)
	     gw_closegateway(conn->gwid);

	 conn_del(conn->fd);
	 break;
      };

      // here is OK
      conn_set(conn, SRB_ROLE_GATEWAY, CONN_TYPE_GATEWAY);
      conn->state = CONNECT_STATE_UP;
      conn_setpeername(conn);
      _mw_srbsendinitreply(conn, srbmsg, 0, NULL);      

      if (conn->type == CONN_TYPE_GATEWAY)
	gw_provideservices_to_peer(conn->gwid);     

    } else {
      _mw_srbsendreject (conn, srbmsg, SRB_TYPE, type, SRB_PROTO_FORMAT);
    };
    break;

  case SRB_RESPONSEMARKER:
    if ( ! iGetReqField(conn, srbmsg, SRB_RETURNCODE, &rc)) break;
    
    if (rc == 0) {
      conn->state = CONNECT_STATE_UP;
      conn_setpeername(conn);

      if (conn->type == CONN_TYPE_GATEWAY)
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

  TIMEPEG();  
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
  TIMEPEG();
  srbmsg.map = urlmapdecode(message+commandlen+1);
  TIMEPEG();

  DEBUG2("srbDoMessage: command=%*.*s marker=%c on fd=%d", 
	commandlen, commandlen, message, *ptr, conn->fd);

  dbg_srbprintmap(srbmsg); 
  TIMEPEG();

  /* if the connection is not initiated, we handle it special */
  if (conn->state != CONNECT_STATE_UP) {
    if ( strcasecmp(srbmsg.command, SRB_INIT) == 0) {
      srbinit(conn, &srbmsg);
    } else if ( strcasecmp(srbmsg.command, SRB_READY) == 0) {
      srbready(conn, &srbmsg);
    } else {
      _mw_srbsendreject(conn, &srbmsg, NULL, NULL, SRB_PROTO_NOINIT);    
    };
    goto out;
  };    

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
      TIMEPEG();  
      if ( (srbmsg.marker == SRB_REQUESTMARKER) || 
	   (srbmsg.marker == SRB_NOTIFICATIONMARKER) )
	srbcall_req(conn, &srbmsg);
      else if (srbmsg.marker == SRB_RESPONSEMARKER)
	srbcall_rpl(conn, &srbmsg);
      else 
	// TODO: reject
	;

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
    if ( strcasecmp(srbmsg.command, SRB_UNPROVIDE) == 0) {
      srbunprovide(conn, &srbmsg);
      break;
    }
    break;

  default:
    _mw_srbsendreject_sz(conn, message, 0);
  };

 out:  
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

  //  _mw_srb_delfield(srbmsg, SRB_INSTANCE);
  //_mw_srb_delfield(srbmsg, SRB_CLIENTNAME);
  _mw_srb_delfield(srbmsg, SRB_NOREPLY);
  _mw_srb_delfield(srbmsg, SRB_MULTIPLE);
  //_mw_srb_delfield(srbmsg, SRB_MAXHOPS);

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

int _mw_srbsendunprovide(Connection * conn, char * service)
{
  int rc;
  SRBmessage srbmsg;
  
  DEBUG2("begin");
  _mw_srb_init(&srbmsg, SRB_UNPROVIDE, SRB_NOTIFICATIONMARKER, 
	       SRB_SVCNAME, service,
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

  Assert (peerinfo != NULL);
  
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
