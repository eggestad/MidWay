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

/* $Id$ */

static char * RCSId = "$Id$";
static char * RCSName = "$Name$"; /* CVS TAG */

/*
 * $Log$
 * Revision 1.3  2001/09/15 23:49:38  eggestad
 * Updates for the broker daemon
 * better modulatization of the code
 *
 * Revision 1.2  2000/08/31 22:06:54  eggestad
 * - srbsend*() funcs now has a _mw_ prefix
 * - all direct use of urlmap, and sprintf() in making messages, we now use SRBmsg structs
 *
 * Revision 1.1  2000/07/20 18:49:59  eggestad
 * The SRB daemon
 *
 */
#include <pthread.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <limits.h>
#include <signal.h>

#include <MidWay.h>
#include <SRBprotocol.h>
#include <ipctables.h>
#include <ipcmessages.h>
#include <urlencode.h>
#include <shmalloc.h>
#include <version.h>

#define _GATEWAY_C
#include "SRBprotocolServer.h"
#include "gateway.h"
#include "tcpserver.h"
#include "store.h"
#include "connections.h"

/***********************************************************************
 * This modules differs a bit from every one else in that is
 * threaded. The reason for threading is that one thread need to be in
 * blocking wait in the select call in tcpserver.c:tcpservermainloop
 * and the other here in ipcmainloop(). Since we need to report our
 * pid to mwd, and we want to attach to mwd as a local gateway before
 * starting to listen, the inside doing IPC is not the new thread, but
 * the "program".
 *
 * In this rather preliminary version we only allow one listen port
 * per mwgwd, but there shall not be any limitations on how many port
 * a mwgwd can listen on.
 *
 * what needs to be determined if we need a gateway table for each
 * port, or just for each domainname, or if there shall be a 1 to 1
 * between the two. So far we only use 1.
 ***********************************************************************/

ipcmaininfo * ipcmain = NULL;
gatewayentry * gwtbl = NULL;

pid_t my_gw_pid = 0;
pid_t tcpserver_pid = 0;

globaldata globals = { 0 };

void usage(char * arg0)
{
  printf ("%s: [-A uri] [-l level] [-c clientport] [-g gatewayport] [-p commonport] [domainname]\n",
	  arg0);
};


