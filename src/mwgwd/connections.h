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
 * Revision 1.7  2003/09/25 19:36:19  eggestad
 * - had a serious bug in the input handling of SRB messages in the Connection object, resulted in lost messages
 * - also improved logic in blocking/nonblocking of reading on Connection objects
 *
 * Revision 1.6  2003/08/06 23:16:19  eggestad
 * Merge of client and mwgwd recieving SRB messages functions.
 *
 * Revision 1.5  2003/03/16 23:50:24  eggestad
 * Major fixups
 *
 * Revision 1.4  2002/10/22 21:58:21  eggestad
 * Performace fix, the connection peer address, is now set when establised, we did a getnamebyaddr() which does a DNS lookup several times when processing a single message in the gateway (Can't believe I actually did that...)
 *
 * Revision 1.3  2002/07/07 22:45:48  eggestad
 * *** empty log message ***
 *
 * Revision 1.2  2001/10/05 14:34:19  eggestad
 * fixes or RH6.2
 *
 * Revision 1.1  2001/09/15 23:40:09  eggestad
 * added the broker daemon
 * 
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



char * conn_print(void);

void unpoll_write(int fd);
void poll_write(int fd);



