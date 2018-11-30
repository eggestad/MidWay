/*
  The MidWay API
  Copyright (C) 2005 Terje Eggestad

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

#include <SRBprotocol.h>
#include <connection.h>

extern char * _mw_srbmessagebuffer;

int _mw_srbsendready(Connection * conn, char * domain);
int _mw_srbsendinitreply(Connection * conn, SRBmessage * srbmsg, int rcode, char * field);
int _mw_srbsendcallreply(Connection * conn, SRBmessage * srbmsg, char * data, int len, 
			 int apprcode, int rcode, int flags);
int _mw_srbsendready(Connection * conn, char * domain);
int _mw_srbsendprovide(Connection * conn, char * service, int cost);
int _mw_srbsendunprovide(Connection * conn, char * service);
int _mw_srbsendgwinit(Connection * conn);

int srbDoMessage(Connection * conn, SRBmessage * srbmsg);
