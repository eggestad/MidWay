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

/*
 * $Log$
 * Revision 1.8  2002/09/26 22:38:19  eggestad
 * we no longer listen on the standalone  port (11000) for clients as default. Default is now teh broker.
 *
 * Revision 1.7  2002/09/22 23:01:16  eggestad
 * fixup policy on *ID's. All ids has the mask bit set, and purified the consept of index (new macros) that has the mask bit cleared.
 *
 * Revision 1.6  2002/08/09 20:50:16  eggestad
 * A Major update for implemetation of events and Task API
 *
 * Revision 1.5  2002/07/07 22:45:48  eggestad
 * *** empty log message ***
 *
 * Revision 1.4  2001/10/05 14:34:19  eggestad
 * fixes or RH6.2
 *
 * Revision 1.3  2001/10/03 22:36:31  eggestad
 * bugfixes
 *
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
#include <assert.h>

#include <MidWay.h>
#include <multicast.h>
#include <SRBprotocol.h>
#include "gateway.h"
#include "connections.h"

/* we use select for now
#ifdef HAS_POLL
#  define USE_POLL
#else 
#  ifdef HAS_SELECT
#  else
#    error "We must have either poll() or select() but according to config.h we have neither!"
# endif
#endif
*/

static char * RCSId UNUSED = "$Id$";


/* hmm I'm almost puzzeled that this all works withouit a mutex on the
   connection table. But I'm pretty sure that I'm OK without it.  */

static Connection * connections = NULL;
static int connectiontablesize = 0;

/************************************************************************
 * here we have the function that turn on and off read/write polling
 * on a given filedesc. THis is in two versions depending on wither we
 * use poll() or select()
 */
static int maxsocket = -1;

#if USE_POLL

#error "poll not implemeted!"

#else

static fd_set sockettable, sockettable_write;

void poll_write(int fd)
{
  if (fd < 0) {
    Error("poll write on fd %d", fd);
    return;
  };

  if (!FD_ISSET(fd, &sockettable)) {
    Error("Internal error, attempt to do write poll without doing read poll on fd=%d", fd);
    return;
  };
  FD_SET(fd, &sockettable_write);

};

void unpoll_write(int fd)
{
  if (fd < 0) {
    Error("unpoll write on fd %d", fd);
    return;
  };

  FD_CLR(fd, &sockettable_write);
};

static void poll_on(int fd)
{
  if (fd < 0) {
    Error("poll on fd %d", fd);
    return;
  };

  if (fd >= (FD_SETSIZE-1)) {
    Error("Polling on file descriptor %d with exceeds max %d, closing connection "
	  "Either spread clients over more instances or recompile with poll() instead of select()", 
	  fd, FD_SETSIZE);
    conn_del(fd);
    return;
  } else if (fd > (FD_SETSIZE - FD_SETSIZE/10)) {    
    Warning("Polling on file descriptor %d witch is close to the max %d, "
	  "Either spread clients over more instances or recompile with poll() instead of select()", 
	  fd, FD_SETSIZE);
  } 
  
  FD_SET(fd, &sockettable);
  if (maxsocket < fd) maxsocket = fd;
};

static void poll_off(int fd)
{
  if (fd < 0) {
    Error("poll off fd %d", fd);
    return;
  };

  unpoll_write(fd);
  FD_CLR(fd, &sockettable);
  
  if (maxsocket < fd) return;

  /* if this is the highest filedescription used, find the next
     highest and assign it to maxsocket. */
  while (--fd > 0) {
    if (FD_ISSET(fd, &sockettable)) {
      maxsocket = fd;
      return ;
    };
  };
  /* in the off case that all socket are now not polled. */
  maxsocket = -1;
  return;
};
#endif

/************************************************************************/


/* we have a connection id that is unique, this is to prevent
   accidential sending of messages that was for the peer that had the
   fd, disconnected, and a new peer now has the fd. This is particual
   important on attaches, in calls, both fd, and client id must match
   also. */
static int lastconnectionid ;

void conn_init(void)
{

 /* init random numbers */
  srand(time(NULL));
  lastconnectionid = rand();
  
#ifdef USE_POLL
#else 
  FD_ZERO(&sockettable);
  FD_ZERO(&sockettable_write);
#endif

  return;
};

/* srb*info are used to set info in SRB INIT and to get info later
   on requests that only gateways may request. */
