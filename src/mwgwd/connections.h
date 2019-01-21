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


#include <MidWay.h>

#include <connection.h>

/* causes from do_select() */
#define COND_READ  1
#define COND_WRITE 2
#define COND_ERROR 4

void conn_init(void);

void conn_setinfo(int fd, int * id, int * role, int * rejects, int * reverse);
int  conn_getinfo(int fd, int * id, int * role, int * rejects, int * reverse);
void conn_getclientpeername (CLIENTID cid, char * buff, int bufflen);
void conn_setpeername (Connection * conn);

Connection * conn_add(int fd, int role, int type);
void conn_del(int fd);
void conn_set(Connection * conn, int role, int type);

Connection * conn_getentry(int fd);
Connection * conn_getfirstclient(void);
Connection * conn_getfirstlisten(void);
Connection * conn_getfirstgateway(void);
Connection * conn_getbroker(void);
Connection * conn_getmcast(void);
Connection * conn_getgateway(GATEWAYID gwid);
Connection * conn_getclient(CLIENTID cid);

void conn_read_fifo_enqueue(Connection *conn);
Connection * conn_read_fifo_dequeue(void);

int conn_select(Connection ** pconn, int * cause, time_t timeout);


void debugDumpSocketTable(FILE *);
void conn_print(FILE *);

void unpoll_write(int fd);
void poll_write(int fd);



