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
 * Revision 1.1  2000/03/21 21:04:06  eggestad
 * Initial revision
 *
 * Revision 1.1.1.1  2000/01/16 23:20:12  terje
 * MidWay
 *
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <MidWay.h>

#define _ADDRESS
#include <address.h>

/* this function ned to find out if the url has a protocol header
   protocol://     
   legal protocols are ipc, midway (mwp?), http
   If not present, the url is a file path.

   hlen is the offset to the first char after ://
*/
static int url_header(char *url , int * hlen)
{
  char * tmpptr;
  int len;

  if (url == NULL ) return -EINVAL;
  if (hlen == NULL ) return -EINVAL;

  *hlen = 0;
  tmpptr = strstr(url, "://");

  if (tmpptr != NULL) {
    len = tmpptr - url;
    *hlen = len+3;
    
    mwlog(MWLOG_ERROR, "In the URL %s the protocol is %*.*s", 
	  url, len, len, url);

    /* check for IPC:  */
    if (len == 3 && ( 0 == strncasecmp("ipc", url, len))) {
      return MWSYSVIPC;
    };
    
    /* check for MidWay: */
    if (len == 6 && ( 0 == strncasecmp("midway", url, len))) {
      return MWNETWORK;
    };

    /* check for http: */
    if (len == 4 && ( 0 == strncasecmp("http", url, len))) {
      return MWHTTP;
    };
    return -1;
  }
 
  return 0;
};

/* we now decode whats after ipc://, this is either
   a number for sysv ipc 
   a name (A directory ~/.MidWay/name is expected.  NYI
   a path for POSIX IPC
*/
static int url_decode_sysvipc(char * url)
{
  char * tmpptr;
  
  errno = 0;
  tmpptr = strstr(url, "://");
  if (tmpptr != NULL) url = tmpptr+3;
  _mwaddress.sysvipckey = strtol(url, NULL, 0);

  mwlog(MWLOG_DEBUG, "Attempting to decode SYSV IPC key with atol. IPCKEY=0x%x errno=%d", 
	_mwaddress.sysvipckey, errno);
  
  if (errno != 0) {
    _mwaddress.sysvipckey = -1;
  } else {
    return MWSYSVIPC;
  };
  /* check for posix ipc **/

  /* assume ftok on config. */
  
  return -1;
};

static int url_decode (char * url)
{
  int len, urltype;

  if (url == NULL) return -EINVAL;
  
  urltype = url_header(url, &len);

  switch (urltype) {

  case MWSYSVIPC:
    return url_decode_sysvipc(url);
  default:
    return -EINVAL;
  }
  return 0;
};

/* the only exported function from this module. */
int _mwdecode_url(char * url)
{
  if (url == NULL) {    
    /* if url is not set, check MWURL or MWADDRESS */
    url = getenv ("MWURL");
    if (url == NULL) url = getenv ("MWADDRESS");
    
  } 

  if (url == NULL) {
    /* We assume IPC, and use UID as key */
    _mwaddress.sysvipckey = getuid();
    _mwaddress.protocol = MWSYSVIPC;
  } else {
    _mwaddress.protocol = url_decode (url);
    if (_mwaddress.protocol <= 0) {
      mwlog(MWLOG_ERROR, 
	    "unable to understand the URL \"%s\" for MidWay instance, error %d", 
	    url, _mwaddress.protocol);
      return -EINVAL;
    }
  }
};