void conn_setinfo(int fd, int * id, int * role, int * rejects, int * reverse)
{
  
  if (id != NULL) {
    DEBUG2("conn_setinfo fd=%d id=%#x", fd, *id);
    if ((*id & MWCLIENTMASK) != 0)
      connections[fd].cid = (*id & MWINDEXMASK) | MWCLIENTMASK;
    if ((*id & MWGATEWAYMASK) != 0)
      connections[fd].gwid = (*id & MWINDEXMASK) | MWGATEWAYMASK;
  };

  if (role != NULL) {
    DEBUG2("conn_setinfo fd=%d role=%d", fd, *role);
    connections[fd].role = *role;
  }
  if (rejects != NULL) {
    DEBUG2("conn_setinfo fd=%d rejects=%d", fd, *rejects);
    connections[fd].rejects = *rejects;
  }
  if (reverse != NULL) {
    DEBUG2("conn_setinfo fd=%d reverse=%d", fd, *reverse);
    connections[fd].reverse = *reverse;
  }
};

int conn_getinfo(int fd, int * id, int * role, int * rejects, int * reverse)
{
  if (id != NULL) {
    if (connections[fd].cid != UNASSIGNED) 
      *id = connections[fd].cid | MWCLIENTMASK;
    else if (connections[fd].gwid != UNASSIGNED) 
      *id = connections[fd].gwid | MWGATEWAYMASK;
    else {
      *id = UNASSIGNED;
    };
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

Connection * conn_getbroker(void)
{
  int i;
  for (i = 0; i <= maxsocket; i++) {
    if (connections[i].fd == -1) continue;
    if (connections[i].type == CONN_TYPE_BROKER) return &connections[i];
  }
  return NULL;
};

Connection * conn_getgateway(GATEWAYID gwid)
{
  int i;
  for (i = 0; i <= maxsocket; i++) {
    if (connections[i].gwid == gwid) return &connections[i];
  }
  return NULL;
};

static Connection * init_mcast(void) 
{
  int s;
  
  s = socket(PF_INET, SOCK_DGRAM, 0);
  if (s == -1) {
    Fatal("Out of sockets?, reason %s aborting", strerror(errno) );
  };
  _mw_setmcastaddr();
  DEBUG("created puedo multicast connection on fd=%d", s);
  return conn_add(s, UNASSIGNED, CONN_TYPE_MCAST);
};


Connection * conn_getmcast(void)
{  
  Connection * connmcast = NULL;
  int i;
  for (i = 0; i < connectiontablesize; i++) {
    if (connections[i].type == CONN_TYPE_MCAST) {
      connmcast = &connections[i];
      break;
    };
  };
  /* we open the "connection" if it doesn't exist */
  if (connmcast == NULL) return init_mcast();
  return connmcast;
};

Connection * conn_getfirstlisten(void)
{
  int i;
  for (i = 0; i <= maxsocket; i++) {
    if (connections[i].fd == -1) continue;
    if (connections[i].type == CONN_TYPE_BROKER) return &connections[i];
    if (connections[i].type == CONN_TYPE_LISTEN) return &connections[i];
  }
  return NULL;
};

Connection * conn_getfirstclient(void)
{
  int i;
  for (i = 0; i <= maxsocket; i++) {
    if (connections[i].fd == -1) continue;
    if (connections[i].type & CONN_TYPE_CLIENT)
      return &connections[i];
  }
  return NULL;
};

Connection * conn_getfirstgateway(void)
{
  int i;
  for (i = 0; i <= maxsocket; i++) {
    DEBUG(" looking at %d gwid = %#x  gateway=%d", 
	  i, connections[i].gwid, connections[i].type & CONN_TYPE_GATEWAY);
    if (connections[i].fd == -1) continue;
    if (connections[i].gwid != UNASSIGNED)
      return &connections[i];

    if (connections[i].type & CONN_TYPE_GATEWAY)
      return &connections[i];

  }
  return NULL;
};

void conn_getpeername (Connection * conn, char * name, int namelen)
{
  char * ipadr, * ipname;
  struct hostent *hent ;
  int len;
  struct sockaddr_in addr;
  
  if (name == NULL) {
    Error("internal error, conn_getpeername called with NULL buffer");
    return;
  };

  if (namelen < 100) {
    Error("internal error, conn_getpeername called with to short buffer %d < 100", namelen);
    name[0] = '\0';
    return;
  };

  len = sizeof(addr);
  getpeername(conn->fd, (struct sockaddr *) &addr, &len);
  ipadr = inet_ntoa(addr.sin_addr);
  hent = gethostbyaddr(ipadr, strlen(ipadr), AF_INET);
  if (hent == NULL)
    ipname = ipadr;
  else 
    ipname = hent->h_name;

  if (conn->cid != UNASSIGNED) {
    len = sprintf(name, "client %d at ", CLTID2IDX(conn->cid));
  } else if (conn->gwid != UNASSIGNED) {
    len = sprintf(name, "gateway %d at ", GWID2IDX(conn->gwid));
  };

  sprintf(name+len, "%s (%s) port %d", ipname, ipadr,
	  ntohs(addr.sin_port));
  return;
};

void conn_getclientpeername (CLIENTID cid, char * name, int namelen)
{
  int i, fd = -1;
  
  cid |= MWCLIENTMASK;

  for (i = 0; i <= maxsocket; i++) {
    DEBUG2("conn_getclientpeername: foreach fd=%d cid=%#x ?= %#x",
	  i, cid, connections[i].cid);
    if (connections[i].cid == cid) {
      fd = i;
      break;
    }
  };

  DEBUG2("conn_getclientpeername: cid=%#x fd = %d",cid, fd);
  if (fd == -1) return;
  DEBUG2("conn_getclientpeername: cid=%#x fd = %d role: %d ?= %d",
	cid, fd, connections[fd].role, SRB_ROLE_CLIENT);
  if (connections[fd].role != SRB_ROLE_CLIENT) return;

  conn_getpeername(&connections[fd], name, namelen);
  return;
};

/* the contructor for the connection table */
static void conn_clear(Connection * conn)
{
  if (conn == NULL) return;

  conn->cid = UNASSIGNED;
  conn->gwid = UNASSIGNED;
  
  if (conn->messagebuffer != NULL) {
    free(conn->messagebuffer);
    conn->messagebuffer = NULL;
  };
  conn->leftover = 0;
  conn->type = 0;
  conn->cost = 100;
  conn->connectionid = UNASSIGNED;    
  conn->fd = UNASSIGNED;    
  conn->role = UNASSIGNED;
  conn->peerinfo = NULL;
  conn->connected = 0;
  conn->lasttx = 0;
  conn->lastrx = 0;
  conn->peerinfo = NULL;
};

/* after a socket or SSL conbection (still a file descriptor) is
   created we create a connection entry to match. role is the SRB
   role, eithe GATEWAY ot CLIENT and may be -1, type is teh CONNECTION
   type either LISTEN, MCAST, GATEWAY , BROKER or CLIENT.  (See
   connecton.h) */
Connection * conn_add(int fd, int role, int type)
{
  /* when inited, 0< <RAND_MAX, and RAND_MAX should be the INT_MAX, or
     2^31. Just need to make sure is stays positive */
  if (fd < 0) {
    Error("conn_add on fd %d", fd);
    return;
  };


  lastconnectionid++;
  if (lastconnectionid <= 0) lastconnectionid=100;

  DEBUG("conn add on %d", fd);


  if (connectiontablesize <= fd) {
    int i, l;

    l = fd+1;
    connections = realloc(connections, sizeof(Connection) * l);
    DEBUG("extending the connection table size from %d to %d", connectiontablesize, l);
  
    for (i = connectiontablesize; i < l; i++) {
      DEBUG("init connection table entry %d", i);
      conn_clear(&connections[i]);
    };
    connectiontablesize = l;
  };

  if (connections[fd].fd != UNASSIGNED) {
    Error("Internal error: attempted to do conn_add on fd=%d while connection already assigned cid=%#x gwid%#x", 
	  fd, connections[fd].cid, connections[fd].gwid);
  } else {
    connections[fd].connectionid = lastconnectionid;
    connections[fd].fd = fd;
    connections[fd].connected = time(NULL);
    connections[fd].messagebuffer = (char *) malloc(SRBMESSAGEMAXLEN+1);
  };
  
  poll_on(fd);
  conn_set(& connections[fd], role, type);
  
  return & connections[fd];
};

void conn_set(Connection * conn, int role, int type)
{
  /* we mark the possible roles, it is cross checked by handling of
     SRB INIT */
  DEBUG("Connection on fd=%d has role=%#x type=%#x", conn->fd, role, type);
  conn->role = role;
  conn->type = type;
};

void conn_del(int fd)
{
  if (fd < 0) {
    Error("conn_del on fd %d", fd);
    return;
  };

  /* if it's a gateway we call gw_closegateway() who call us later*/
  if (connections[fd].gwid  != UNASSIGNED) {
    DEBUG("a close on a gateway, calling gw_closegateway() fd = %d", fd);
    gw_closegateway(connections[fd].gwid);
    return;
  };

  poll_off(fd);
  conn_clear(&connections[fd]);
  close (fd);

  return;
};

/* just for debugging purpose, print out all relevant data for all connections. */
char * conn_print(void)
{
  static char * output = NULL;
  int i, l;

  if (maxsocket <= 0) {
    output = realloc(output, 4096);
  } else {
    output = realloc(output, maxsocket * 4096);
  };

  l = sprintf (output, "Printing connection table\n"
	       "fd role version mtu cid gwid state type connected lasttx lastrx peerinfo");
  for (i = 0; i<maxsocket+1; i++) {
    if (connections[i].fd != -1) {
      l += sprintf (output+l, "\n%2d %4d %7d %3d %3d %4d %5d %4d %9d %6d %6d %p", 
		    connections[i].fd, connections[i].role, connections[i].version, 
		    connections[i].mtu, 
		    connections[i].cid != UNASSIGNED ? connections[i].cid & MWINDEXMASK : -1,
		    connections[i].gwid != UNASSIGNED ? connections[i].gwid  & MWINDEXMASK : -1, 
		    connections[i].state, connections[i].type, 
		    connections[i].connected, connections[i].lasttx, connections[i].lastrx, 
		    connections[i].peerinfo);
    };
  };
  return output;
};


/* just to make it look better, we could do a little bit of
   optimization here by only copying the n entries. */
static void copy_fd_set(fd_set * copy, fd_set * orig, int n)
{
  memcpy(copy, orig, sizeof(fd_set));
};


/* at some poing we're going to do OpenSSL. Then conn_read, and
   conn_write will hide the SSL, and the rest of MidWay can treat it
   as a stream connection. */
int conn_read(Connection * conn)
{
  int rc, l;
  char buffer[64];
  /* if it is the mcast socket, this is a UDP socket and we use
     recvfrom. The conn->peeraddr wil lthen always hold the address of
     sender for the last recv'ed message, which is OK, with UDPO we
     must either get the whole message or not at all. */
  if (conn->type == CONN_TYPE_MCAST) {
    DEBUG("UDP recv, leftover = %d, better be 0!", conn->leftover); 
    assert ( conn->leftover == 0);
    l = sizeof(conn->peeraddr);
    rc = recvfrom(conn->fd, conn->messagebuffer+conn->leftover, 
	      SRBMESSAGEMAXLEN-conn->leftover, 0, &conn->peeraddr.sa, &l);
    
    DEBUG("UDP: read %d bytes from %s:%d", rc, 
	  inet_ntop(AF_INET, &conn->peeraddr.ip4.sin_addr, buffer, 64),
	  ntohs(conn->peeraddr.ip4.sin_port)); 
  } else {    
    rc = read(conn->fd, conn->messagebuffer+conn->leftover, 
	      SRBMESSAGEMAXLEN-conn->leftover);
  };
  return rc;
};

/* 
  int conn_write(Connection * conn, char * buffer, int len);
*/

int conn_select(int * cause, time_t deadline)
{
  static fd_set rfds, wfds, errfds;
  static int lastret = 0;
  static int n = 0;
  static struct timeval tv, * tvp;
  int  i, fd;
  int now;

  DEBUG("^^^^^^^^^^  Beginning conn_select() select version");    

  if (deadline >= 0) {
    now = time(NULL);
    
    /* if already expired */
    if (deadline <= now) {
      DEBUG("deadline expired (%d <= %d)", deadline, now);    
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
  
    DEBUG("about to select() timeout = %d", 
	  deadline==-1?deadline:tv.tv_sec);    

#ifdef DEBUG
    for (i = 0; i <=maxsocket; i++) 
      if (FD_ISSET(i, &rfds)) printf("waiting on %d\n", i);
#endif
    
    errno = 0;
    n = select(maxsocket+1, &rfds, NULL, &errfds, tvp);
    
    if (n == -1) {
      DEBUG("select returned with failure %s", 
	    strerror(errno));
      return n;
    };
  };

  /* if timeout */
  if (n == 0) {
    errno = ETIME;
    return -1;
  };

  DEBUG("vvvvvvvvvv There are %d sockets that need attention", n);    
    
  /* we do a round robin and do a foreach on the connections from last
     to last-1 */
  for (i=0; i<maxsocket+1; i++) {
    fd = (lastret + i) % (maxsocket+1);
    DEBUG("checking fd %d", fd);

    *cause = 0;

    if (FD_ISSET(fd, &errfds)) {
      DEBUG("fd %d has an error condition", fd);
      if (cause) *cause |= COND_ERROR;
      FD_CLR(fd, &errfds);
    };

    if (FD_ISSET(fd, &wfds)) {
      DEBUG("fd %d has a write condition", fd);
      if (cause) *cause |= COND_WRITE;
      FD_CLR(fd, &wfds);
    };

    if (FD_ISSET(fd, &rfds)) {
      DEBUG("fd %d has a read condition", fd);
      if (cause) *cause |= COND_READ;
      FD_CLR(fd, &rfds);
    };
    
    if (*cause != 0) {
      lastret = fd;
      n--;
      return fd;
    };
  };

  //* we should never get here!
  
  Error("we have %d left after a select unaccounted for", n);
  sleep(10);
  n = 0;
  return 0;
};









