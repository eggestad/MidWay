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

static char * RCSId = "$Id$";
static char * RCSName = "$Name$"; /* CVS TAG */

/*
 * $Log$
 * Revision 1.2  2001/09/15 23:49:38  eggestad
 * Updates for the broker daemon
 * better modulatization of the code
 *
 * Revision 1.1  2000/07/20 18:49:59  eggestad
 * The SRB daemon
 *
 */

#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include <MidWay.h>
#include <SRBprotocol.h>

#include "connections.h"

static Connection connections[FD_SETSIZE];

static fd_set sockettable;

static int maxsocket = -1;


/* we have a connection id that is unique, this is to prevent
   accidential sending of messages that was for the peer that had the
   fd, disconnected, and a new peer now has the fd. This is particual
   important on attaches, in calls, both fd, and client id must match
   also. */
static int lastconnectionid ;


void conn_init(void)
{
  
  int i;

  /* clean the connections table */
  for (i = 0; i < FD_SETSIZE; i++){
    connections[i].client = UNASSIGNED;
    connections[i].gateway = UNASSIGNED;
    connections[i].messagebuffer = NULL;
    connections[i].leftover = 0;
    connections[i].listen = 0;
    connections[i].broker = 0;    
    connections[i].connectionid = UNASSIGNED;    
    connections[i].fd = UNASSIGNED;    
    connections[i].role = UNASSIGNED;
  };
 /* init random numbers */
  srand(time(NULL));
  lastconnectionid = rand();
  
  mwlog(MWLOG_DEBUG, "size of conn table is %d bytes", FD_SETSIZE * sizeof(Connection));
  /* init the select() table to say that no fd shell be selected. */
  FD_ZERO(&sockettable);

  return;
};

/* srb*info are used to set info in SRB INIT and to get info later
   on requests that only gateways may request. */
void conn_setinfo(int fd, int * id, int * role, int * rejects, int * reverse)
{
  
  if (id != NULL) {
    mwlog(MWLOG_DEBUG2, "conn_setinfo fd=%d id=%#x", fd, *id);
    if ((*id & MWCLIENTMASK) != 0)
      connections[fd].client = (*id & MWINDEXMASK) | MWCLIENTMASK;
    if ((*id & MWGATEWAYMASK) != 0)
      connections[fd].gateway = (*id & MWINDEXMASK) | MWGATEWAYMASK;
  };

  if (role != NULL) {
    mwlog(MWLOG_DEBUG2, "conn_setinfo fd=%d role=%d", fd, *role);
    connections[fd].role = *role;
  }
  if (rejects != NULL) {
    mwlog(MWLOG_DEBUG2, "conn_setinfo fd=%d rejects=%d", fd, *rejects);
    connections[fd].rejects = *rejects;
  }
  if (reverse != NULL) {
    mwlog(MWLOG_DEBUG2, "conn_setinfo fd=%d reverse=%d", fd, *reverse);
    connections[fd].reverse = *reverse;
  }
};

int conn_getinfo(int fd, int * id, int * role, int * rejects, int * reverse)
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

Connection * conn_getentry(int fd)
{
  return &connections[fd];
};

Connection * conn_getbroker()
{
  int i, fd = -1;
  for (i = 0; i <= maxsocket; i++) {
    if (connections[i].broker) return &connections[i];
  }
  return NULL;
};

Connection * conn_getfirstlisten()
{
  int i, fd = -1;
  for (i = 0; i <= maxsocket; i++) {
    if (connections[i].listen) return &connections[i];
  }
  return NULL;
};

Connection * conn_getfirstclient()
{
  int i, fd = -1;
  for (i = 0; i <= maxsocket; i++) {
    if ( !((connections[i].listen) || 
	   (connections[i].broker) ))
      return &connections[i];
  }
  return NULL;
};

char * conn_getpeername (int fd)
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

