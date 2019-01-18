/*
  MidWay
  Copyright (C) 2000,2005 Terje Eggestad

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

#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>
#include <string.h>
#include <signal.h>

#include <MidWay.h>
#include <SRBprotocol.h>
#include "SRBprotocolServer.h"
#include <multicast.h>
#include <tasks.h>
#include "tcpserver.h"
#include "gateway.h"
#include "ipcserver.h"
#include "connections.h"
#include "broker.h"
#include "srbevents.h"

/* linux only???? seem to be a bug in /usr/include/bits/in.h 
   see /usr/include/bits/linux/in.h 
*/
#ifndef IP_MTU
#define IP_MTU          14
#endif

static int tcpshutdown = 0;
static int reconnect_broker = 0;

/* setup global data. */
void tcpserverinit(void)
{

  conn_init();

  /* the send buffer is located in the lib/SRBclient.c and is not
     alloc'ed until we really know we need it. */
  if (_mw_srbmessagebuffer == NULL) _mw_srbmessagebuffer = malloc(SRBMESSAGEMAXLEN+100);

  DEBUG2( "tcpserverinit: completed");
  return;
};

/* begin listening on a gven port and set role of port. */
int tcpstartlisten(int port, int role)
{
  int s, rc, len;
  char buffer [1024];
  int blen = 1024;
  Connection  * conn;
  SockAddress addr;

  if ( (role < 1) || (role > 3) ) {
    errno = EINVAL;
    return -1;
  };
  
  if ( (port < 1) || (port > 0xffff) ) {
    errno = EINVAL;
    return -1;
  };

  s = socket(AF_INET, SOCK_STREAM, 0);

  /* set reuse adress for faster restarts. */
  rc = setsockopt(s, SOL_SOCKET, SO_REUSEADDR, buffer, blen);
  DEBUG("setsockopt SO_REUSESDDR returned %d, errno = %d", 
	rc, errno);

 
  conn = conn_add(s, role, CONN_TYPE_LISTEN);

  /* at some point we shall be able to specify which local interfaces
     we listen on but for now we use any. */
  addr.sin4.sin_addr.s_addr = INADDR_ANY ;
  addr.sin4.sin_port = htons(port);
  addr.sin4.sin_family = AF_INET;
  len = sizeof(struct sockaddr_in);

  rc = bind(s, &addr.sa, len);
  if (rc == -1) {
    Error("Attempted to bind socket to port %d but failed, reason %s", 
	  port, strerror(errno));
    close (s);
    return -1;
  };
  
  errno = 0;
  * (int *) buffer = 1;

  if (role & SRB_ROLE_CLIENT) 
    Info("Listening on port %d for clients", port);
      
  if (role & SRB_ROLE_GATEWAY) 
    Info("Listening on port %d for gateways", port);

  rc = listen (s, 10);
  if (rc == -1) {
    Error("Attempted to start listening on port %d but failed, reason %s", 
	  port, strerror(errno));
    close (s);
    return -1;
  };

  DEBUG("Listening on socket with fd = %d", s);
  return s;
};

/* for every time we get a new connection we call here to accept ot
   and pace it into the list of sockets to do select on.*/
static Connection *  tcpnewconnection(Connection * listensocket)
{
  int mtu=-1, rc, fd;
  SockAddress addr;
  socklen_t len;
  Connection * conn;

  /* fom gateway.c */
  
  errno = 0;
  len = sizeof(addr);

  fd = accept(listensocket->fd,  &addr.sa, &len);
  if (fd == -1) {
    return NULL;
  };

  if (tcpshutdown) {
    close (fd);
    return NULL;
  };

  conn = conn_add(fd,  listensocket->role, CONN_TYPE_CLIENT);
  
  memcpy(&conn->peeraddr.sa, &addr.sa, len);


  _mw_srbsendready(conn, globals.mydomain);
  /* Now we get the MTU of the socket. if MTU is less than
     MWMAXMESSAGELEN, but over a size like 256 we limit messages to
     that size. to get some control over fragmetation. This is
     probably crap on TCP but very important should we do UDP. */
  len = sizeof(int);
  rc = getsockopt(fd, IPPROTO_IP, IP_MTU, &mtu, &len);
  DEBUG("Got new connection fd = %d, MTU=%d errno = %s from %s:%d",
          fd, mtu, strerror(errno), inet_ntoa(addr.sin4.sin_addr),
	  ntohs(addr.sin4.sin_port));

  return conn;
};

