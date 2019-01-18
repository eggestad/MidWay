/*
  MidWay
  Copyright (C) 2000-2005 Terje Eggestad

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

#ifndef _GATEWAY_H
#define _GATEWAY_H

/** @file 
    This is the include for gateway,c. The gateway module keep
    track of peers and clients.
*/

#include <urlencode.h>
#include <sys/types.h>

#include <MidWay.h>
#include <connection.h>
#include <SRBprotocol.h>

/**
   The global variables. We put them in a separate struct for
   readability.
*/
typedef struct {
  int shutdownflag;
  char * mydomain;
  char * myinstance;
  pid_t tcpserverpid;
  pid_t ipcserverpid;
   /**
      The big lock indicator. Used for a work-around for locking bugs,
      this ensure that only the IPC thread or the SRB thread run at the
      same time. The correct normal operation is that the fine granular
      lock shall be correct.
   */
  int biglock;
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

void gw_setmystatus(int status);
GATEWAYID allocgwid(int location, int role);
void freegwid(GATEWAYID gwid);

int gw_addknownpeer(char * instance, char * domain, struct sockaddr * peeraddr, struct gwpeerinfo ** piptr);
int gw_addknownpeerhost(char * hostname);
void gw_connectpeer(struct gwpeerinfo * peerinfo);
void gw_connectpeers(void);
int gw_peerconnected(char * instance, char * peerdomain, Connection * conn);
void gw_provideservices_to_peer(GATEWAYID gwid);
void gw_provideservice_to_peers(char * service);
void gw_send_to_peers(SRBmessage * srbmsg);

void gw_closegateway(Connection *);
int gw_getcostofservice(char * service);
void gw_setipc(struct gwpeerinfo * pi);
void gw_sendmcasts(void);

Connection * gwlocalclient(CLIENTID cid);

void signal_tcp_thread(int sig);

#endif


/* the big lock. This is locked when ipcgetmessage or conn_select
   return. THis ensure that only one of the two threads do real work
   at the same time. This is just an easy way to circumvent race
   conditions. */
// we've got an bad race cond in imp list... so default on
#define DEFAULT_BIGLOCK_FLAG 1
#define envBIGLOCK "MWGWD_BIGLOCK"
#define    UNLOCK_BIGLOCK() do { if (globals.biglock) UNLOCKMUTEX(bigmutex);} while (0)
#define    LOCK_BIGLOCK()   do { if (globals.biglock) LOCKMUTEX(bigmutex); } while (0)

#ifndef _GATEWAY_C
extern globaldata globals;
#endif
