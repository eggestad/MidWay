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
 * Revision 1.2  2000/07/20 19:24:22  eggestad
 * CVS keywords were missing.
 *
 * Revision 1.1.1.1  2000/03/21 21:04:12  eggestad
 * Initial Release
 *
 * Revision 1.1.1.1  2000/01/16 23:20:12  terje
 * MidWay
 *
 */

static char * RCSId = "$Id$";
static char * RCSName = "$Name$"; /* CVS TAG */

#include <stdio.h>
#include <errno.h>

/*#include <sys/types.h>
#include <sys/ipc.h>
*/
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>

#include <math.h>

#include <MidWay.h>
#include <ipctables.h>
#include <ipcmessages.h>
#include <shmalloc.h>
#include <address.h>
/*
  Here we provide the "visible" library API, in the IPC only form.
  This is however limited to the client API, since the server API are 
  available in IPC only.
  
  This module is only linked in with ipc only lib.
  IPC Only in libmw.c we provide the top level API version 
  that do both IPC and TCP 

  !!!!!!  well we just have to see about that. 
  THere was a reason for this, but I think it was before I chose URI as adress format.

  Also see documentation on linking
  with IPC lib only, Network only, and with both libs.
*/

/*
  fastpath flag. This spesify weither programs are passed a copy
  of the data passed thru shared memoery or a pointer to shared
  memory directly.
  (Does it need to be global??)
*/
int _mw_fastpath = 0;
int _mw_attached = 0;

static struct timeval deadline = {0, 0};


int _mwattachipc(int type, char * name, int key)
{
  int rc;
  
  if (key <= 0) return -EINVAL;

  rc = _mw_attach_ipc(key, type);
  if (rc != 0) return rc;
  
  return _mw_ipcsend_attach(type, name, 0);
};

/* should it take a struct as arg??, bust as well not, even in a struct, all
   members must be init'ed.*/
int mwattach(char * url, char * name, 
	     char * username, char * password, int flags)
{
  FILE * proc;
  int ipckey, type;
  char buffer[256];
  int rc;

  /* if we're already connected .... */
  rc = _mwsystemstate();
  if (rc == 0) return -EISCONN;;
  if (rc != 1) return rc;

  errno = 0;
  rc = _mwdecode_url(url);
  if (_mwaddress.protocol == 0) {
    mwlog(MWLOG_ERROR, "The URL %s is invalid, decode returned %d", url, rc);
    return -EINVAL;
  };

  if (name == NULL) {
    /* if on linux this works, else open fails 
     * /proc/self/cmdline gives each cmdline param, \0 terminated.
     */
    proc = fopen("/proc/self/cmdline", "r");
    if (proc == NULL) name = "Anonymous";
    else {
      fgets(buffer, 255, proc);
      name = buffer;
    };
  };
    
  if ( (flags & MWSERVER) && (_mwaddress.protocol != MWSYSVIPC) ) {
    mwlog(MWLOG_ERROR, "Attempting to attach as a server on a protocol other than IPC.");
      return -EINVAL;
  };

  if (_mwaddress.protocol == MWSYSVIPC) {

    type = 0;
    if (flags & MWSERVER)       type |= MWIPCSERVER;
    if (!(flags & MWNOTCLIENT)) type |= MWIPCCLIENT;
    if ((type < 1) || (type > 3)) return -EINVAL;

    mwlog(MWLOG_DEBUG, "attaching to IPC name=%s, IPCKEY=0x%x type=%#x", 
	  name, _mwaddress.sysvipckey, type);
    rc = _mwattachipc(type, name, _mwaddress.sysvipckey);
    if (rc == 0) _mw_attached = 1;
    return rc;
  }

  return -EINVAL;
};

