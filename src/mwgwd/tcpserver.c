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
 * Revision 1.4  2001/10/03 22:39:30  eggestad
 * many bugfixes: memleak, propper shutdown, +++
 *
 * Revision 1.3  2001/09/15 23:49:38  eggestad
 * Updates for the broker daemon
 * better modulatization of the code
 *
 * Revision 1.2  2000/08/31 22:18:50  eggestad
 * - added some debugging
 * - sendmessage() now in lib
 * - we now sending a detach message to mwd when a client disconnects
 * - the end buffer is in lib and only alloc'ed if you use SRB.
 * - tcpgetclientpeername() reworked.
 *
 * Revision 1.1  2000/07/20 18:49:59  eggestad
 * The SRB daemon
 *
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
#include "tcpserver.h"
#include "gateway.h"
#include "connections.h"
#include "broker.h"

/* linux only???? seem to be a bug in /usr/include/bits/in.h 
   see /usr/include/bits/linux/in.h 
*/
#ifndef IP_MTU
#define IP_MTU          14
#endif

static int tcpshutdown = 0;
static int reconnect_broker = 0;

extern globaldata globals;

/* setup global data. */
void tcpserverinit(void)
{

  conn_init();

  /* the send buffer is located in the lib/SRBclient.c and is not
     alloc'ed until we really know we need it. */
  if (_mw_srbmessagebuffer == NULL) _mw_srbmessagebuffer = malloc(SRBMESSAGEMAXLEN+1);

  mwlog(MWLOG_DEBUG2, "tcpserverinit: completed");
  return;
};

/* begin listening on a gven port and set role of port. */
int tcpstartlisten(int port, int role)
{
  int i, s, rc, len;
  char buffer [1024];
  int blen = 1024;
  Connection  * conn;

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
  mwlog(MWLOG_DEBUG, "setsockopt SO_REUSESDDR returned %d, errno = %d", 
	rc, errno);

 
  conn = conn_add(s, role, CONN_LISTEN);

  /* at some point we shall be able to specify which local interfaces
     we listen on but for now we use any. */
  conn->ip4.sin_addr.s_addr = INADDR_ANY ;
  conn->ip4.sin_port = htons(port);
  conn->ip4.sin_family = AF_INET;
  len = sizeof(struct sockaddr_in);

  rc = bind(s, (struct sockaddr *) &conn->ip4, len);
  if (rc == -1) {
    mwlog(MWLOG_ERROR, "Attempted to bind socket to port %d but failed, reason %s", 
	  port, strerror(errno));
    close (s);
    return -1;
  };
  
  errno = 0;
  * (int *) buffer = 1;

  if (role & SRB_ROLE_CLIENT) 
    mwlog(MWLOG_INFO, "Listening on port %d for clients", port);
      
  if (role & SRB_ROLE_GATEWAY) 
    mwlog(MWLOG_INFO, "Listening on port %d for gateways", port);

  rc = listen (s, 10);
  if (rc == -1) {
    mwlog(MWLOG_ERROR, "Attempted to start listening on port %d but failed, reason %s", 
	  port, strerror(errno));
    close (s);
    return -1;
  };

  mwlog(MWLOG_DEBUG, "Listening on socket with fd = %d", s);
  return s;
};

/* for every time we get a new connection we call here to accept ot
   and pace it into the list of sockets to do select on.*/
static int newconnection(int listensocket)
{
  int mtu=-1, rc, fd, len;
  struct sockaddr_in sain;
  Connection * conn;

  /* fom gateway.c */
  
  errno = 0;
  len = sizeof(struct sockaddr_in);

  fd = accept(listensocket, (struct sockaddr *) &sain, &len);
  if (fd == -1) {
    return -1;
  };

  if (tcpshutdown) {
    close (fd);
    return 0;
  };

  conn = conn_add(fd,  conn_getentry(listensocket)->role, CONN_CLIENT);
  conn->messagebuffer = (char *) malloc(SRBMESSAGEMAXLEN+1);
  
  if (sain.sin_family == AF_INET) {
    memcpy(&conn->ip4, (struct sockaddr *) &sain, len);
  };

  _mw_srbsendready(fd, globals.mydomain);
  /* Now we get the MTU of the socket. if MTU is less than
     MWMAXMESSAGELEN, but over a size like 256 we limit messages to
     that size. to get some control over fragmetation. This is
     probably crap on TCP but very important should we do UDP. */
  len = sizeof(int);
  rc = getsockopt(fd, SOL_IP, IP_MTU, &mtu, &len);
  mwlog(MWLOG_DEBUG, "Got new connection fd = %d, MTU=%d errno = %s from %s:%d",
          fd, mtu, strerror(errno), inet_ntoa(sain.sin_addr),
	  ntohs(sain.sin_port));

  return fd;
};