/* close the socket, and remove it from the list of fd to do select on. */  
void tcpcloseconnection(Connection * conn)
{

  TIMEPEGNOTE("begin");

  DEBUG("on  fd=%d cid %#x", conn->fd, conn->cid);

  if (conn->cid != UNASSIGNED) {
    storeSRCClearClient(conn->cid);
    srb_unsubscribe_all(conn);
    gwdetachclient(conn->cid);

  } else if (conn->type == CONN_TYPE_BROKER) {
    reconnect_broker = 1;   

  } else if ((conn->type == CONN_TYPE_GATEWAY) && (conn->type != UNASSIGNED)) {
     gw_closegateway(conn);
    return;
  };

  /* TODO: what if a listen socket closes, reall can only happen if
     the OS is going down (on way or another */
  TIMEPEGNOTE("conn del");
  conn_del(conn->fd);
  TIMEPEGNOTE("end");
};

/* called on shutdown, the shutdown flag will stop all new
   connections, and on all open (none listen) sockets
   send a "TERM!" and call tcpcloseconnection(). */
void tcpcloseall(void)
{
  Connection * conn;

  tcpshutdown = 1; 
  DEBUG("closing all connections");

  while ( (conn = conn_getbroker()) != NULL) {
    DEBUG("closing broker");
    _mw_srbsendterm(conn, -1);
    conn_del(conn->fd);
  };

  while (  (conn = conn_getfirstlisten()) != NULL) {
    DEBUG("closing listen");
    conn_del(conn->fd);
  };
  
  while (  (conn = conn_getfirstclient()) != NULL) {
    DEBUG("closing client fd=%d CLIENTID=%d", conn->fd, conn->cid&MWINDEXMASK);
    _mw_srbsendterm(conn, -1);
    conn_del(conn->fd);
  };

  while (  (conn = conn_getfirstgateway()) != NULL) {
    DEBUG("closing gateway fd=%d GATEWAYID=%d", conn->fd, conn->cid&MWINDEXMASK);
    _mw_srbsendterm(conn, -1);
    gw_closegateway(conn);
  };

  return;
};

/* now we do the actuall read of a message, remember that a message
   shall never be longer than 3000++ octets, we set it io 3100 to be
   safe. 
   
   We also must handle multpile and partial messages.
*/

static void gwreadmessage(Connection * conn)
{
  SRBmessage * srbmsg;

  if (conn == NULL) return;

  TIMEPEGNOTE("begin");
  DEBUG2("read a message from fd=%d", conn->fd);
  errno = 0;
  srbmsg = _mw_srb_recvmessage(conn, MWNOBLOCK);
  TIMEPEG();

  DEBUG2("read a message from fd=%d returned %p errno=%s", conn->fd,  srbmsg, _mw_errno2str());

  /* read return 0 on end of file, and -1 on error. */
  if (srbmsg == NULL) {
     switch (errno) {

     case EAGAIN:
     case ENOMSG:
	return;

     default:
	tcpcloseconnection(conn);
	return ;
     };
  };

  srbDoMessage(conn, srbmsg);
  _mw_srb_destroy(srbmsg);
  return;
};
		

void sig_initshutdown(int sig)
{
  if (globals.shutdownflag == 0) {
    globals.shutdownflag = 1;
    if (getpid() != globals.tcpserverpid)
      kill(globals.tcpserverpid, sig);
    if (getpid() != globals.ipcserverpid)
      kill(globals.ipcserverpid, sig);
  };
};

static int alarms = 0;
void sig_alarm(int sig)
{
  alarms++;
};

void sig_dummy(int sig)
{
  ;
};

