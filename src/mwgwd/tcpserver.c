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

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

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

/* linux only???? seem to be a bug in /usr/include/bits/in.h 
   see /usr/include/bits/linux/in.h 
*/
#ifndef IP_MTU
#define IP_MTU          14
#endif

/* information on every connection. Most importantly it keeps
   the message buffer, and if this is a client or gateway,
*/
typedef struct {
  int connectionid;
  CLIENTID  client;
  GATEWAYID gateway;
  int    role;
  int    listen;
  char * messagebuffer;
  int    leftover;  
  int    protocol;
  struct sockaddr_in  ip4;
  struct sockaddr_in6 ip6;
  int    mtu;
  time_t connected;
  time_t lasttx;
  time_t lastrx;
  int 	 rejects;
  int    reverse;
} Connection;

static Connection connections[FD_SETSIZE];

static fd_set sockettable;

static int maxsocket = -1;

/*
int mapClientConnection[FD_SETSIZE];
int mapGateWayConnection[FD_SETSIZE];
*/

/* a global trace flag for, now, needs replament before release */
static int trace = 1;

static int tcpshutdown = 0;

/* we have a connection id that is unique, this is to prevent
   accidential sending of messages that was for the peer that had the
   fd, disconnected, and a new peer now has the fd. This is particual
   important on attaches, in calls, both fd, and client id must match
   also. */
static int lastconnectionid ;


extern globaldata globals;

/* srb*conninfo are used to set info in SRB INIT and to get info later
   on requests that only gateways may request. */
void tcpsetconninfo(int fd, int * id, int * role, int * rejects, int * reverse)
{
  
  if (id != NULL) {
    mwlog(MWLOG_DEBUG2, "tcpsetconninfo fd=%d id=%#x", fd, *id);
    if ((*id & MWCLIENTMASK) != 0)
      connections[fd].client = (*id & MWINDEXMASK) | MWCLIENTMASK;
    if ((*id & MWGATEWAYMASK) != 0)
      connections[fd].gateway = (*id & MWINDEXMASK) | MWGATEWAYMASK;
  };

  if (role != NULL) {
    mwlog(MWLOG_DEBUG2, "tcpsetconninfo fd=%d role=%d", fd, *role);
    connections[fd].role = *role;
  }
  if (rejects != NULL) {
    mwlog(MWLOG_DEBUG2, "tcpsetconninfo fd=%d rejects=%d", fd, *rejects);
    connections[fd].rejects = *rejects;
  }
  if (reverse != NULL) {
    mwlog(MWLOG_DEBUG2, "tcpsetconninfo fd=%d reverse=%d", fd, *reverse);
    connections[fd].reverse = *reverse;
  }
};

int tcpgetconninfo(int fd, int * id, int * role, int * rejects, int * reverse)
{
  if (id != NULL)
    if (connections[fd].client != UNASSIGNED) 
      *id = connections[fd].client | MWCLIENTMASK;
    else if (connections[fd].gateway != UNASSIGNED) 
      *id = connections[fd].gateway | MWGATEWAYMASK;
    else {
      *id = UNASSIGNED;
    };

  if (role != NULL)
    *role = connections[fd].role;

  if (rejects != NULL)
    *rejects = connections[fd].rejects;

  if (reverse != NULL)
    *reverse = connections[fd].reverse;
  
  return 0;
};

int tcpgetconnid(int fd)
{
  return connections[fd].connectionid;
};

char * tcpgetconnpeername (int fd)
{
  static char name[100];
  char * ipadr, * ipname;
  struct hostent *hent ;
  int len  = 0;

  ipadr = inet_ntoa(connections[fd].ip4.sin_addr);
  hent = gethostbyaddr(ipadr, strlen(ipadr), AF_INET);
  if (hent == NULL)
    ipname = ipadr;
  else 
    ipname = hent->h_name;

  if (connections[fd].client != UNASSIGNED) {
    len = sprintf(name, "client %d at ", connections[fd].client & MWINDEXMASK);
  } else if (connections[fd].gateway != UNASSIGNED) {
    len = sprintf(name, "gateway %d at ", connections[fd].gateway & MWINDEXMASK);
  };
  sprintf(name+len, "%s (%s) port %d", ipname, ipadr,
	  ntohs(connections[fd].ip4.sin_port));
  return name;
};