/* close the socket, and remove it from the list of fd to do select on. */  
void tcpcloseconnection(int fd)
{
  Connection * conn;

  conn = conn_getentry(fd);

  mwlog(MWLOG_DEBUG, "tcpcloseconnection on  fd=%d", fd);
  if (conn->client != UNASSIGNED) 
    gwdetachclient(conn->client);
  else if (conn->broker) {
    reconnect_broker = 1;   
  };
  /* TODO: what if a listen socket closes, reall can only happen if
     the OS is going down (on way or another */
  conn_del(fd);
};

/* called on shutdown, the shutdown flag will stop all new
   connections, and on all open (none listen) sockets
   send a "TERM!" and call tcpcloseconnection(). */
void tcpcloseall(void)
{
  int i;
  Connection * conn;

  tcpshutdown = 1; 
  mwlog(MWLOG_DEBUG, "closing all connections");

  while ( conn = conn_getbroker()) {
    mwlog(MWLOG_DEBUG, "closing broker");
    _mw_srbsendterm(conn->fd, -1);
    conn_del(conn->fd);
  };

  while ( conn = conn_getfirstlisten()) {
    mwlog(MWLOG_DEBUG, "closing listen");
    conn_del(conn->fd);
  };
  
  while ( conn = conn_getfirstclient()) {
    mwlog(MWLOG_DEBUG, "closing client fd=%d", conn->fd);
    _mw_srbsendterm(conn->fd, -1);
    conn_del(conn->fd);
  };

  return;
};

/* now we do the actuall read of a message, remember that a message
   shall never be longer than 3000++ octets, we set it io 3100 to be
   safe. 
   
   We also must handle multpile and partial messages.
*/

