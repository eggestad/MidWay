/*
  MidWay
  Copyright (C) 2000 Terje Eggestad

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
 * Revision 1.2  2000/08/31 22:19:31  eggestad
 * added prototype for tcpgetclientpeername()
 *
 * Revision 1.1  2000/07/20 18:49:59  eggestad
 * The SRB daemon
 *
 */

/* the max number of connections. */
#define FD_MAX 1024

void tcpsetconninfo (int fd, int * id, int * role, int * rejects, int * reverse);
int  tcpgetconninfo (int fd, int * id, int * role, int * rejects, int * reverse);
int tcpgetconnid(int fd);
char * tcpgetconnpeername (int fd);
char * tcpgetclientpeername (CLIENTID);

void tcpcloseconnection (int fd);
int  sendmessage (int fd, char * message, int len);

void tcpserverinit(void);
void tcpcloseall(void);
int tcpstartlisten(int port, int role);
void * tcpservermainloop(void *);
