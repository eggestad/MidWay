/*
  MidWay
  Copyright (C) 2005 Terje Eggestad

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
 * Revision 1.1  2005/10/12 22:46:27  eggestad
 * Initial large data patch
 *
 *
 */

#include <MidWay.h> 
#include <urlencode.h>
#include <ipcmessages.h>
#include <SRBprotocol.h>

int storeSRBCall(MWID mwid, SRBhandle_t nethandle, urlmap *map, Connection * conn, int datatotal, 
		 char * service, char * instance, char * domain, char * clientname, int timeout, int flags);
int storeSRBData(MWID mwid, SRBhandle_t nethandle, char * data, int datalen);
int storeSRBReply(MWID mwid, SRBhandle_t nethandle, urlmap *map, int rcode, int apprcode, int datatotal);
int storeIPCCall(Call * cmsg, Connection *conn);
int storeIPCReply(Call * cmsg, Connection ** pconn, urlmap ** pmap);
