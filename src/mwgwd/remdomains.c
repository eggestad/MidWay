/*
  MidWay
  Copyright (C) 2002 Terje Eggestad

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

/*
 * $Log$
 * Revision 1.1  2002/07/07 22:45:48  eggestad
 * *** empty log message ***
 *
 *
 */

#include <pthread.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <MidWay.h>
#include "connection.h"
#include "connections.h"
#include "gateway.h"


static char * RCSId UNUSED = "$Id$";
static char * RCSName UNUSED = "$Name$";

struct Imports {
  char * localname;
  char * remotename;

  struct Imports * next ;

};

struct RemoteDomain {
  char * name;
  struct Imports * imports;
  struct Imports * exports;

  //  Connection * conn;
  GATEWAYID gwid;

  // adress
  // authentication.

  struct RemoteDomain * next;
};

static struct RemoteDomain * remDomains = NULL;

int remdom_getid(char * domain) 
{
  if (domain == NULL) {
    return -EINVAL;
  };

  if (strcmp(domain, globals.mydomain) == 0) return 0;
  return -ENOENT;
};

void remdom_sendmcasts(void)
{
  int fd;
  struct RemoteDomain * rd;

  fd = conn_getmcast()->fd;

  for (rd = remDomains; rd != NULL; rd = rd->next) {
    
    DEBUG("sending multicast query on fd=%d for remote domain %s", 
	  fd, globals.mydomain);

    _mw_sendmcastquery(fd, rd->name, NULL);
  };
  return;
};