static void readmessage(int fd)
{
  int i, n, end, start = 0;
  Connection * conn;

  conn = conn_getentry(fd);

  n = read(fd, conn->messagebuffer+conn->leftover, 
	       SRBMESSAGEMAXLEN-conn->leftover);

  /* read return 0 on end of file, and -1 on error. */
  if ((n == -1) || (n == 0)) {
    tcpcloseconnection(fd);
    return ;
  };

  end = conn->leftover + n;
  mwlog(MWLOG_DEBUG, "readmessage(fd=%d) read %d bytes + %d leftover buffer now:%.*s", 
	fd, n, conn->leftover, end, conn->messagebuffer);

  
  /* we start at leftover since there can't be a \r\n in what was
     leftover from the last time we read from the connection. */
  for (i = conn->leftover ; i < end; i++) {
    
    /* if we find a message end, process it */
    if ( (conn->messagebuffer[i] == '\r') && 
	 (conn->messagebuffer[i+1] == '\n') ) {
      conn->messagebuffer[i] = '\0';

      mwlog(MWLOG_DEBUG, "read a message from fd=%d, about to process with srbDomessage", fd);

      _mw_srb_trace(SRB_TRACE_IN, fd, conn->messagebuffer, i);

      conn->lastrx = time(NULL);
      srbDoMessage(fd, conn->messagebuffer+start);
      mwlog(MWLOG_DEBUG, "srbDomessage completed");

      i += 2;
      start = i;
    };
  };
  
  /* nothing leftover, normal case */
  if (start == end) { 
    conn->leftover = 0;
    return;
  };
  
  /* test for message overflow */
  if ( (end - start) >= SRBMESSAGEMAXLEN) {
    mwlog(MWLOG_WARNING, "readmessage(fd=%d) message buffer was exceeded, rejecting it, next message should be rejected", fd);
    conn->leftover = 0;
    return;
  };  

  /* now there must be left over, move it to the beginning of the
     message buffer */
  conn->leftover = end - start;
  mwlog(MWLOG_DEBUG2, "memcpy %d leftover %p => %p", 
	conn->leftover, conn->messagebuffer + start,conn->messagebuffer);
  memcpy(conn->messagebuffer, conn->messagebuffer + start, 
	 conn->leftover);

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

void sig_dummy(int sig)
{
  ;
};


/* the main loop, here we select on all sockets and issue actions. */
void * tcpservermainloop(void * param)
{
  int fd, cond, timeout = 0;
  extern pid_t tcpserver_pid;
  Connection * conn;
  
  globals.tcpserverpid = getpid();
  mwlog(MWLOG_INFO, "tcpserver thread starting with pid=%d", globals.tcpserverpid);

  /* TODO: what are these doing here, and not in gateway.c */
  signal(SIGINT, sig_initshutdown);
  signal(SIGTERM, sig_initshutdown);
  signal(SIGQUIT, sig_initshutdown);
  signal(SIGHUP, sig_initshutdown);

  signal(SIGPIPE, sig_dummy);

  if (globals.mydomain && globals.myinstance) {
    reconnect_broker = 1;
  };

  while(! globals.shutdownflag) {
 
    if (reconnect_broker == 0) {
      timeout = -1;
    } else if (reconnect_broker == 1) {
	timeout = time(NULL);
	reconnect_broker++;
    } else {
      timeout = time(NULL) + BROKER_RECONNECT_TIMER;
    };

    fd = do_select(&cond, timeout);
    mwlog(MWLOG_DEBUG, "do_select returned %d errno=%d", fd, errno);

    if (fd < 0) {
    
      if (errno == ETIME) {
	/* reconnect broker */
	if (reconnect_broker) {
	  fd = connectbroker(globals.mydomain, globals.myinstance);
	  mwlog(MWLOG_DEBUG, "connectbroker returned %d", fd);
	  if (fd != -1) {
	    conn_add(fd, SRB_ROLE_CLIENT|SRB_ROLE_GATEWAY, CONN_BROKER);
	    reconnect_broker = 0;
	  };
	  continue;
	} 
	mwlog(MWLOG_ERROR, "This can't happen, do_select returned 0, " 
	      "but we're not trying to reconnect the broker");
	continue;
      };
      mwlog(MWLOG_ERROR, "do_select returned error %d, shutingdown", errno);
      globals.shutdownflag = 1;
      continue;
    }

    conn = conn_getentry(fd);
    mwlog(MWLOG_DEBUG, "conn info listen=%d broker=%d", conn->listen, conn->broker);

    if (cond == COND_ERROR) {
      mwlog(MWLOG_DEBUG, "error condition on fd=%d", fd);
      
      if (conn->listen == FALSE) tcpcloseconnection(fd);
      else if (conn->broker == TRUE) {
	tcpcloseconnection(fd);
	timeout = BROKER_RECONNECT_TIMER;
	conn_del(fd);
	reconnect_broker = 1;
      } else {
	mwlog (MWLOG_ERROR, 
	       "a listen socket (fd=%d) %s:%d has dissapeared, really can't happen.",
	       fd, inet_ntoa(conn->ip4.sin_addr), 
	       ntohs(conn->ip4.sin_port));
	* (int *) param = -1;
	return param;
      };
      
    };
      
    if (cond == COND_READ) {

      mwlog(MWLOG_DEBUG, "read condition on fd=%d", fd);
      /* new connection from broker */
      if (conn->broker) {
	char * msg;
	int len, c_fd;

	msg = malloc(SRBMESSAGEMAXLEN+1);
	len = read_with_fd(fd, msg, SRBMESSAGEMAXLEN, &c_fd);

	mwlog(MWLOG_DEBUG, "read %d bytes with new fd (%d) from broker", len, c_fd);
	/* if message from broker */
	if (c_fd == -1) {
	  /* the only legal message from the broker is TERM! */

	  conn_del(fd);

	  /* if the broker is going down, there is no point in
             immediate reconnect, hence 2, not 1 */
	  reconnect_broker = 2; 
	  continue;
	};
	fd = c_fd;
	/* since the broker must send a complete SRB INIT?, and it's
	   illegal for the client/peer gateway to send anything until
	   a SRB INIT. is received we know that we shall have one and
	   only one message in msg */

	/* clear \r\n at end before we trace and decode */
	msg[len-2] = '\0'; 
	len -= 2;

	_mw_srb_trace(SRB_TRACE_IN, fd, msg, len);
	conn = conn_add(fd, SRB_ROLE_CLIENT|SRB_ROLE_GATEWAY, 0);
	conn->lastrx = time(NULL);
	conn->messagebuffer = msg;	 
	srbDoMessage(fd, conn->messagebuffer);

	mwlog(MWLOG_DEBUG, "srbDomessage completed");
	continue;
      };
      
      if (conn->listen) {
	mwlog(MWLOG_DEBUG, "new connection on listen socket");
	fd = newconnection(fd);
      };
      
      mwlog(MWLOG_DEBUG, "doing readmessage on fd %d", fd);
      readmessage(fd);
      
    };

  };

  tcpcloseall();
  mwlog(MWLOG_INFO, "Connections closed");

  * (int *) param = 0;
  return param;
};
  




