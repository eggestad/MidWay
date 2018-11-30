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


#ifndef _CONNECTION_H
#define _CONNECTION_H

#include <MidWay.h>


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

typedef struct Connection Connection;

struct Connection {
  int fd;
  int role;
  int version;

#ifdef USETHREADS
   DECLARESTRUCTMUTEX(sendlock);
   DECLARESTRUCTMUTEX(recvlock);
#endif
  
  int protocol;
  SockAddress peeraddr;
  char   peeraddr_string[128];
  int    mtu;
  int 	 rejects;
  char * domain;

  char * messagebuffer;
  int    leftover;  
  char * next_message_boundry_marker;
  //  int    possible_message_in_buffer;
  char * som; //StartOfMessage
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

   /* a read fifo and write fifo pointers. Used in mwgwd/connections.c
      to maintain lists of connections that have pending reads and
      writes. */
   Connection * read_fifo_prev, * read_fifo_next;
   Connection * write_fifo_prev, * write_fifo_next;
   
} ;

/* type parm to conn_add, all possible types of sockets (peers) */
#define CONN_TYPE_CLIENT  'C'
#define CONN_TYPE_GATEWAY 'G'
#define CONN_TYPE_BROKER  'B'
#define CONN_TYPE_LISTEN  'L'
#define CONN_TYPE_MCAST   'M'

/* the state, only used in the gateway when we do no_blocking to
   remeber if we're waiting for a connect() to complete, or we got an
   connect, but waiting to SRB_INIT, or if're connected. SSL adds more states. */
enum {  CONNECT_STATE_CONNWAIT = 1, 
	CONNECT_STATE_READYWAIT, 	
	CONNECT_STATE_INITWAIT, 
	CONNECT_STATE_UP, 
	CONNECT_STATE_CLOSING };


#endif // _CONNECTION_H


int _mw_conn_read(Connection * conn, int blocking);
int _mw_conn_write(Connection * conn, char * msg, int mlen, int flags);
