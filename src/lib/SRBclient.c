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


static char * RCSId = "$Id$";
static char * RCSName = "$Name$"; /* CVS TAG */

/*
 * $Log$
 * Revision 1.1  2000/07/20 19:12:13  eggestad
 * The SRB protcol
 *
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>


#include <MidWay.h>
#include <SRBprotocol.h>
#include <urlencode.h>

static char messagebuffer[SRBMESSAGEMAXLEN];

struct state {
  int fd ;
  int rejects;
  char * domain;
  float version;
} ;
static struct state connectionstate = { -1, 1, NULL, 0.0 };

/**********************************************************************
 * API for Sending messages
 **********************************************************************/
static int sendmessage(void)
{
  int len;

  len = strlen(messagebuffer);
  if (len < SRBMESSAGEMAXLEN - 2)
    len += sprintf(messagebuffer+len, "\r\n");
  else {
    errno = E2BIG;
    return -1;
  };

  
  write(connectionstate.fd, messagebuffer, len);
  return len;
};

static void _mw_srbsendreject(char * message, int offset, 
			      char * causefield, char * causevalue, 
			      int rc)
{
  int len;
  urlmap * map = NULL;

  /* if peer don't wantn rejects. */
  if (!connectionstate.rejects) return;

  map = urlmapaddi(map, SRB_REASONCODE, rc);
  map = urlmapadd(map, SRB_REASON, strreason(rc));

  len = sprintf(messagebuffer, "%s%c", SRB_REJECT, SRB_NOTIFICATIONMARKER);

  if (offset > -1) 
    map = urlmapaddi(map, SRB_OFFSET, offset);
  
  map = urlmapadd(map, SRB_MESSAGE, message);

  if (causefield != NULL) 
    map = urlmapadd(map, SRB_CAUSEFIELD, causefield);

  if (causevalue != NULL) 
    map = urlmapadd(map, SRB_CAUSEVALUE, causevalue);

  len += urlmapnencode(messagebuffer+len, SRBMESSAGEMAXLEN-len, map);
  urlmapfree(map);

  sendmessage();
};

static void _mw_srbsendterm(int grace)
{
  int len;

  if (grace == -1) {
    len = sprintf(messagebuffer, "%s%c", SRB_TERM, SRB_NOTIFICATIONMARKER);
  } else 
    len = sprintf(messagebuffer, "%s%c%s=%d", 
		  SRB_TERM, SRB_REQUESTMARKER, SRB_GRACE, grace);

  sendmessage();
  
};

static int _mw_srbsendinit(char * user, char * password, char * name, char * domain)
{
  int len;
  urlmap * map = NULL;
  char * auth = SRB_AUTH_NONE;

  if (name == NULL) {
    errno = EINVAL;
    return -1;
  };

  /* mandatory felt */
  map = urlmapadd(map, SRB_VERSION, SRBPROTOCOLVERSION);
  map = urlmapadd(map, SRB_TYPE, SRB_CLIENT);
  map = urlmapadd(map, SRB_NAME, name);
  
  /* optional */
  if (user != NULL) {
    auth = SRB_AUTH_UNIX;
    map = urlmapadd(map, SRB_USER, user);
  };

  if (password != NULL)
    map = urlmapadd(map, SRB_PASSWORD, password);

  if (domain != NULL)
    map = urlmapadd(map, SRB_DOMAIN, domain);

  map = urlmapadd(map, SRB_AUTHENTICATION, auth);
  sendmessage();
  
};


int _mwattach_srb(char * url, char * name, char * username, char * password, int flags)
{
  /* first we get the IP address */

  /* connect */

  /* read SRBREADY, if present */

  /* send init */

  /* get reply */

  /* if rejected for version, attempt to negotioate protocol version */

};


  