int mwdetach()
{
  int rc;

  ipcmaininfo * ipcmain;
  
  /* If we're not attached */
  ipcmain = _mw_ipcmaininfo(); 
  if (ipcmain == NULL) return 0;

  rc = _mwsystemstate();
  if (!rc) {
    errno = 0;
    rc = _mw_ipcsend_detach(0);
  } else {
    rc = _mw_ipcsend_detach(1);
  };
  _mw_detach_ipc();
  _mw_attached = 0;
  return 0;
};


/* This conclude the administrative calls.*/




/*****************************************************************
 * 
 * Here We have the API that makes a client able to talk to a server
 * Note that some of these are essetial for making a server talk to 
 * a calling client.
 * 
 * Thus a valid client can be cmplied without mwserver.c but you can't 
 * legally compile a server with out this module, even if it we 
 * attach as a server only.
 *
 *****************************************************************/



/* the now usuall fallback incase libmw.c are not before in the link path */
int mwfetch(int handle, char ** data, int * len, int * appreturncode, int flags) 
{
  int rc;

  if ( (data == NULL) || (len == NULL) || (appreturncode == NULL) )
    return -EINVAL;

  rc = _mwsystemstate();
  if (rc) return rc;

  return _mwfetchipc ( handle, data, len, appreturncode, flags);
}
/* the usuall IPC only API, except that mwcall() is implemeted
   entierly here. Reemeber to do the same in lwbmw.c and TCP imp.
*/
int mwacall(char * svcname, char * data, int datalen, int flags) 
{
  int rc;
  
  rc = _mwsystemstate();
  if (rc) return rc;

  /* input sanyty checking, everywhere else we depend on params to be sane. */
  if ( (data == NULL ) || (datalen < 0) || (svcname == NULL) ) 
    return -EINVAL;

  /* clen may be zero if data is null terminated */
  if (datalen == 0) datalen = strlen(data);
  
  return _mwacallipc (svcname, data, datalen, &deadline, flags);
}

int mwcall(char * svcname, 
	   char * cdata, int clen, 
	   char ** rdata, int * rlen, 
	   int * appreturncode, int flags)
{
  int hdl;
  mwsvcinfo * si;
  int rc;
  
  rc = _mwsystemstate();
  if (rc) return rc;

  /* input sanyty checking, everywhere else we depend on params to be sane. */
  if ( (cdata == NULL ) || (clen < 0) || (svcname == NULL) ) 
    return -EINVAL;
  if  ( !(flags & MWNOREPLY) && ((rlen == NULL) || (rdata == NULL)))
    return -EINVAL; 

  /* clen may be zero if data is null terminated */
  if (clen == 0) clen = strlen(cdata);

  hdl = _mwacallipc (svcname, cdata, clen, &deadline, flags);
  if (hdl < 0) return hdl;
  hdl = _mwfetchipc ( hdl, rdata, rlen, appreturncode, flags);
  return hdl;
};

/* TRANSACTIONAL API
   these calls are currently here only to set a deadline.
   (the MWNOTRAN flags is implied. Should we require it?)
*/
int mwbegin(float fsec, int flags)
{
  struct timeval now;
  float s, ss;
  
  errno = 0;
  if (fsec <= 0) return -EINVAL;

  gettimeofday(&now, NULL);
  s = fabs(fsec);
  ss = fsec - s;
  
  deadline.tv_sec = now.tv_sec + (int) s;
  deadline.tv_usec = now.tv_usec + (int) (ss * 1000000); /*micro secs*/
  return 0;
};

int mwcommit()
{
  deadline.tv_sec = 0;
  deadline.tv_usec = 0;
};

int mwabort()
{
  deadline.tv_sec = 0;
  deadline.tv_usec = 0;
};

/* for IPC we use shmalloc. For network we use std malloc */
void * mwalloc(int size) {
  return _mwalloc(size);
};
void * mwrealloc(void * adr, int newsize) {
  return _mwrealloc(adr, newsize);
};
int mwfree(void * adr) {
  return _mwfree( adr);
};