char * tcpgetclientpeername (CLIENTID cid)
{
  static char name[100];
  char * ipadr, * ipname;
  struct hostent *hent ;
  int i, fd = -1;
  
  cid |= MWCLIENTMASK;

  for (i = 0; i <= maxsocket; i++) {
    mwlog(MWLOG_DEBUG2, "tcpgetclientpeername: foreach fd=%d cid=%#x ?= %#x",
	  i, cid, connections[i].client);
    if (connections[i].client == cid) {
      fd = i;
      break;
    }
  };

  mwlog(MWLOG_DEBUG2, "tcpgetclientpeername: cid=%#x fd = %d",cid, fd);
  if (fd == -1) return NULL;
  mwlog(MWLOG_DEBUG2, "tcpgetclientpeername: cid=%#x fd = %d role: %d ?= %d",
	cid, fd, connections[fd].role, SRB_ROLE_CLIENT);
  if (connections[fd].role != SRB_ROLE_CLIENT) return NULL;

  ipadr = inet_ntoa(connections[fd].ip4.sin_addr);
  hent = gethostbyaddr(ipadr, strlen(ipadr), AF_INET);
  if (hent == NULL)
    ipname = ipadr;
  else 
    ipname = hent->h_name;

  sprintf(name, "client %d at %s (%s) port %d", 
	  MWINDEXMASK & connections[fd].client, 
	  ipname, ipadr,
	  ntohs(connections[fd].ip4.sin_port));
  return name;
};
  

/* setup global data. */
void tcpserverinit(void)
{
  int i;

  /* clean the connections table */
  for (i = 0; i < FD_SETSIZE; i++){
    connections[i].client = UNASSIGNED;
    connections[i].gateway = UNASSIGNED;
    connections[i].messagebuffer = NULL;
    connections[i].leftover = 0;
    connections[i].role = UNASSIGNED;
  };

  /* init random numbers */
  srand(time(NULL));
  lastconnectionid = rand();
  
  /* the send buffer is located in the lib/SRBclient.c and is not
     alloc'ed until we really know we need it. */
  if (_mw_srbmessagebuffer == NULL) _mw_srbmessagebuffer = malloc(SRBMESSAGEMAXLEN);

  /* init the select() table to say that no fd shell be selected. */
  FD_ZERO(&sockettable);

  mwlog(MWLOG_DEBUG2, "tcpserverinit: completed");
  return;
};

/* begin listening on a gven port and set role of port. */
int tcpstartlisten(int port, int role)
{
  int i, s, rc, len;
  char buffer [1024];
  int blen = 1024;
  if ( (role < 1) || (role > 3) ) {
    errno = EINVAL;
    return -1;
  };
  
  if ( (port < 1) || (port > 0xffff) ) {
    errno = EINVAL;
    return -1;
  };

  s = socket(AF_INET, SOCK_STREAM, 0);

  
  errno = 0;
  * (int *) buffer = 1;
  /* set reuse adress for faster restarts. */
  rc = setsockopt(s, SOL_SOCKET, SO_REUSEADDR, buffer, blen);
  mwlog(MWLOG_DEBUG, "setsockopt SO_REUSESDDR returned %d, errno = %d", 
	rc, errno);

  /* the the role of the listen socket, we either listen for gateway
     calls, clients or both*/
  connections[s].role = role;
  connections[s].listen = TRUE;

  /* at some point we shall be able to specify which local interfaces
     we listen on but for now we use any. */
  connections[s].ip4.sin_addr.s_addr = INADDR_ANY ;
  connections[s].ip4.sin_port = htons(port);
  connections[s].ip4.sin_family = AF_INET;
  len = sizeof(struct sockaddr_in);

  rc = bind(s, &connections[s].ip4, len);
  if (rc == -1) {
    mwlog(MWLOG_ERROR, "Attempted to bind socket to port %d but failed, reason %s", 
	  port, strerror(errno));
    close (s);
    connections[s].role = -1;
    return -1;
  };
  
  FD_SET(s, &sockettable);
  maxsocket = s;
  rc = listen (s, 1);
  if (rc == -1) {
    mwlog(MWLOG_ERROR, "Attempted to start listening on port %d but failed, reason %s", 
	  port, strerror(errno));
    close (s);
    connections[s].role = -1;
    return -1;
  };

  if (role & SRB_ROLE_CLIENT) 
    mwlog(MWLOG_INFO, "Listening on port %d for clients", port);
      
  if (role & SRB_ROLE_GATEWAY) 
    mwlog(MWLOG_INFO, "Listening on port %d for gateways", port);
      
  return s;
};