int ipcmainloop(void)
{
  int i, rc, errors, len, mqid;
  char message[MWMSGMAX];
  char * data;
  int fd, connid;
  SRBmessage srbmsg;
  cliententry * cltent;

  /* really a union with the message buffer*/
  long *           mtype  = (long *)           message;
  
  Attach *         amsg   = (Attach *)         message;
  Administrative * admmsg = (Administrative *) message;
  Provide *        pmsg   = (Provide *)        message;
  Call *           cmsg   = (Call *)           message;

  memset (&srbmsg, '\0', sizeof(SRBmessage));
  i = _mw_get_my_gatewayid();
  i &= MWINDEXMASK;
  gwtbl[i].status = MWREADY;
  mwlog(MWLOG_INFO, "Ready.");
  
  errors = 0;

  while(!globals.shutdownflag) {
    len = MWMSGMAX;
    mwlog(MWLOG_DEBUG, "ipcmainloop: doing _mw_ipc_getmessage()");
    rc = _mw_ipc_getmessage(message, &len, 0, 0);
    mwlog(MWLOG_DEBUG, "ipcmainloop: _mw_ipc_getmessage() returned %d", rc);

    if (rc == -EIDRM) {
      globals.shutdownflag = TRUE;
      break;
    };
    if (rc == -EINTR) 
      continue;
    if (rc < 0) {
      errors++;
      if (errors > 10) {
	mwlog(MWLOG_ERROR, "To many errors while trying to read message queue");
	return -rc; /* going down on error */
      };
    }

    _mw_dumpmesg(message);

    switch (*mtype) {
    case ATTACHREQ:
    case DETACHREQ: 
      /* it is consiveable that we may add the possibility to
         disconnect spesific clients, but that may just as well be
         done thru a ADMREQ. We have so fare no provition for
         disconnecting clients other than mwd marking a client dead in
         shm tables. */
      mwlog(MWLOG_WARNING, "Got a attach/detach req from pid=%d, ignoring", amsg->pid);
      break;

    case ATTACHRPL:
      /* now this is in response to an attach. mwd must reply in
	 sequence, so this is the reply to my oldest pending request.
	 (not that it matters)*/
      strcpy(srbmsg.command, SRB_INIT);
      srbmsg.marker = SRB_RESPONSEMARKER;
      rc = storePopAttach(amsg->cltname, &connid, &fd, &srbmsg.map);
      if (rc != 1) {
	mwlog(MWLOG_WARNING, "Got a Attach reply that I was not expecting! clientname=%s gwid=%#x srvid=%#x", amsg->cltname, amsg->gwid, amsg->srvid); 

	/* we don't bother mwd with detaches, mwd will reuse if
           cltent->status == UNASSIGNED */
	cltent = _mw_get_client_byid(amsg->cltid);
	if (cltent != NULL) {
	  cltent->location = UNASSIGNED;
	  cltent->type = UNASSIGNED;
	  cltent->pid = UNASSIGNED;
	  cltent->mqid = UNASSIGNED;
	  cltent->clientname[0] = '\0';
	  cltent->username[0] = '\0';
	  /* hand over to mwd */
	  cltent->status = UNASSIGNED;
	};
	amsg->cltid |= MWCLIENTMASK;
	conn_setinfo(fd, &amsg->cltid, NULL, NULL, NULL);
	/* nothing more to do */
	break;
      };

      mwlog(MWLOG_DEBUG, "connection for clientname %s on fd=%d is assigned the clientid %#x", 
	    amsg->cltname, fd, &amsg->cltid);
      /* now assign the CLIENTID to the right connection. */
      conn_setinfo(fd, &amsg->cltid, NULL, NULL, NULL);
      _mw_srbsendinitreply(fd, &srbmsg, SRB_PROTO_OK, NULL);
      break;

    case DETACHRPL:
      /* do I really care? */
      break;

    case SVCCALL:
      /* gateway only, we have imported a service, find the remote gw
         and send the call */
      break;

    case SVCREPLY:
      mwlog(MWLOG_DEBUG, 
	    "Got a svcreply service %s from server %d for client %d, ipchandle=%#x", 
	    cmsg->service, cmsg->srvid&MWINDEXMASK, 
	    cmsg->cltid&MWINDEXMASK, cmsg->handle);

      /* we must lock since it happens that the server replies before
         we do storeSetIpcCall() in SRBprotocol.c */
      strcpy(srbmsg.command, SRB_SVCCALL);
      srbmsg.marker = SRB_RESPONSEMARKER;

      storeLockCall();
      rc = storePopCall(cmsg->cltid, cmsg->handle, &fd, &srbmsg.map);
      storeUnLockCall();

      if (!rc ) {
	mwlog(MWLOG_DEBUG, 
	      "Couldn't find a waiting call for this message, possible quite normal");
	break;
      };
      mwlog(MWLOG_DEBUG2, "storePopCall returned %d, address of old map is %#x", 
	    rc, &srbmsg.map);
      {
	int idx = 0;

	while(srbmsg.map[idx].key != NULL) {
	  mwlog(MWLOG_DEBUG2, "  Field %s => %s", 
		srbmsg.map[idx].key, srbmsg.map[idx].value);
	  idx++;
	};
      };
      if (cmsg->flags & MWNOREPLY) {
	mwlog(MWLOG_DEBUG, "Noreply flags set, ignoring");
	_mwfree(_mwoffset2adr(cmsg->data));
	break;
      };
      
      if (cmsg->datalen == 0) {
	data = NULL;
	len = 0;
      } else {
	len = cmsg->datalen;
	data = _mwoffset2adr(cmsg->data);
      };
      
      mwlog(MWLOG_DEBUG, "sending reply on fd=%d", fd);
      _mw_srbsendcallreply(fd, &srbmsg, data, len, 
		       cmsg->appreturncode,  cmsg->returncode, 
		       cmsg->flags); 
      mwlog(MWLOG_DEBUG, "reply sendt");
      urlmapfree(srbmsg.map);
      _mwfree(_mwoffset2adr(cmsg->data));
      /* may be either a client or gateway, if clientid is myself,
         then gateway. */
      break;

    case ADMREQ:
      /* the only adm request we accept right now is shutdown, we may
         later accept reconfig, and connect/disconnect gateways. */
      if (admmsg->opcode == ADMSHUTDOWN) {
	mwlog(MWLOG_INFO, "Received a shutdown command from clientid=%d", 
	      admmsg->cltid&MWINDEXMASK);
	return 2; /* going down on command */
      }; 
      break;

    case PROVIDEREQ:
      mwlog(MWLOG_WARNING, "Got a providereq from serverid=%d, sending rc=EBADRQC", 
	    pmsg->srvid&MWINDEXMASK);
      pmsg->returncode = -EBADRQC;
      _mw_ipc_putmessage(pmsg->srvid, message, len, IPC_NOWAIT);
      break;

    case PROVIDERPL:
      /* do I really care? */
      break;

    default:
      mwlog(MWLOG_WARNING, "got a unknown message with message type = %#x", *mtype);
    } /* end switch */
  } /* end while */
  return 1; /* going down on signal */
  
};

