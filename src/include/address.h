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
 * $Log$
 * Revision 1.1  2000/03/21 21:04:03  eggestad
 * Initial revision
 *
 * Revision 1.1.1.1  2000/01/16 23:20:12  terje
 * MidWay
 *
 */

#include <netinet/in.h>

/* protocol types */
#define MWSYSVIPC 1
#define MWPOSIXIPC 2
#define MWNETWORK 3
#define MWHTTP    4
#define MWTCPIPv4 3
#define MWTCPIPv6 6

typedef struct {
  int protocol ;
  int sysvipckey ;
  char * posixipcpath ;
  struct sockaddr_in * remoteaddress_v4;
  struct sockaddr_in6 * remoteaddress_v6;  
} mwaddress_t;

#ifdef _ADDRESS
mwaddress_t _mwaddress = {0, -1, NULL, NULL, NULL};
#else 
extern mwaddress_t _mwaddress;
#endif

int _mwdecode_url(char * url);