/* for every time we get a new connection we call here to accept ot
   and pace it into the list of sockets to do select on.*/
static int newconnection(int listensocket)
{
  int mtu=-1, rc, fd, len;
  struct sockaddr_in sain;

  /* fom gateway.c */
  
  errno = 0;
  len = sizeof(struct sockaddr_in);

  fd = accept(listensocket, &sain, &len);
  if (fd == -1) {
    return -1;
  };

  if (tcpshutdown) {
    close (fd);
    return 0;
  };

  /* when inited, 0< <RAND_MAX, and RAND_MAX should be the INT_MAX, or
     2^31. Just need to make sure is stays positive */
  lastconnectionid++;
  if (lastconnectionid <= 0) lastconnectionid=100;
  
  connections[fd].connectionid = lastconnectionid;
  connections[fd].client = UNASSIGNED;
  connections[fd].gateway = UNASSIGNED;
  connections[fd].listen = FALSE;
  connections[fd].connected = time(NULL);
  
  /* we mark the possible roles, it is cross checked by handling of
     SRB INIT */
  connections[fd].role = connections[listensocket].role;

  if (sain.sin_family == AF_INET) {
    memcpy(&connections[fd].ip4, &sain, len);
  };
  connections[fd].messagebuffer = (char *) malloc(SRBMESSAGEMAXLEN);
  
  _mw_srbsendready(fd, mydomain);
  /* Now we get the MTU of the socket. if MTU is less than
     MWMAXMESSAGELEN, but over a size like 256 we limit messages to
     that size. to get some control over fragmetation. This is
     probably crap on TCP but very important should we do UDP. */
  len = sizeof(int);
  rc = getsockopt(fd, SOL_IP, IP_MTU, &mtu, &len);
  mwlog(MWLOG_DEBUG, "Got new connection fd = %d, MTU=%d errno = %s from %s:%d",
          fd, mtu, strerror(errno), inet_ntoa(sain.sin_addr),
	  ntohs(sain.sin_port));

  FD_SET(fd, &sockettable);
  if (maxsocket < fd) maxsocket = fd;
  return fd;
};

/* close the socket, and remove it from the list of fd to do select on. */  
void tcpcloseconnection(int fd)
{
  FD_CLR(fd, &sockettable);
  mwlog(MWLOG_DEBUG, "tcpcloseconnection on  fd=%d", fd);
  if (connections[fd].client != UNASSIGNED) 
    gwdetachclient(connections[fd].client);

  _mw_srbsendterm(fd, -1);
  close (fd);

  connections[fd].client = UNASSIGNED;
  connections[fd].gateway = UNASSIGNED;
  connections[fd].role = UNASSIGNED;
  free(connections[fd].messagebuffer);
  connections[fd].messagebuffer == NULL;
  
  if (maxsocket < fd) return;

  /* if this is the highest filedescription used, find the next
     highest and assign it to maxsocket. */
  if (maxsocket == fd)
    while (--fd > 0) {
      if (FD_ISSET(fd, &sockettable)) {
	maxsocket = fd;
	return ;
      };
    };
  return;
};

/* called on shutdown, the shutdown flag will stop all new
   connections, but not close the listen socket(s) since it will cause
   a spin loop on the select(), and on all open (none listen) sockets
   send a "TERM!" and call tcpcloseconnection(). */