int gwattachclient(int connid, int fd, char * cname, char * username, char * password, urlmap * map)
{
  Attach mesg;
  int rc;

  /* first of all we should test authentication..... that means
     checking config for authentication level required for this
     gateway or client name, whatever, we need to code the config
     first. */
  
  if (connid <= 0) {
    Connection * conn;
    conn = conn_getentry(fd);
    if (conn) 
      connid = conn->connectionid;
  };
  storePushAttach(cname, connid, fd, map);

  mwlog(MWLOG_DEBUG,
	"gwattachclient(fd=%d, cname=\"%s\", connid=%d)", fd, cname, connid);

  mesg.mtype = ATTACHREQ;
  mesg.gwid = _mw_get_my_gatewayid(); 
  mesg.client = TRUE;    
  mesg.server = FALSE;
  strncpy(mesg.cltname, cname, MWMAXNAMELEN);
  mesg.pid = my_gw_pid;
  mesg.flags = 0;
  mesg.ipcqid = _mw_my_mqid();

  rc = _mw_ipc_putmessage(0, (void *) &mesg, sizeof(mesg) ,0);

  if (rc != 0) {
    mwlog (MWLOG_ERROR, 
	   "gwattachclient()=>%d failed with error %d(%s)", 
	   rc, errno, strerror(errno));
    return -errno;
  };
  return 0;
};


int gwdetachclient(int cltid)
{
  Attach mesg;
  int rc;

  /* first of all we should test authentication..... that means
     checking config for authentication level required for this
     gateway or client name, whatever, we need to code the config
     first. */
  
  mwlog(MWLOG_DEBUG,
	"gwdetachclient(clientid=%d) (%s)", MWINDEXMASK& cltid, conn_getclientpeername(cltid));

  memset (&mesg, '\0', sizeof(Attach));

  mesg.mtype = DETACHREQ;
  mesg.cltid = cltid | MWCLIENTMASK;
  mesg.gwid = _mw_get_my_gatewayid(); 
  mesg.client = TRUE;    
  mesg.server = FALSE;
  mesg.pid = my_gw_pid;
  mesg.flags = MWFORCE;
  mesg.ipcqid = _mw_my_mqid();

  rc = _mw_ipc_putmessage(0, (void *) &mesg, sizeof(mesg) ,0);

  if (rc != 0) {
    mwlog (MWLOG_ERROR, 
	   "gwdetachclient() putmessage =>%d failed with error %d(%s)", 
	   rc, errno, strerror(errno));
    return -errno;
  };
  return 0;
};