int tcp_do_error_condiion(Connection * conn)
{
  
  DEBUG("error condition on fd=%d", conn->fd);

  if (conn->type == CONN_TYPE_LISTEN) {
    Error("a listen socket (fd=%d) has dissapeared, really can't happen.",
	   conn->fd);
    sig_initshutdown(0);
  } else if (conn->type == CONN_TYPE_BROKER) {
    tcpcloseconnection(conn);
    conn_del(conn->fd);
    reconnect_broker = 1;
  } else { // client or peer
    tcpcloseconnection(conn);
  };
  return 0;
};
   
int tcp_do_write_condiion(Connection * conn)
{
  DEBUG("write condition on fd=%d", conn->fd);
  if (conn->type == CONN_TYPE_GATEWAY) {
    /* the other side shall send an SRB READY, we don't send an INIT
       until that is received, thus the task of sending an INIT falls
       on dosrbready() not here. We just set the state and unpoll from write events.*/
    if (conn->state == CONNECT_STATE_CONNWAIT) {	
      conn->state = CONNECT_STATE_READYWAIT;
      unpoll_write(conn->fd);
    };
    return 0;
  };
  return -1;
};

int tcp_do_read_condiion(Connection * conn)
{
  int fd;
  char * msg;
  int len, c_fd;
  SRBmessage * srbmsg;

  fd = conn->fd;
  DEBUG("read condition on fd=%d", conn->fd);
  /* new connection from broker */
  if (conn->type == CONN_TYPE_BROKER) {
    msg = malloc(SRBMESSAGEMAXLEN+1);
    len = read_with_fd(fd, msg, SRBMESSAGEMAXLEN, &c_fd);
    
    DEBUG("read %d bytes with new fd (%d) from broker", len, c_fd);
    /* if message from broker */
    if (c_fd == -1) {

      /* the only legal message from the broker is TERM! so we don't
         bother to decode it */
      
      conn_del(fd);
      
      /* if the broker is going down, there is no point in
	 immediate reconnect, hence 2, not 1 */
      reconnect_broker = 2; 
      return 0;
    };
    fd = c_fd;
    /* since the broker must send a complete SRB INIT?, and it's
       illegal for the client/peer gateway to send anything until a
       SRB INIT. is received we know that we shall have one and only
       one message in msg buffer. Anything more is an attack or broken
       client
    */
    
    /* clear \r\n at end before we trace and decode */
    msg[len-2] = '\0'; 
    len -= 2;
    
    conn = conn_add(fd, SRB_ROLE_CLIENT|SRB_ROLE_GATEWAY, 0);
    _mw_srb_trace(SRB_TRACE_IN, conn, msg, len);
    conn->lastrx = time(NULL);
    memcpy(conn->messagebuffer, msg, len+1);
    free(msg);
    TIMEPEG();

    srbmsg = _mw_srbdecodemessage(conn, conn->messagebuffer, len);

    if (srbmsg) {
       srbDoMessage(conn, srbmsg);
       DEBUG("srbDomessage completed");
       free(srbmsg);
       
    } else {
       Warning ("got a message from broker that's wrongly formated");
       conn_del(conn->fd);
    };

    return 0;
  };
      
  if (conn->type == CONN_TYPE_LISTEN) {
    DEBUG("new connection on listen socket");
    conn = tcpnewconnection(conn);
    return 0;
  };
  
  DEBUG("doing gwreadmessage on fd %d", conn->fd);
  TIMEPEG();
  gwreadmessage(conn);
  return 0;
}; 

/************************************************************************
 * timer task, is called every time conn_select() times out */

static double  task_interval = 60.0 /* secs */;
static double  fast_task_interval = 1.0 /* secs */;

/* the timeout is the next time in unix time that we shall be
   called. initial value of 1 just ensure that it will be called first
   time out */

static PTask slowtask;
static PTask fasttask;

static time_t  timeout = -1; 

