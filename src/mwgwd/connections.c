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
 * Revision 1.21  2003/09/25 19:36:17  eggestad
 * - had a serious bug in the input handling of SRB messages in the Connection object, resulted in lost messages
 * - also improved logic in blocking/nonblocking of reading on Connection objects
 *
 * Revision 1.20  2003/08/06 23:16:19  eggestad
 * Merge of client and mwgwd recieving SRB messages functions.
 *
 * Revision 1.19  2003/07/13 22:42:04  eggestad
 * added timepegs
 *
 * Revision 1.18  2003/07/06 22:07:56  eggestad
 * took out reverse ip lookups, it caused timeout on mwgwd start if no dns server is available
 *
 * Revision 1.17  2003/03/16 23:50:24  eggestad
 * Major fixups
 *
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

/* threre was a major problem in th eoriginal implementation. Nota
mutex problem though.  There was only onle connection table that was
realloc'ed when expansion was needed. However, there are pointers to
the conn entry around, most notably in gwpeerinfo. Hence the conn
struct can't move. Also when one thread would do realloc, the other
may do dereferencing of the conn pointer that are no longer valid. 

Our solution is to to a two level array to save space, and teh to
level array may be realloced. */

#define CONNDIRSIZE 10
 
static Connection ** conn_dir = NULL;
//static Connection * connections = NULL;
static int connectiontablesize = 0;

/************************************************************************
 * here we have the function that turn on and off read/write polling
 * on a given filedesc. THis is in two versions depending on wither we
 * use poll() or select()
 ************************************************************************/

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

static inline Connection * fd2conn(int fd)
{
  int dir;
  int idx;

  dir = fd / CONNDIRSIZE;
  idx = fd % CONNDIRSIZE;
  
  //  DEBUG2("fd=%d dir = %d idx = %d", fd, dir, idx);

  Assert(conn_dir[dir] != NULL);
  return &conn_dir[dir][idx];
};

/* srb*info are used to set info in SRB INIT and to get info later
   on requests that only gateways may request. */
void conn_setinfo(int fd, int * id, int * role, int * rejects, int * reverse)
{
  Connection * conn;
  
  conn = fd2conn(fd);

  if (id != NULL) {
    DEBUG2("fd=%d id=%#x", fd, *id);
    if ((*id & MWCLIENTMASK) != 0)
      conn->cid = (*id & MWINDEXMASK) | MWCLIENTMASK;
    if ((*id & MWGATEWAYMASK) != 0)
      conn->gwid = (*id & MWINDEXMASK) | MWGATEWAYMASK;
  };

  if (role != NULL) {
    DEBUG2("fd=%d role=%d", fd, *role);
    conn->role = *role;
  }
  if (rejects != NULL) {
    DEBUG2("fd=%d rejects=%d", fd, *rejects);
    conn->rejects = *rejects;
  }
  if (reverse != NULL) {
    DEBUG2("fd=%d reverse=%d", fd, *reverse);
    conn->reverse = *reverse;
  }
};

int conn_getinfo(int fd, int * id, int * role, int * rejects, int * reverse)
{
  Connection * conn;
  
  conn = fd2conn(fd);

  if (id != NULL) {
    if (CLTID(conn->cid) != UNASSIGNED) 
      *id = conn->cid;
    else if (GWID(conn->gwid) != UNASSIGNED) 
      *id = conn->gwid;
    else {
      *id = UNASSIGNED;
    };
  };

  if (role != NULL)
    *role = conn->role;

  if (rejects != NULL)
    *rejects = conn->rejects;

  if (reverse != NULL)
    *reverse = conn->reverse;
  
  return 0;
};

Connection * conn_getentry(int fd)
{
  Assert(fd >= 0);

  Assert (fd < connectiontablesize);

  return fd2conn(fd);
};

Connection * conn_getbroker(void)
{
  int i;
  Connection * conn;
  for (i = 0; i <= maxsocket; i++) {
    conn = fd2conn(i);
    if (conn->fd == UNASSIGNED) continue;
    if (conn->type == CONN_TYPE_BROKER) return conn;
  }
  return NULL;
};

Connection * conn_getgateway(GATEWAYID gwid)
{
  int i;
  Connection * conn;
  for (i = 0; i <= maxsocket; i++) {
    conn = fd2conn(i);
    DEBUG2("testing connections[%d].gwid = %#x != gwid %#x", i, conn->gwid, gwid);
    if (conn->gwid == gwid) {
      DEBUG2("returning &connections[%d] = %p",i,  conn);
      return conn;
    };
  }
  return NULL;
};

