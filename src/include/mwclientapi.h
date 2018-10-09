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
  License along with the MidWay distribution; see the file COPYING.  If not,
  write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA. 
*/

/*
 * $Id$
 * $Name$
 * 
 *  $Log$
 *  Revision 1.4  2004/04/08 10:34:05  eggestad
 *  introduced a struct with pointers to the functions implementing the midway functions
 *  for a given protocol.
 *  This is in preparation for be able to do configure with/without spesific protocol.
 *  This creates a new internal API each protocol must addhere to.
 *
 *
 */

#ifndef _MWCLIENTAPI_H
#define _MWCLIENTAPI_H

#include <netinet/in.h>

struct _mwcred_t {
   int authtype;
   char * username;
   union  {
      char * password;
      void * data;
   } cred;
};

typedef struct _mwcred_t mwcred_t;

typedef struct _mwaddress_t mwaddress_t;

// to find the functions that will be assigned to these, look in
// mwclientapi.c for notconnected and not implemeted dummies. There is
// a _mwipcprotosetup() in mwclientipcapi.c for sysvipc funcs, and
// _mwsrbprotosetup() in SRBclient.c

struct mwprotocol {
   int (*attach)(int type, mwaddress_t * mwadr, const char * cname, mwcred_t * cred, int flags);
   int (*detach)(void);

   int (*acall) (const char * svcname, const char * data, int datalen, int flags);
   int (*fetch) (int *hdl, char ** data, int * len, int * appreturncode, int flags);
   int (*listsvc) (const char * glob, char *** list, int flags);
   
   int (*event) (const char * evname, const char * data, int datalen, const char * username, const char * clientname, 
		 MWID fromid, int remoteflag);
   void (*recvevents) (void);
   int (*subscribe) (const char * pattern, int id, int flags);
   int (*unsubscribe) (int id); 
};


/* protocol types */
#define MWNOTCONN  0
#define MWSYSVIPC  1
#define MWPOSIXIPC 2
#define MWSRBP     3
#define MWHTTP     4

struct _mwaddress_t {
  int protocol ;
  struct mwprotocol proto;
  int sysvipckey ;
  char * domain;
  char * posixipcpath ;
  union _ipaddress {
    struct sockaddr * sa;
    struct sockaddr_in * sin4;
    struct sockaddr_in6 * sin6;  
  } ipaddress ;
};

#endif

mwaddress_t * _mw_get_mwaddress(void);
void _mw_clear_mwaddress(void);

int _mw_deadline(struct timeval * tv_deadline, float * ms_deadlineleft);
int _mw_isattached(void);
int _mw_fastpath_enabled(void);
int _mw_nexthandle(void);

void _mw_doevent(int subid, const char * event, const char * data, int datalen);

int _mw_notimp_attach (int type, mwaddress_t * mwadr, const char * cname, mwcred_t * cred, int flags);
int _mw_notimp_detach(void);

int _mw_notimp_acall (const char * svcname, const char * data, int datalen, int flags);
int _mw_notimp_fetch (int *hdl, char ** data, int * len, int * appreturncode, int flags);
int _mw_notimp_listsvc (const char *glob, char *** list, int flags);

int _mw_notimp_event (const char * evname, const char * data, int datalen, const char * username, const char * clientname, 
		 MWID fromid, int remoteflag);
void _mw_notimp_recvevents(void);
int _mw_notimp_subscribe (const char * pattern, int id, int flags);
int _mw_notimp_unsubscribe (int id); 


int _mw_notconn_attach (int type, mwaddress_t * mwadr, const char * cname, mwcred_t * cred, int flags);
int _mw_notconn_detach (void);

int _mw_notconn_acall (const char * svcname, const char * data, int datalen, int flags);
int _mw_notconn_fetch (int *hdl, char ** data, int * len, int * appreturncode, int flags);
int _mw_notconn_listsvc (const char *glob, char *** list, int flags);

int _mw_notconn_event (const char * evname, const char * data, int datalen, const char * username, const char * clientname, 
		 MWID fromid, int remoteflag);
void _mw_notconn_recvevents(void);
int _mw_notconn_subscribe (const char * pattern, int id, int flags);
int _mw_notconn_unsubscribe (int id); 