#define LOCK -1
#define UNLOCK 1
static int lockgwtbl(int lock_unlock)
{
  struct sembuf sop[1];

  if (ipcmain == NULL) {
    errno = ENOENT;
    return -1;
  };
  sop[0].sem_num = 0;
  sop[0].sem_op =  lock_unlock;
  sop[0].sem_flg = 0;

  return semop(ipcmain->gwtbl_lock_sem, sop, 1);
};

GATEWAYID allocgwid(int location, int role)
{
  int rc, i, idx;
  errno = 0;
  if ( (rc = lockgwtbl(LOCK)) < 0) {
    mwlog(MWLOG_ERROR, "Failed to lock the gateway table, reason: %s. Exiting...", 
	  strerror(errno));
    return -1;
  };
  mwlog(MWLOG_DEBUG, "start allocgwid(location=%d, role=%d) nextidx=%d max=%d", 
	location, role, ipcmain->gwtbl_nextidx, ipcmain->gwtbl_length);

  for (i = ipcmain->gwtbl_nextidx; i < ipcmain->gwtbl_length; i++) {
    if (gwtbl[i].pid == UNASSIGNED) {
      gwtbl[i].srbrole = role;
      gwtbl[i].mqid = _mw_my_mqid();
      gwtbl[i].pid = getpid();
      gwtbl[i].status = MWBOOTING;
      gwtbl[i].location = location;
      gwtbl[i].connected = time(NULL);

      mwlog(MWLOG_DEBUG, "allocgwid() returns %d (>)", i);
      ipcmain->gwtbl_nextidx = i+1;
      lockgwtbl(UNLOCK);
      return i|MWGATEWAYMASK;
    }
  }; 
  for (i = 0; i < ipcmain->gwtbl_nextidx; i++) {
    if (gwtbl[i].pid == UNASSIGNED) {
      gwtbl[i].srbrole = role;
      gwtbl[i].mqid = _mw_my_mqid();
      gwtbl[i].pid = getpid();
      gwtbl[i].status = MWBOOTING;
      gwtbl[i].location = location;
      gwtbl[i].connected = time(NULL);

      mwlog(MWLOG_DEBUG, "allocgwid() returns %d (<)", i);
      ipcmain->gwtbl_nextidx = i+1;
      lockgwtbl(UNLOCK);
      return i|MWGATEWAYMASK;
    }
  };
  /* what should we do if this fail? it really can't */
  rc = lockgwtbl(UNLOCK);
  mwlog(MWLOG_DEBUG, "allocgwid() returns %d", i);
  return -1;
};

void freegwid(GATEWAYID gwid)
{

  if (gwid == UNASSIGNED) return;
  if ( (gwid & ~MWINDEXMASK) != MWGATEWAYMASK) return;

  gwid &= MWINDEXMASK;

  lockgwtbl(LOCK);
  gwtbl[gwid].srbrole = 0;
  gwtbl[gwid].mqid = UNASSIGNED;
  gwtbl[gwid].status = MWDEAD;
  gwtbl[gwid].location = UNASSIGNED;
  gwtbl[gwid].pid = UNASSIGNED;
  lockgwtbl(UNLOCK);
};