Connection * conn_getclient(CLIENTID cid)
{
  int i;
  Connection * conn;
  for (i = 0; i <= maxsocket; i++) {
    conn = fd2conn(i);
    DEBUG2("testing connections[%d].cid = %#x != cid %#x", i, conn->cid, cid);
    if (conn->cid == cid) {
      DEBUG2("returning &connections[%d] = %p",i,  conn);
      return conn;
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
  Connection * connmcast = NULL, * conn;
  int i;
  for (i = 0; i < connectiontablesize; i++) {
    conn = fd2conn(i);
    if (conn->type == CONN_TYPE_MCAST) {
      connmcast = conn;
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
  Connection * conn;
  for (i = 0; i <= maxsocket; i++) {
    conn = fd2conn(i);
    if (conn->fd == UNASSIGNED) continue;
    if (conn->type == CONN_TYPE_BROKER) return conn;
    if (conn->type == CONN_TYPE_LISTEN) return conn;
  }
  return NULL;
};

Connection * conn_getfirstclient(void)
{
  int i;
  Connection * conn;
  for (i = 0; i <= maxsocket; i++) {
    conn = fd2conn(i);
    if (conn->fd == UNASSIGNED) continue;
    if (conn->type == CONN_TYPE_CLIENT)
      return conn;
  }
  return NULL;
};

Connection * conn_getfirstgateway(void)
{
  int i;
  Connection * conn;
  for (i = 0; i <= maxsocket; i++) {
    conn = fd2conn(i);
    DEBUG2(" looking at %d gwid = %#x  gateway=%d", 
	  i, conn->gwid, conn->type == CONN_TYPE_GATEWAY);
    if (conn->fd == UNASSIGNED) continue;
    if (conn->gwid != UNASSIGNED)
      return conn;

    if (conn->type == CONN_TYPE_GATEWAY)
      return conn;

  }
  return NULL;
};

void conn_setpeername (Connection * conn)
{
  char ipadr[128], * ipname;
  struct hostent *hent ;
  char * role = "unknown";
  int len, family, rc, id = UNASSIGNED, port;
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

    // postpone this until we got an async DNS lookup, gateway
    // connects fail when DNS is unavailable
#if 0
    hent = gethostbyaddr(&conn->peeraddr.sin4.sin_addr, sizeof(struct in_addr), family);
    if (hent == NULL) {
      Error("gethostbyaddr failed reason %s", hstrerror(h_errno));
      ipname = ipadr;
    } else {
      ipname = hent->h_name;
    };
#else 
    ipname = ipadr;
#endif

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

  Info ("fd=%d peeraddr_string = \"%s\", ipcaddr = %s", conn->fd, conn->peeraddr_string, ipcaddrstr);

  return;
};

void conn_getclientpeername (CLIENTID cid, char * name, int namelen)
{
  int i, fd = -1;
  Connection * conn;
  
  cid = CLTID2IDX(cid);

  for (i = 0; i <= maxsocket; i++) {
    conn = fd2conn(i);
    /*
    DEBUG2("conn_getclientpeername: foreach fd=%d cid=%d ?= %#x",
	  i, cid, CLTID2IDX(conn->cid));
    */
    if (CLTID2IDX(conn->cid) == cid) {
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

  conn = fd2conn(fd);
  DEBUG2("conn_getclientpeername: cid=%d fd = %d role: %d ?= %d",
	cid, fd, conn->role, SRB_ROLE_CLIENT);
  if (conn->role != SRB_ROLE_CLIENT) return;

  strncpy(name, conn->peeraddr_string, namelen);
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
   Connection * conn;

   /* when inited, 0< <RAND_MAX, and RAND_MAX should be the INT_MAX, or
      2^31. Just need to make sure is stays positive */
   if (fd < 0) {
     Error("conn_add on fd %d", fd);
     return;
   };


  lastconnectionid++;
  if (lastconnectionid <= 0) lastconnectionid=100;

  DEBUG("conn add on %d: conntblsize = %d", fd, connectiontablesize);


  if (connectiontablesize <= fd) {
    int i, l;
    int oldndir, newndir;

    /* we always round up the conn table to N*CONNDIRSIZE */

    if (connectiontablesize == 0) 
       oldndir = 0;
    else 
       oldndir = (connectiontablesize-1)/CONNDIRSIZE +1;

    DEBUG("we need to extend the conn table, current directories = %d",
	  oldndir);

    newndir = fd/CONNDIRSIZE +1;
    
    conn_dir = realloc(conn_dir, sizeof(Connection*) * newndir);
    DEBUG("increasing directories from %d to %d", oldndir, newndir);

    for (i = oldndir; i < newndir; i++) {
      DEBUG("addingdirectory %d", i);
      conn_dir[i] = malloc( sizeof(Connection) * CONNDIRSIZE);
      memset (conn_dir[i], 0, sizeof(Connection) * CONNDIRSIZE);
    };

    l = newndir * CONNDIRSIZE ;
    DEBUG("extending the connection table size from %d to %d", connectiontablesize, l);
    for (i = connectiontablesize; i < l; i++) {
      DEBUG("init connection table entry %d", i);
      conn_clear(fd2conn(i));
    };
    connectiontablesize = l;
  };

  conn = fd2conn(fd);
  if (conn->fd != UNASSIGNED) {
    Error("Internal error: attempted to do conn_add on fd=%d while connection already assigned cid=%#x gwid%#x", 
	  fd, conn->cid, conn->gwid);
    // TODO: shouldn't we:     return NULL;
    return conn;
  };

  conn->connectionid = lastconnectionid;
  conn->fd = fd;
  conn->connected = time(NULL);
  conn->messagebuffer = (char *) malloc(SRBMESSAGEMAXLEN+1);
  conn->cost = 100;

  poll_on(fd);
  conn_set(conn, role, type);

  len = sizeof(val);
  rc = setsockopt(fd, SOL_TCP, TCP_NODELAY, &val, len);
  DEBUG("Nodelay is set to 1 rc %d errno %d", rc, errno);  
 
  return conn;
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
  Connection * conn;

  conn = fd2conn(fd);

  /* if it's a gateway we call gw_closegateway() who call us later*/
  if (conn->gwid  != UNASSIGNED) {
    DEBUG("a close on a gateway, calling gw_closegateway() fd = %d", fd);
    gw_closegateway(conn);
    return;
  };

  poll_off(fd);
  conn_clear(conn);
  close (fd);

  return;
};

/* just for debugging purpose, print out all relevant data for all connections. */
char * conn_print(void)
{
  static char * output = NULL;
  static int buffersize = 0;
  int i, l;
  struct tm * ts;
  Connection * conn;

  if (maxsocket < 0) 
     i = 4096;
  else 
     i = maxsocket * 4096;

  if (buffersize != i) {
     buffersize = i;
     output = realloc(output, buffersize);
  };

  l = sprintf (output, "Printing connection table\n"
	       "  fd role version  mtu "
	       " cid gwid"
	       " state type"
	       " connected     lasttx        lastrx       "
	       " peeraddr   peerinstance GWID");

  for (i = 0; i<maxsocket+1; i++) {
     struct gwpeerinfo * pi;
     conn = fd2conn(i);
     if ( buffersize - l < 4096) Fatal("output buffer too small");

     //     DEBUG2("i = %d conn = %p", i, conn);
     if (conn->fd != -1) {
       
	l += sprintf (output+l, "\n  %2d %4d %7d %4d",
		      conn->fd, conn->role, 
		      conn->version, 
		      conn->mtu);
      
	l += sprintf (output+l, " %4d %4d", 
		      CLTID2IDX(conn->cid), GWID2IDX(conn->gwid) );
	
	l += sprintf (output+l, " %5d %4c",
		      conn->state, 
		      conn->type ? conn->type : '?');
	
	ts = localtime(&conn->connected);
	l+= strftime(output+l, 100, " %Y%m %H%M%S", ts);
	ts = localtime(&conn->lasttx);
	l+= strftime(output+l, 100, " %Y%m %H%M%S", ts);
	ts = localtime(&conn->lastrx);
	l+= strftime(output+l, 100, " %Y%m %H%M%S", ts);
	
	l += sprintf (output+l, " %s", 
		      conn->peeraddr_string);
	
	
	pi = (struct gwpeerinfo *) conn->peerinfo;
	if (pi == NULL) continue;
	
	/*
	DEBUG2("%s", output);
	
	DEBUG2(" . fd=%d pi =%p pi->inst = %p . ",  i, pi, pi->instance );
	{
	   char buffer[MWMAXNAMELEN*3];
	   int x, ll = 0;

	   DEBUG2("conn = %p pi = %p buffer %p length %d", conn, pi, buffer, MWMAXNAMELEN*3);

	   for (x = 0; x < MWMAXNAMELEN; x++) {
	      DEBUG2("conn = %p pi = %p buffer %p length %d : x = %d ll = %d", 
		     conn, pi, buffer, MWMAXNAMELEN*3, 
		     x, ll);
	      DEBUG2("char %x", (unsigned char) pi->instance[x]);
	      ll += sprintf (buffer+ll, "%hhx ", (unsigned char) pi->instance[x]);
	   };
	   DEBUG2(" hex dump: %s", buffer);
	};
	
	DEBUG2(" . pi->inst len = %d . ",strlen( pi->instance) );
	DEBUG2(" . pi->inst = \"%s\" . ", pi->instance );
	*/
	
	l += sprintf (output+l, " %s",  pi->instance );
	
	l += sprintf (output+l, " %d",  GWID2IDX(pi->gwid));
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

////////////////////////////////////////////////////////////////////////

/* the read fifo is a poll/select queue of connections that has data,
   probably a completemessage in the receive buffer. conn_select will
   attempt to dequeue here before doing select. */

static Connection * read_pending_fifo_head = NULL;
static Connection * read_pending_fifo_tail = NULL;
static Connection * write_pending_fifo = NULL;

void conn_read_fifo_print(void)
{
   Connection * this;
   Connection * head = read_pending_fifo_head;
   Connection * tail = read_pending_fifo_tail;
   
   DEBUG2("fifo HEAD = %p", head);
   for (this = head; this != NULL; this = this->read_fifo_next) {
      DEBUG2 ("     conn = %p <fd=%-4d next=%p prev=%p leftover=%d", 
	      this, this->fd, this->read_fifo_next, this->read_fifo_prev, this->leftover);
   };
   DEBUG2("fifo TAIL = %p", tail);
};


void conn_read_fifo_enqueue(Connection *conn)
{
   Assert (conn);
   Assert (conn->read_fifo_prev == NULL);
   Assert (conn->read_fifo_next == NULL);

   if (read_pending_fifo_head == NULL) {
      DEBUG("fifo empty inserting %p", conn);
      read_pending_fifo_head = read_pending_fifo_tail = conn;
      conn->read_fifo_prev = conn->read_fifo_next = NULL;
      conn_read_fifo_print();
      return;
   };

   DEBUG("fifo not empty inserting %p", conn);
   read_pending_fifo_head->read_fifo_prev = conn;
   conn->read_fifo_next = read_pending_fifo_head;
   read_pending_fifo_head = conn;
   conn_read_fifo_print();
   return;
};

Connection * conn_read_fifo_dequeue(void) 
{
   Connection * conn;
   conn_read_fifo_print();
   
   if (read_pending_fifo_head == NULL) {
      DEBUG("fifo empty");
      assert (read_pending_fifo_tail == NULL);
      return NULL;
   };

   if (read_pending_fifo_head == read_pending_fifo_tail) {
      conn = read_pending_fifo_head;
      DEBUG("fifo has only one element: %p", conn);
      conn->read_fifo_prev = conn->read_fifo_next = NULL;
      read_pending_fifo_head = read_pending_fifo_tail = NULL;      
      return conn;
   };
   
   conn = read_pending_fifo_tail;
   DEBUG("dequeuing last element: %p", conn);
   conn->read_fifo_prev->read_fifo_next = NULL;
   read_pending_fifo_tail = conn->read_fifo_prev;
   conn->read_fifo_prev = NULL;

   return conn;
};
   
   
   
void conn_write_pending(Connection *);
Connection * conn_next_write(void);


DECLAREEXTERNMUTEX(bigmutex);

int conn_select(Connection ** pconn, int * cause, time_t deadline)
{
  static fd_set rfds, wfds, errfds;
  static int lastret = 0;
  static int n = 0;
  static struct timeval tv, * tvp;
  int  i, fd;
  int now;
  Connection * conn;


  DEBUG2("Beginning conn_select() select version");    
  *pconn = NULL;

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

  conn = conn_read_fifo_dequeue();
  if (conn) {
     // we do a biglock unlock/lock to let the ipcmain do something between our processing
     UNLOCK_BIGLOCK();
     DEBUG("Connection %p fd=%d has dat ain buffer leftover=%d", conn, conn->fd, conn->leftover);
     LOCK_BIGLOCK();
     
     *pconn = conn;
     *cause = COND_READ;
     return conn->fd;
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

    UNLOCK_BIGLOCK();

    n = select(maxsocket+1, &rfds, NULL, &errfds, tvp);

    LOCK_BIGLOCK();

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
      conn = fd2conn(fd);
      *pconn = conn;
      return fd;
    };
  };

  //* we should never get here!
  
  Error("we have %d left after a select unaccounted for", n);
  sleep(10);
  n = 0;
  return 0;
};









