/*
  The MidWay API
  Copyright (C) 2002 Terje Eggestad

  The MidWay API is free software; you can redistribute it and/or
  modify it under the terms of the GNU Library General Public License as
  published by the Free Software Foundation; either version 2 of the
  License, or (at your option) any later version.
  
  The MidWay API is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Library General Public License for more details.
  
  You should have received a copy of the GNU Library General Public
  License along with the MidWay distribution; see the file COPYING.  If not,
  write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA. 
*/

/*
 * $Id$
 * $Name$
 * 
 * $Log$
 * Revision 1.2  2002/10/09 12:30:30  eggestad
 * Replaced all unions for sockaddr_* with a new type SockAddress
 *
 * Revision 1.1  2002/07/07 22:45:48  eggestad
 * *** empty log message ***
 *
 *
 */

#ifndef _CONNECTION_H
#define _CONNECTION_H

#include <sys/socket.h>
/* ip */
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
/* unix */
#include <sys/un.h>

/* information on every connection. Most importantly it keeps
   the message buffer, and if this is a client or gateway,

   This is also a shared struct between clients and gateways.
   Primarely in order to use must of the client API as possibe in the
   gateway. At the same time both gateway and client need it to keep 
   status, message buffers and SSL info etc. All SRB call are passed 
   a COnnection pointer. 

*/

union _SockAddress {
  struct sockaddr sa;
  struct sockaddr_in  sin4;
  struct sockaddr_in6 sin6;
  struct sockaddr_un sun;
};

typedef union _SockAddress SockAddress;

typedef struct {
  int fd;
  int role;
  int version;

  int protocol;
  SockAddress peeraddr;
  int    mtu;
  int 	 rejects;
  char * domain;
  char * messagebuffer;
  int    leftover;  

  /* the latter is used by the gateway only */
  int connectionid;
  CLIENTID  cid;
  GATEWAYID gwid;
  int    state;   // connection states, defines below
  int    type;    // peer type, defines below
  int    cost;    // the SRB cost to the peer
  time_t connected;
  time_t lasttx;
  time_t lastrx;
  int    reverse;
  void * peerinfo;
} Connection;

/* type parm to conn_add, all possible types of sockets (peers) */
#define CONN_TYPE_CLIENT  01
#define CONN_TYPE_GATEWAY 02
#define CONN_TYPE_BROKER 010 
#define CONN_TYPE_LISTEN 020
#define CONN_TYPE_MCAST  040

/* the state, only used in the gateway when we do no_blocking to
   remeber if we're waiting for a connect() to complete, or we got an
   connect, but waiting to SRB_INIT, or if're connected. SSL adds more states. */
enum {  CONNECT_STATE_CONNWAIT = 1, 
	CONNECT_STATE_READYWAIT, 	
	CONNECT_STATE_INITWAIT, 
	CONNECT_STATE_UP, 
	CONNECT_STATE_CLOSING };

#endif // _CONNECTION_H


