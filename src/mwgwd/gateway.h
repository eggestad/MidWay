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
 * Revision 1.6  2002/09/22 22:58:50  eggestad
 * prototype fixup
 *
 * Revision 1.5  2002/07/07 22:45:48  eggestad
 * *** empty log message ***
 *
 * Revision 1.4  2001/10/03 22:38:10  eggestad
 * pids now in globals struct
 *
 * Revision 1.3  2001/09/15 23:49:38  eggestad
 * Updates for the broker daemon
 * better modulatization of the code
 *
 * Revision 1.2  2000/08/31 22:08:31  eggestad
 * new global vars in gateway.c
 *
 * Revision 1.1  2000/07/20 18:49:59  eggestad
 * The SRB daemon
 *
 */

#ifndef _GATEWAY_H
#define _GATEWAY_H

#include <urlencode.h>
#include <sys/types.h>

#include <MidWay.h>
#include <connection.h>

typedef struct {
  int shutdownflag;
  char * mydomain;
  char * myinstance;
  pid_t tcpserverpid;
  pid_t ipcserverpid;
} globaldata;

// TODO we have a limitation on hostnames. 
#define MAXADDRLEN 255
struct gwpeerinfo {
  char hostname[MAXADDRLEN + 1];
  char instance[MWMAXNAMELEN];
  int domainid;
  
  time_t last_connected;
  GATEWAYID gwid;
  Connection * conn;  

  /* auth data, no user, with GW to GW it's a domain/passowrd, thus
     not user necessary */
  char * password; 
  // SSL

  // Imports

  // exports

  // events
  
} ;

int gwattachclient(Connection *, char *, char *, char *, urlmap *);
int gwdetachclient(int);

int gwattachgateway(char *);
int gwdetachgateway(int);

GATEWAYID allocgwid(int location, int role);
void freegwid(GATEWAYID gwid);

int gw_addknownpeer(char * instance, char * domain, struct sockaddr * peeraddr, struct gwpeerinfo ** piptr);
int gw_addknownpeerhost(char * hostname);
void gw_connectpeer(struct gwpeerinfo * peerinfo);
void gw_connectpeers(void);
int gw_peerconnected(char * instance, char * peerdomain, Connection * conn);
void gw_provideservices_to_peer(GATEWAYID gwid);
void gw_provideservice_to_peers(char * service);
void gw_closegateway(GATEWAYID gwid);
int gw_getcostofservice(char * service);
void gw_setipc(struct gwpeerinfo * pi);

#endif

#ifndef _GATEWAY_C
extern globaldata globals;
#endif