void tcpcloseall(void)
{
  int i;

  tcpshutdown = 1; 
  mwlog(MWLOG_DEBUG, "closing all connections, maxsocket is %d", maxsocket);

  for (i = 0; i <= maxsocket; i++) {
    if (connections[i].role != UNASSIGNED) 
      if (connections[i].listen) {
	mwlog(MWLOG_INFO, "closing listening %d", i);
	connections[i].client = UNASSIGNED;
	connections[i].gateway = UNASSIGNED;
	connections[i].role = UNASSIGNED;
	close (i);
	FD_CLR(i, &sockettable);
      } else { // Not listen socket
	mwlog(MWLOG_INFO, "closing connection %d to %s:%d", i,  
	      inet_ntoa(connections[i].ip4.sin_addr),
	      ntohs(connections[i].ip4.sin_port));
	tcpcloseconnection(i);
      };
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

  n = read(fd, connections[fd].messagebuffer+connections[fd].leftover, 
	       SRBMESSAGEMAXLEN-connections[fd].leftover);
  /* read return 0 on end of file, and -1 on error. */
  if ((n == -1) || (n == 0)) {
    tcpcloseconnection(fd);
    return ;
  };

  end = connections[fd].leftover + n;
  mwlog(MWLOG_DEBUG, "readmessage(fd=%d) read %d bytes + %d leftover buffer now:%.*s", 
	fd, n, connections[fd].leftover, end, connections[fd].messagebuffer);

  
  /* we start at leftover since there can't be a \r\n in what was
     leftover from the last time we read from the connection. */
  for (i = connections[fd].leftover ; i < end; i++) {
    
    /* if we find a message end, process it */
    if ( (connections[fd].messagebuffer[i] == '\r') && 
	 (connections[fd].messagebuffer[i+1] == '\n') ) {
      connections[fd].messagebuffer[i] = '\0';

      mwlog(MWLOG_DEBUG, "read a message from fd=%d, about to process with srbDomessage", fd);

      if (trace)
	printf(">%d (%d) %s\n", fd, i, connections[fd].messagebuffer);

      connections[fd].lastrx = time(NULL);
      srbDoMessage(fd, connections[fd].messagebuffer+start);
      mwlog(MWLOG_DEBUG, "srbDomessage completed");

      i += 2;
      start = i;
    };
  };
  
  /* nothing leftover, normal case */
  if (start == end) { 
    connections[fd].leftover = 0;
    return;
  };
  
  /* test for message overflow */
  if ( (end - start) >= SRBMESSAGEMAXLEN) {
    mwlog(MWLOG_WARNING, "readmessage(fd=%d) message buffer was exceeded, rejecting it, next message should be rejected", fd);
    connections[fd].leftover = 0;
    return;
  };  

  /* now there must be left over, move it to the beginning of the
     message buffer */
  connections[fd].leftover = end - start;
  memcpy(connections[fd].messagebuffer, connections[fd].messagebuffer + start, 
	 connections[fd].leftover);

  return;
};
		

/* just to make it look better, we could do a little bit of
   optimization here by only copying the n entries. */
static void copy_fd_set(fd_set * copy, fd_set * orig, int n)
{
  memcpy(copy, orig, sizeof(fd_set));
};

void sig_dummy(int sig)
{
  signal(sig, sig_dummy);
};


/* the main loop, here we select on all sockets and issue actions. */
void * tcpservermainloop(void * param)
{
  fd_set rfds, errfds;
  int rc, i;
  extern pid_t tcpserver_pid;

  tcpserver_pid = getpid();
  mwlog(MWLOG_INFO, "tcpserver thread starting with pid=%d", tcpserver_pid);

  signal(SIGINT, sig_dummy);
  signal(SIGPIPE, sig_dummy);

  while(! globals.shutdownflag) {
    copy_fd_set(&rfds, &sockettable, maxsocket);
    copy_fd_set(&errfds, &sockettable, maxsocket);

  mwlog(MWLOG_DEBUG, "In tcpserver mainloop, about to select()");    
#ifdef DEBUG
    for (i = 0; i <=maxsocket; i++) 
      if (FD_ISSET(i, &rfds)) printf("waiting on %d\n", i);
#endif
    rc = select(maxsocket+1, &rfds, NULL, &errfds, NULL);
    
    if (rc == -1) {
      mwlog(MWLOG_DEBUG, "select returned with failure %s", 
	    strerror(errno));
      continue;
    };

    mwlog(MWLOG_DEBUG, "There are %d sockets that need attention", rc);    
    
    for (i=0; i<maxsocket+1; i++) {
      if (FD_ISSET(i, &errfds)) {
	mwlog(MWLOG_DEBUG, "error condition of fd=%d", i);
	if (connections[i].listen == FALSE) tcpcloseconnection(i);
	else {
	  mwlog (MWLOG_ERROR, 
		 "a listen socket (fd=%d) %s:%d has dissapeared, really can't happen.",
		 i, inet_ntoa(connections[i].ip4.sin_addr), 
		 ntohs(connections[i].ip4.sin_port));
	  * (int *) param = -1;
	  return param;
	};
	FD_CLR(i, &errfds);
      };
      if (FD_ISSET(i, &rfds)) {
	mwlog(MWLOG_DEBUG, "read condition of fd=%d", i);
	if (connections[i].listen == FALSE) readmessage(i);
	else newconnection(i);
	FD_CLR(i, &rfds);
      };
    };
  }
 
  * (int *) param = 0;
  return param;
};
  