char * conn_getclientpeername (CLIENTID cid)
{
  static char name[100];
  char * ipadr, * ipname;
  struct hostent *hent ;
  int i, fd = -1;
  
  cid |= MWCLIENTMASK;

  for (i = 0; i <= maxsocket; i++) {
    mwlog(MWLOG_DEBUG2, "conn_getclientpeername: foreach fd=%d cid=%#x ?= %#x",
	  i, cid, connections[i].client);
    if (connections[i].client == cid) {
      fd = i;
      break;
    }
  };

  mwlog(MWLOG_DEBUG2, "conn_getclientpeername: cid=%#x fd = %d",cid, fd);
  if (fd == -1) return NULL;
  mwlog(MWLOG_DEBUG2, "conn_getclientpeername: cid=%#x fd = %d role: %d ?= %d",
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
  
Connection * conn_add(int fd, int role, int type)
{
  
  /* when inited, 0< <RAND_MAX, and RAND_MAX should be the INT_MAX, or
     2^31. Just need to make sure is stays positive */
  lastconnectionid++;
  if (lastconnectionid <= 0) lastconnectionid=100;
  
  connections[fd].connectionid = lastconnectionid;
  connections[fd].fd = fd;
  connections[fd].client = UNASSIGNED;
  connections[fd].gateway = UNASSIGNED;
  connections[fd].listen = type && CONN_LISTEN;
  connections[fd].broker = type && CONN_BROKER;
  connections[fd].connected = time(NULL);
  
  
  /* we mark the possible roles, it is cross checked by handling of
     SRB INIT */
  connections[fd].role = role;


  FD_SET(fd, &sockettable);
  if (maxsocket < fd) maxsocket = fd;
  
  return & connections[fd];
};

void conn_del(int fd)
{
  
  FD_CLR(fd, &sockettable);
  connections[fd].fd = UNASSIGNED;
  connections[fd].client = UNASSIGNED;
  connections[fd].gateway = UNASSIGNED;
  connections[fd].role = UNASSIGNED;
  if (connections[fd].messagebuffer == NULL) {
    free(connections[fd].messagebuffer);
    connections[fd].messagebuffer == NULL;
  };

  close (fd);

  if (maxsocket <= fd) return;

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
  

/* just to make it look better, we could do a little bit of
   optimization here by only copying the n entries. */
static void copy_fd_set(fd_set * copy, fd_set * orig, int n)
{
  memcpy(copy, orig, sizeof(fd_set));
};

int do_select(int * cause, int deadline)
{
  static fd_set rfds, errfds;
  static int lastret = 0;
  static int n = 0;
  static struct timeval tv, * tvp;
  int  i, fd;
  int now;

  if (deadline >= 0) {
    now = time(NULL);
    
    /* if already expired */
    if (deadline <= now) {
      errno = ETIME;
      return -1;;
    };

    tv.tv_sec = deadline - now;
    tv.tv_usec = 0;
    tvp = &tv;
  } else {
    tvp = NULL;
  };

  /* we only call select if we've returned all the fd's returned by
     the previous select() call */
  if (n <= 0) { 
    copy_fd_set(&rfds, &sockettable, maxsocket); 
    copy_fd_set(&errfds, &sockettable, maxsocket);
  
    mwlog(MWLOG_DEBUG, "In do_select, about to select() timeout = %d", 
	  deadline==-1?deadline:tv.tv_sec);    

#ifdef DEBUG
    for (i = 0; i <=maxsocket; i++) 
      if (FD_ISSET(i, &rfds)) printf("waiting on %d\n", i);
#endif
 
    n = select(maxsocket+1, &rfds, NULL, &errfds, tvp);
    
    if (n == -1) {
      mwlog(MWLOG_DEBUG, "select returned with failure %s", 
	    strerror(errno));
      return n;
    };
  };

  /* if timeout */
  if (n == 0) {
    errno = ETIME;
    return -1;
  };

  mwlog(MWLOG_DEBUG, "There are %d sockets that need attention", n);    
    
  /* we do a round robin and do a foreach on the connections from last
     to last-1 */
  for (i=0; i<maxsocket+1; i++) {
    fd = (lastret + i) % (maxsocket+1);
    mwlog(MWLOG_DEBUG, "checking fd %d", fd);

    if (FD_ISSET(fd, &errfds)) {
      mwlog(MWLOG_DEBUG, "fd %d has an error condition", fd);
      if (cause) *cause = COND_ERROR;
      FD_CLR(fd, &errfds);
      if (FD_ISSET(fd, &rfds)) 
	mwlog(MWLOG_DEBUG, "fd %d has an error and read condition (can't happen)", fd);
      FD_CLR(fd, &rfds); // can this ever happen???
      lastret = fd;
      n--;
      return fd;
    };

    if (FD_ISSET(fd, &rfds)) {
      mwlog(MWLOG_DEBUG, "fd %d has a read condition", fd);
      if (cause) *cause = COND_READ;
      FD_CLR(fd, &rfds);
      lastret = fd;
      n--;
      return fd;
    };
  };

  //* we should never get here!
  
  mwlog(MWLOG_ERROR, "we have %d left after a select unaccounted for", n);
  sleep(10);
  n = 0;
  return 0;
};