int main(int argc, char ** argv)
{
  int loglevel, gateway = 0, client = 0, port = 11000;
  int role = 0;
  char * uri;
  char c, *name;
  int key = -1;

  char logprefix[PATH_MAX];

  pthread_t tcp_thread;
  int tcp_thread_rc;
  int rc = 0, i;

  loglevel = MWLOG_DEBUG2;
  /* first of all do command line options */
  while((c = getopt(argc,argv, "A:l:cgp:")) != EOF ){
    switch (c) {
    case 'l':
      if      (strcmp(optarg, "fatal")   == 0) loglevel=MWLOG_FATAL;
      else if (strcmp(optarg, "error")   == 0) loglevel=MWLOG_ERROR;
      else if (strcmp(optarg, "warning") == 0) loglevel=MWLOG_WARNING;
      else if (strcmp(optarg, "alert")   == 0) loglevel=MWLOG_ALERT;
      else if (strcmp(optarg, "info")    == 0) loglevel=MWLOG_INFO;
      else if (strcmp(optarg, "debug")   == 0) loglevel=MWLOG_DEBUG;
      else if (strcmp(optarg, "debug1")  == 0) loglevel=MWLOG_DEBUG1;
      else if (strcmp(optarg, "debug2")  == 0) loglevel=MWLOG_DEBUG2;
      else if (strcmp(optarg, "debug3")  == 0) loglevel=MWLOG_DEBUG3;
      else usage(argv[0]);
      break;
    case 'A':
      uri = strdup(optarg);
      break;
    case 'c':
      client = 1;
      break;
    case 'g':
      gateway = 1;
    case 'p':
      port = atoi(optarg);
      break;
    default:
      usage(argv[0]);
      break;
    }
  }

  if (argc == optind) {
    mwlog(MWLOG_DEBUG, "no domain name given");
  } else if (argc == optind+1) {
    mwlog(MWLOG_DEBUG, "domain name = %s", argv[optind]);
    globals.mydomain=argv[optind];
  } else {
    usage(argv[0]);
  };

  /* if neither g or c was giver, both is assumed.*/
  if ((gateway == 0) && (client == 0)) gateway = client = 1;
  if (client) role |= SRB_ROLE_CLIENT;
  if (gateway) role |= SRB_ROLE_GATEWAY;

  /* first we attach to the IPC tables of the running instance */
  if (key == -1) key = getuid();
  rc = _mw_attach_ipc(key, MWIPCSERVER);
  if (rc != 0) {
    mwlog(MWLOG_ERROR, "MidWay instance is not running");
    exit(rc);
  };
  ipcmain = _mw_ipcmaininfo();
  gwtbl = _mw_get_gateway_table();
  
  strncpy(logprefix, ipcmain->mw_homedir, 256);
  strcat(logprefix, "/log/SYSTEM");
  printf("logprefix = %s\n", logprefix);

  name = strrchr(argv[0], '/');
  if (name == NULL) name = argv[0];
  else name++;
  mwopenlog(name, logprefix, loglevel);

  mwlog(MWLOG_INFO, "MidWay GateWay Daemon version %s starting", mwversion());
  /* lock the gateway table and assign ourslef the first available entry */
  _mw_set_my_gatewayid(allocgwid(MWLOCAL,role));
  if (_mw_get_my_gatewayid() == UNASSIGNED) {
    mwlog(MWLOG_ERROR, "No entry was available in the gateway IPC table. Exiting...");
    _mw_detach_ipc();
    exit(-1);
  };

  my_gw_pid = getpid();

  /*********************************************************************************
  /* we can now start in earnest 
   *********************************************************************************/

  mwlog(MWLOG_INFO, "mwgwd starting gateway id %d domain=%s instance=%s", 
	MWINDEXMASK&_mw_get_my_gatewayid(), 
	globals.mydomain?globals.mydomain:"(none)", 
	ipcmain->mw_instance_name?ipcmain->mw_instance_name:"(none)"
	);
  globals.myinstance = ipcmain->mw_instance_name;
  /* now do the magic on the outside (TCP) */
  tcpserverinit ();
  rc = tcpstartlisten(port, role);
  if (rc == -1) {
    mwlog(MWLOG_ERROR, "Failed to init tcp server, reason %d", errno);
    exit(errno);
  };
  rc = pthread_create(&tcp_thread, NULL, tcpservermainloop, &tcp_thread_rc);
  mwlog(MWLOG_DEBUG, "tcp_thread has id %d,", tcp_thread);

  rc = ipcmainloop();
  mwlog(MWLOG_INFO, "ipcmainloop() returned %d, going down", rc);

  globals.shutdownflag = 1;

  mwlog(MWLOG_INFO, "Executing normal shutdown");
  /*  pthread_cancel(tcp_thread);*/
  tcpcloseall();
  kill(tcpserver_pid, SIGINT);
  pthread_join(tcp_thread, NULL);
 
  mwlog(MWLOG_INFO, "Connections closed");
  i = _mw_get_my_gatewayid();
  freegwid(i);

  _mw_detach_ipc();
  mwlog(MWLOG_INFO, "Bye!");
  exit(rc);
};
