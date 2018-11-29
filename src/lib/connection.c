/*
  The MidWay API
  Copyright (C) 2003 Terje Eggestad

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
 * Revision 1.3  2005/12/07 11:44:16  eggestad
 * large data SRB patch
 *
 * Revision 1.2  2003/09/25 19:36:17  eggestad
 * - had a serious bug in the input handling of SRB messages in the Connection object, resulted in lost messages
 * - also improved logic in blocking/nonblocking of reading on Connection objects
 *
 * Revision 1.1  2003/08/07 11:19:55  eggestad
 * Merge of client and mwgwd recieving SRB messages functions.
 *
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>

#include <MidWay.h>
#include <SRBprotocol.h>
#include <urlencode.h>

#include <connection.h>

static char * RCSId UNUSED = "$Id$";


/* at some poing we're going to do OpenSSL. Then conn_read, and
   conn_write will hide the SSL, and the rest of MidWay can treat it
   as a stream connection. */
int _mw_conn_read(Connection * conn, int blocking)
{
   int rc; 
   socklen_t l;
  int flags;
  char buffer[64];
  char * remaddr;
  unsigned long maxrecvlen;
  #ifdef MSG_NOSIGNAL
  flags = MSG_NOSIGNAL;
  #else
  flags = 0;
  #endif
  if (!blocking)
     flags |= MSG_DONTWAIT;

  if (conn->fd == -1) return 0;
  /* if it is the mcast socket, this is a UDP socket and we use
     recvfrom. The conn->peeraddr wil lthen always hold the address of
     sender for the last recv'ed message, which is OK, with UDPO we
     must either get the whole message or not at all. */

  if (conn->type == CONN_TYPE_MCAST) {
     remaddr = (char *) &conn->peeraddr.sa;
     l = sizeof(conn->peeraddr);     
     DEBUG("UDP recv, leftover = %d, better be 0!", conn->leftover);      
     assert ( conn->leftover == 0);
  } else {
     
     remaddr = NULL;
     l = 0;
  };
  
  TIMEPEGNOTE("doing recvfrom");
  
  
  if (remaddr != NULL) {
     DEBUG("UDP: read %d bytes from %s:%d", rc, 
	     inet_ntop(AF_INET, &conn->peeraddr.sin4.sin_addr, buffer, 64),
	   ntohs(conn->peeraddr.sin4.sin_port)); 
  };
  
  DEBUG3("doing recvfrom stream fd=%d buf=%p start=%p leftover=%d flags=%#x", 
	 conn->fd, conn->messagebuffer, conn->som, conn->leftover, flags);
  errno = 0;
  
  maxrecvlen =  SRBMESSAGEMAXLEN;
  maxrecvlen -= (conn->som+conn->leftover) - conn->messagebuffer;
  DEBUG3("there is %lu octets left in the message buffer", maxrecvlen);
  
  DEBUG3("doing recvfrom stream fd=%d buf=%p insert@%p max=%lu flags=%#x", 
	 conn->fd, conn->messagebuffer, conn->som+conn->leftover, maxrecvlen, flags);
  
  rc = recvfrom(conn->fd, conn->som+conn->leftover,  maxrecvlen, flags, (struct sockaddr *)remaddr, &l);
  if (rc > 0) {
       conn->leftover += rc;
       conn->som[conn->leftover] = '\0';  
  };

  DEBUG3("recvfrom => %d errno = %d", rc, errno);
  TIMEPEGNOTE("done");
  return rc;
};


int _mw_conn_write(Connection * conn, char * msg, int mlen, int flags)
{
   int rc;
   int socketflags = 0;
#ifdef MSG_NOSIGNAL
   socketflags = MSG_NOSIGNAL;
#endif

   if (flags & MWNOBLOCK) socketflags |= MSG_DONTWAIT;

   if (!conn) return -EINVAL;

   rc = sendto (conn->fd, msg, mlen, socketflags, NULL, -1);

   if (rc < 0) return -errno;
   return rc;
};
