/*
  MidWay
  Copyright (C) 2001 Terje Eggestad

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

/* $Id$ */

/*
 * $Log$
 * Revision 1.1  2001/09/15 23:40:09  eggestad
 * added the broker daemon
 * 
 */


#include <sys/socket.h>
/* ip */
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
/* unix */
#include <sys/un.h>

#include <MidWay.h>

/* information on every connection. Most importantly it keeps
   the message buffer, and if this is a client or gateway,
*/
typedef struct {
  int fd;
  int connectionid;
  CLIENTID  client;
  GATEWAYID gateway;
  int    role;
  int    listen;
  int    broker;
  char * messagebuffer;
  int    leftover;  
  int    protocol;
  union  {
    struct sockaddr_in  ip4;
    struct sockaddr_in6 ip6;
    struct sockaddr_un un;
  } ;
  int    mtu;
  time_t connected;
  time_t lasttx;
  time_t lastrx;
  int 	 rejects;
  int    reverse;
} Connection;

/* type parm to conn_add, all possible types of sockets (peers) */
#define CONN_BROKER 20 
#define CONN_LISTEN 10
#define CONN_CLIENT 0
#define CONN_GATWEAY 1

/* causes from dp_select() */
#define COND_READ  1
#define COND_ERROR 3

void conn_init(void);

void conn_setinfo(int fd, int * id, int * role, int * rejects, int * reverse);
int conn_getinfo(int fd, int * id, int * role, int * rejects, int * reverse);
char * conn_getclientpeername (CLIENTID cid);
char * conn_getpeername (int fd);

Connection * conn_add(int fd, int role, int type);
void conn_del(int fd);

Connection * conn_getentry(int fd);
Connection * conn_getfirstclient(void);
Connection * conn_getfirstlisten(void);
Connection * conn_getbroker(void);

int do_select(int * cause, int timeout);