// the slow task
int timer_task_1(PTask pt)
{
  int fd;

  DEBUG("vvvvvvvvvv starting slow timer task");
  if (globals.mydomain && globals.myinstance) {
    /* reconnect broker */
    if (reconnect_broker) {
      fd = connectbroker(globals.mydomain, globals.myinstance);
      DEBUG("connectbroker returned %d %d", fd, errno);
      if (fd != -1) {
	reconnect_broker = 0;
      } else {
	Error("connectbroker failed errno %d", errno);
      };
    } 

    gw_connectpeers();
    
    /* do mcasts */
    gw_sendmcasts();

  };

  DEBUG("^^^^^^^^^^ ending timer task");

  return 0;
};

// the fast task
int timer_task_2(PTask pt)
{
  int rc;

  DEBUG("vvvvvvvvvv starting fast timer task");

  rc = flush_ipc_fifo();
  DEBUG("fifo queue = %d", rc);
  if (rc == 0) {
     DEBUG("suspending fast task");
     mwsuspendtask(pt);
  };

  DEBUG("^^^^^^^^^^ ending fast timer task ");

  return 0;
}

void wake_fast_task(void)
{
  if (_mw_gettaskstate(fasttask) == TASK_SUSP) {
    DEBUG("waking fast task");
    mwresumetask(fasttask);
  };
  return;
};
  
/* the main loop, here we select on all sockets and issue actions. */
void * tcpservermainloop(void * param)
{
  int cond, rc;
  Connection * conn;
  char * penv;
  double d;

  globals.tcpserverpid = getpid();
  Info("tcpserver thread starting with pid=%d", globals.tcpserverpid);

  /* TODO: what are these doing here, and not in gateway.c */
  signal(SIGINT, sig_initshutdown);
  signal(SIGTERM, sig_initshutdown);
  signal(SIGQUIT, sig_initshutdown);
  signal(SIGHUP, sig_initshutdown);

  signal(SIGPIPE, sig_dummy);
  signal(SIGALRM, sig_alarm);

  /* if we're in a domain, nad not in a standalone mode */
  if (globals.mydomain && globals.myinstance) {
    reconnect_broker = 1;
  };

  if ((penv = getenv ("MWGWD_TASK_INTERVAL"))) {
    d = atof(penv);
    if (d > 0) task_interval = d;
  };

  if ((penv = getenv ("MWGWD_FAST_TASK_INTERVAL"))) {
    d = atof(penv);
    if (d > 0) fast_task_interval = d;
  };

  Info("slow task interval %lg", task_interval);
  slowtask = mwaddtaskdelayed(timer_task_1, task_interval, 1);
  Info("fast task interval %lg", fast_task_interval);
  fasttask = mwaddtaskdelayed(timer_task_2, fast_task_interval, -1);
  mwsuspendtask(fasttask);

  while(! globals.shutdownflag) {

    errno = 0;
    DEBUG2("%s", conn_print());
 
    conn = NULL;
    rc= conn_select(&conn, &cond, timeout);
    DEBUG2("conn_select returned %d errno=%d", rc, errno);

    TIMEPEG();
    if (rc < 0) {
      if ((rc == -ETIME) || (rc == -EINTR) ) {

	DEBUG("conn_select returned timeout or interrupted, doing tasks");
	mwblocksigalarm();
	alarms = 0;
	while(mwdotasks());
	mwunblocksigalarm();
	DEBUG("tasks completed");
	continue;
      };
      
      Error("conn_select returned error %d, shutingdown", rc);
      globals.shutdownflag = 1;
      continue;
    }
    TIMEPEG();
    Assert (conn);
    DEBUG("conn info fd=%d listen=%d broker=%d cond=%#x", 
	  conn->fd, conn->type == CONN_TYPE_LISTEN, conn->type == CONN_TYPE_BROKER, cond);
    TIMEPEG();
    if (cond & COND_ERROR) {
      rc = tcp_do_error_condiion(conn);
      continue;
    };
      
    if (cond & COND_WRITE) {
      rc = tcp_do_write_condiion(conn);
    };
    if (cond & COND_READ) {
      rc = tcp_do_read_condiion(conn);
    };
    
  };

  tcpcloseall();
  Info("Connections closed tcp thread is now dead");

  * (int *) param = 0;
  return param;
};
  
/* Emacs C indention
Local variables:
c-basic-offset: 2
End:
*/




