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
 * Revision 1.16  2003/01/07 08:27:48  eggestad
 * * Major fixes to get three mwgwd working correctly with one service
 * * and other general fixed for suff found on the way
 *
 * Revision 1.15  2002/11/19 12:43:55  eggestad
 * added attribute printf to mwlog, and fixed all wrong args to mwlog and *printf
 *
 * Revision 1.14  2002/11/18 00:19:36  eggestad
 * - added setting of address string for Multicast pseudo connect
 * - peeraddress fix
 *
 * Revision 1.13  2002/10/22 21:58:20  eggestad
 * Performace fix, the connection peer address, is now set when establised, we did a getnamebyaddr() which does a DNS lookup several times when processing a single message in the gateway (Can't believe I actually did that...)
 *
 * Revision 1.12  2002/10/20 18:17:44  eggestad
 * fixup of debug messages
 *
 * Revision 1.11  2002/10/17 22:16:54  eggestad
 * - fix for gateways
 * - downgraded some debug to debug2
 *
 * Revision 1.10  2002/10/09 12:30:30  eggestad
 * Replaced all unions for sockaddr_* with a new type SockAddress
 *
 * Revision 1.9  2002/10/03 21:15:34  eggestad
 * - conn_select now return -errno on error, not -1 with errno set.
 *
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
#include <sys/socket.h>
#include <netinet/tcp.h>

#include <MidWay.h>
#include <multicast.h>
#include <SRBprotocol.h>
#include <ipctables.h>
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

#error "poll not implemented!"

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
  if ( (fd < 0) || (fd > maxsocket) ) {
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
  if ( (fd < 0) || (fd > maxsocket) ) {
    Error("poll off fd %d", fd);
    return;
  };

  unpoll_write(fd);
  FD_CLR(fd, &sockettable);

  DEBUG2("maxsocket = %d fd = %d", maxsocket, fd);
  if (maxsocket > fd) return;

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
    if (CLTID(connections[fd].cid) != UNASSIGNED) 
      *id = connections[fd].cid;
    else if (GWID(connections[fd].gwid) != UNASSIGNED) 
      *id = connections[fd].gwid;
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
    DEBUG2("testing connections[%d].gwid = %#x != gwid %#x", i, connections[i].gwid, gwid);
    if (connections[i].gwid == gwid) {
      DEBUG2("returnbing &connections[%d] = %p",i,  &connections[i]);
      return &connections[i];
    };
  }
  return NULL;
};

static Connection * init_mcast(void) 
{
  int s;
  Connection * conn;

  s = socket(PF_INET, SOCK_DGRAM, 0);
  if (s == -1) {
    Fatal("Out of sockets?, reason %s aborting", strerror(errno) );
  };
  _mw_setmcastaddr();
  DEBUG("created psuedo multicast connection on fd=%d", s);
  conn =  conn_add(s, UNASSIGNED, CONN_TYPE_MCAST);
  strcpy(conn->peeraddr_string, "Multicast");
  return conn;
};


Connection * conn_getmcast(void)
{  
  Connection * connmcast = NULL;
  int i;
  for (i = 0; i < connectiontablesize; i++) {
    if (connections[i].type == CONN_TYPE_MCAST) {
      connmcast = &connections[i];
      DEBUG2("found mcast connection od fd=%d %s", connmcast->fd, connmcast->peeraddr_string);
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
    if (connections[i].type == CONN_TYPE_CLIENT)
      return &connections[i];
  }
  return NULL;
};

Connection * conn_getfirstgateway(void)
{
  int i;
  for (i = 0; i <= maxsocket; i++) {
    DEBUG2(" looking at %d gwid = %#x  gateway=%d", 
	  i, connections[i].gwid, connections[i].type == CONN_TYPE_GATEWAY);
    if (connections[i].fd == -1) continue;
    if (connections[i].gwid != UNASSIGNED)
      return &connections[i];

    if (connections[i].type == CONN_TYPE_GATEWAY)
      return &connections[i];

  }
  return NULL;
};

void conn_setpeername (Connection * conn)
{
  char ipadr[128], * ipname;
  struct hostent *hent ;
  char * role = "unknown";
  int len, family, rc, id, port;
  cliententry * cltent = NULL;
  gatewayentry * gwent = NULL;
  char * ipcaddrstr = NULL;

  if (conn == NULL) return;

  if (conn->cid != UNASSIGNED) {
    role = "client";
    id = CLTID2IDX(conn->cid);
    cltent = _mw_getcliententry(conn->cid);
    if (cltent!= NULL)
      ipcaddrstr = cltent->addr_string;
  } else if (conn->gwid != UNASSIGNED) {
    role = "gateway";
    id = GWID2IDX(conn->gwid);
    gwent = _mw_getgatewayentry(conn->gwid);
    if (gwent!= NULL)
      ipcaddrstr = gwent->addr_string;
  }; 

  len = sizeof(conn->peeraddr);
  rc = getpeername(conn->fd, &conn->peeraddr.sa, &len);
  if (rc == -1) {
    sprintf (conn->peeraddr_string, "%s id %d error: getpeername returned %s", role, id, strerror(errno));
    return;
  };

  family = conn->peeraddr.sa.sa_family;
  switch(family) {

  case AF_INET:
    inet_ntop(family, &conn->peeraddr.sin4.sin_addr, ipadr, 128);
    hent = gethostbyaddr(&conn->peeraddr.sin4.sin_addr, sizeof(struct in_addr), family);
    if (hent == NULL) {
      Error("gethostbyaddr failed reason %s", hstrerror(h_errno));
      ipname = ipadr;
    } else {
      ipname = hent->h_name;
    };

    port = ntohs(conn->peeraddr.sin4.sin_port);
    snprintf(conn->peeraddr_string, 128, "%s id %d INET %s (%s) port %d", 
	     role, id,
	     ipname, ipadr,
	     port);
    if (ipcaddrstr)
      snprintf(ipcaddrstr, MWMAXNAMELEN, "INET:%s:%d", ipname, port);
    break;

  case AF_INET6:
    sprintf(conn->peeraddr_string, "%s id %d: INET6 not yet supported",
	    role, id);
    if (ipcaddrstr)
      snprintf(ipcaddrstr, MWMAXNAMELEN, "INET6:");;
    break;

  default:
    sprintf(conn->peeraddr_string, "%s id %d Connection has Unknown address family %d", 
	    role, id, family);
    if (ipcaddrstr)
      snprintf(ipcaddrstr, MWMAXNAMELEN, "??Family %d:", family);
    break;
  };

  Info ("fd=%d peeraddr_string = %s, ipcaddr = %s", conn->fd, conn->peeraddr_string, ipcaddrstr);

  return;
};

void conn_getclientpeername (CLIENTID cid, char * name, int namelen)
{
  int i, fd = -1;
  
  cid = CLTID2IDX(cid);

  for (i = 0; i <= maxsocket; i++) {
    DEBUG2("conn_getclientpeername: foreach fd=%d cid=%d ?= %#x",
	  i, cid, CLTID2IDX(connections[i].cid));
    if (CLTID2IDX(connections[i].cid) == cid) {
      fd = i;
      break;
    }
  };

  name[0] = '\0';
  if (namelen > 127) {
    name[127] = '\0';
    namelen = 127;
  };

  DEBUG2("conn_getclientpeername: cid=%d fd = %d", cid, fd);
  if (fd == -1) return;

  DEBUG2("conn_getclientpeername: cid=%d fd = %d role: %d ?= %d",
	cid, fd, connections[fd].role, SRB_ROLE_CLIENT);
  if (connections[fd].role != SRB_ROLE_CLIENT) return;

  strncpy(name, connections[fd].peeraddr_string, namelen);
  return;
};

/* the contructor for the connection table */
static void conn_clear(Connection * conn)
{
  if (conn == NULL) return;

  if (conn->messagebuffer != NULL) {
    free(conn->messagebuffer);
    conn->messagebuffer = NULL;
  };

  memset (conn, '\0', sizeof(Connection));
  conn->cid = UNASSIGNED;
  conn->gwid = UNASSIGNED;
  conn->cost = 100;
  conn->connectionid = UNASSIGNED;    
  conn->fd = UNASSIGNED;    
  conn->role = UNASSIGNED;
};

/* after a socket or SSL conbection (still a file descriptor) is
   created we create a connection entry to match. role is the SRB
   role, eithe GATEWAY ot CLIENT and may be -1, type is teh CONNECTION
   type either LISTEN, MCAST, GATEWAY , BROKER or CLIENT.  (See
   connecton.h) */
Connection * conn_add(int fd, int role, int type)
{
   int val = 1, len, rc;
 
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
    // TODO: shouldn't we:     return NULL;
    return & connections[fd];
  };

  connections[fd].connectionid = lastconnectionid;
  connections[fd].fd = fd;
  connections[fd].connected = time(NULL);
  connections[fd].messagebuffer = (char *) malloc(SRBMESSAGEMAXLEN+1);
  connections[fd].cost = 100;

  poll_on(fd);
  conn_set(& connections[fd], role, type);

  len = sizeof(val);
  rc = setsockopt(fd, SOL_TCP, TCP_NODELAY, &val, len);
  DEBUG("Nodelay is set to 1 rc %d errno %d", rc, errno);  
 
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
  if ( (fd < 0) || (fd > maxsocket) ) {
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
  struct tm * ts;

  if (maxsocket <= 0) {
    output = realloc(output, 4096);
  } else {
    output = realloc(output, maxsocket * 4096);
  };

  l = sprintf (output, "Printing connection table\n"
	       "  fd role version  mtu "
	       " cid gwid"
	       " state type"
	       " connected     lasttx        lastrx       "
	       " peeraddr   peerinstance GWID");

  for (i = 0; i<maxsocket+1; i++) {
    if (connections[i].fd != -1) {

      l += sprintf (output+l, "\n  %2d %4d %7d %4d",
		    connections[i].fd, connections[i].role, 
		    connections[i].version, 
		    connections[i].mtu);
      
      l += sprintf (output+l, " %4d %4d", 
		    connections[i].cid != UNASSIGNED ? connections[i].cid & MWINDEXMASK : -1,
		    connections[i].gwid != UNASSIGNED ? connections[i].gwid  & MWINDEXMASK : -1);
      
      l += sprintf (output+l, " %5d %4c",
		    connections[i].state, 
		    connections[i].type);

      ts = localtime(&connections[i].connected);
      l+= strftime(output+l, 100, " %Y%m %H%M%S", ts);
      ts = localtime(&connections[i].lasttx);
      l+= strftime(output+l, 100, " %Y%m %H%M%S", ts);
      ts = localtime(&connections[i].lastrx);
      l+= strftime(output+l, 100, " %Y%m %H%M%S", ts);
	
      l += sprintf (output+l, " %s", 
		    connections[i].peeraddr_string);

      if (connections[i].peerinfo == NULL) continue;

      l += sprintf (output+l, " %s %d", 
		    ((struct gwpeerinfo *) connections[i].peerinfo)->instance, 
		    GWID2IDX( ((struct gwpeerinfo *) connections[i].peerinfo)->gwid));
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
	  inet_ntop(AF_INET, &conn->peeraddr.sin4.sin_addr, buffer, 64),
	  ntohs(conn->peeraddr.sin4.sin_port)); 
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

  DEBUG2("Beginning conn_select() select version");    

  if (deadline >= 0) {
    now = time(NULL);
    
    /* if already expired */
    if (deadline <= now) {
      DEBUG("deadline expired (%d <= %d)", deadline, now);    
      return -ETIME;;
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
  
    DEBUG2("about to select() timeout = %d", 
	  deadline==-1?deadline:tv.tv_sec);    

#ifdef DEBUGGING
    for (i = 0; i <=maxsocket; i++) 
      if (FD_ISSET(i, &rfds)) DEBUG2("waiting on %d", i);
#endif
    
    TIMEPEGNOTE("End of TCP");
    timepeg_log();
    DEBUG("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^");    
    n = select(maxsocket+1, &rfds, NULL, &errfds, tvp);
    DEBUG("vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv");    
    TIMEPEGNOTE("start TCP");
    TIMEPEGNOTE("start");

    if (n == -1) {
      DEBUG("select returned with failure %s", 
	    strerror(errno));
      return -errno;
    };
  };

  /* if timeout */
  if (n == 0) {
    return -ETIME;
  };

  DEBUG2("There are %d sockets that need attention", n);    
    
  /* we do a round robin and do a foreach on the connections from last
     to last-1 */
  for (i=0; i<maxsocket+1; i++) {
    fd = (lastret + i) % (maxsocket+1);
    DEBUG2("checking fd %d", fd);

    *cause = 0;

    if (FD_ISSET(fd, &errfds)) {
      DEBUG2("fd %d has an error condition", fd);
      if (cause) *cause |= COND_ERROR;
      FD_CLR(fd, &errfds);
    };

    if (FD_ISSET(fd, &wfds)) {
      DEBUG2("fd %d has a write condition", fd);
      if (cause) *cause |= COND_WRITE;
      FD_CLR(fd, &wfds);
    };

    if (FD_ISSET(fd, &rfds)) {
      DEBUG2("fd %d has a read condition", fd);
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









