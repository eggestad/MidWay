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
 * Revision 1.4  2001/05/12 18:00:31  eggestad
 * changes to multiple reply handling, MWMULTIPLE are no langer sent to server, replies are cat'ed in client
 *
 * Revision 1.3  2000/11/15 21:23:38  eggestad
 * fix for NULL as input data
 *
 * Revision 1.2  2000/09/21 18:39:36  eggestad
 * - issue of handle now moved here
 * - deadline handling now placed here
 * - lots of changes to deadline handling
 * - fastpath flag now private and added func to access it.
 *
 * Revision 1.1  2000/08/31 19:37:30  eggestad
 * This file is used for implementing SRB client side
 *
 *
 */


#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <math.h>

#include <MidWay.h>
#include <address.h>
#include <ipcmessages.h>
#include <shmalloc.h>

static struct timeval deadline = {0, 0};

int _mw_deadline(struct timeval * tv_deadline, float * ms_deadlineleft)
{
  struct timeval now;
  float timeleft;

  if (deadline.tv_sec == 0) return 0;
  gettimeofday(&now, NULL);

  if (ms_deadlineleft != NULL) {
    timeleft = now.tv_sec;
    timeleft -= deadline.tv_sec;
    timeleft += now.tv_usec / 1000000;
    timeleft -= deadline.tv_usec / 1000000;
    *ms_deadlineleft = timeleft;
  };
  
  if (tv_deadline != NULL) {
    tv_deadline->tv_sec = deadline.tv_sec;
    tv_deadline->tv_usec = deadline.tv_usec; 
  };
  
  return 0 ;
};

mwaddress_t * _mwaddress;

int _mw_isattached(void)
{
  if (_mwaddress == NULL) return 0;
  return 1;
};

/*
  fastpath flag. This spesify weither programs are passed a copy
  of the data passed thru shared memoery or a pointer to shared
  memory directly.
  (Does it need to be global??)
*/
static int _mw_fastpath = 0;

int _mw_fastpath_enabled(void) 
{
  return _mw_fastpath;
};

/* the call handle, it is inc'ed everytime we need a new, and randomly
   assigned the first time.*/
int handle = -1;
int _mw_nexthandle(void)
{
  if (handle > 0) handle++;

  /* test for overflow (or init) */
  if (handle < 1) {
    while(handle < 1) {
      srand(time(NULL));
      handle = rand() + 1;
    }; 
  }; 
  return handle;
};


/* should it take a struct as arg??, may as well not, even in a struct, all
   members must be init'ed.*/
int mwattach(char * url, char * name, 
	     char * username, char * password, int flags)
{
  FILE * proc;
  int ipckey, type;
  char buffer[256];
  int rc;

  /* if we're already connected .... */
  if (_mwaddress != NULL) 
    return -EISCONN;;

  rc = _mwsystemstate();
  mwlog(MWLOG_DEBUG3, "_mwsystemstate returned %d", rc);
  if (rc == 0) return -EISCONN;;
  if (rc != 1) return rc;

  errno = 0;
  _mwaddress = _mwdecode_url(url);
  if (_mwaddress == NULL) {
    mwlog(MWLOG_ERROR, "The URL %s is invalid, decode returned %d", url, errno);
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
    
  if ( (flags & MWSERVER) && (_mwaddress->protocol != MWSYSVIPC) ) {
    mwlog(MWLOG_ERROR, "Attempting to attach as a server on a protocol other than IPC.");
      return -EINVAL;
  };

  /* protocol is IPC */
  if (_mwaddress->protocol == MWSYSVIPC) {

    type = 0;
    if (flags & MWSERVER)       type |= MWIPCSERVER;
    if (!(flags & MWNOTCLIENT)) type |= MWIPCCLIENT;
    if ((type < 1) || (type > 3)) return -EINVAL;

    mwlog(MWLOG_DEBUG, "attaching to IPC name=%s, IPCKEY=0x%x type=%#x", 
	  name, _mwaddress->sysvipckey, type);
    rc = _mwattachipc(type, name, _mwaddress->sysvipckey);
    if (rc != 0) {
      mwlog(MWLOG_DEBUG, "attaching to IPC failed with rc=%d",rc);
      free(_mwaddress);
      _mwaddress = NULL;
    };
    return rc;
  }

  /* protocol is SRB */
  if (_mwaddress->protocol == MWSRBP) {
    return _mwattach_srb(_mwaddress, name, username, password, flags);
  };
  free(_mwaddress);
  _mwaddress = NULL;
  return -EINVAL;
};

int mwdetach()
{
  int rc, proto;

  mwlog(MWLOG_DEBUG1, "mwdetach: %s", _mwaddress?"detaching":"not attached");

  /* If we're not attached */  
  if (_mwaddress == NULL) return 0;

  proto = _mwaddress->protocol;
  free(_mwaddress);
  _mwaddress = NULL;
 
  switch (proto) {
    
  case MWSYSVIPC:

    return _mwdetachipc();
    
  case MWSRBP:
    return _mwdetach_srb();
  };
  mwlog(MWLOG_ERROR, "mwdetach: This can't happen unknown protocol %d", proto);

};

int mwfetch(int handle, char ** data, int * len, int * appreturncode, int flags) 
{
  int rc;

  if ( (data == NULL) || (len == NULL) || (appreturncode == NULL) )
    return -EINVAL;

  mwlog(MWLOG_DEBUG1, "mwfetch called handle = %#x", handle);

  if (!_mwaddress) {
    mwlog(MWLOG_DEBUG1, "not attached");
    return -ENOTCONN;
  };
     
  switch (_mwaddress->protocol) {
    
  case MWSYSVIPC:

    return _mwfetch_ipc(handle, data, len, appreturncode, flags);
    
  case MWSRBP:
    return _mwfetch_srb(handle, data, len, appreturncode, flags);
  };
  mwlog(MWLOG_ERROR, "mwfetch: This can't happen unknown protocol %d", 
	_mwaddress->protocol);

  return -EFAULT;
};

int mwacall(char * svcname, char * data, int datalen, int flags) 
{
  int handle;
  float timeleft;

  /* input sanyty checking, everywhere else we depend on params to be sane. */
  if ( (datalen < 0) || (svcname == NULL) ) 
    return -EINVAL;
  
  mwlog(MWLOG_DEBUG1, "mwacall called for service %.32s", svcname);

  if ( _mw_deadline(NULL, &timeleft)) {
    /* we should maybe say that a few ms before a deadline, we call it expired? */
    if (timeleft <= 0.0) {
      /* we've already timed out */
      mwlog(MWLOG_DEBUG1, "call to %s was made %d ms after deadline had expired", 
	    timeleft, svcname);
      return -ETIME;
    };
  };

  /* datalen may be zero if data is null terminated */
  if ( (data != NULL ) && (datalen == 0) ) datalen = strlen(data);

  if (!_mwaddress) {
    mwlog(MWLOG_DEBUG1, "not attached");
    return -ENOTCONN;
  };

  handle = _mw_nexthandle();
  switch (_mwaddress->protocol) {
    
  case MWSYSVIPC:

    return _mwacall_ipc(svcname, data, datalen, flags);
    
  case MWSRBP:
    return _mwacall_srb(svcname, data, datalen, flags);
  };
  mwlog(MWLOG_ERROR, "mwacall: This can't happen unknown protocol %d", 
	_mwaddress->protocol);

  return -EFAULT;
};

int mwcall(char * svcname, 
	       char * cdata, int clen, 
	       char ** rdata, int * rlen, 
	       int * appreturncode, int flags)
{
  int hdl;
  float timeleft;

  /* input sanyty checking, everywhere else we depend on params to be sane. */
  if ( (clen < 0) || (svcname == NULL) ) 
    return -EINVAL;
  if  ( !(flags & MWNOREPLY) && ((rlen == NULL) || (rdata == NULL)))
    return -EINVAL; 

  mwlog(MWLOG_DEBUG1, "mwcall called for service %.32s", svcname);

  if ( _mw_deadline(NULL, &timeleft)) {
    /* we should maybe say that a few ms before a deadline, we call it expired? */
    if (timeleft <= 0.0) {
      /* we've already timed out */
      mwlog(MWLOG_DEBUG1, "call to %s was made %d ms after deadline had expired", 
	    timeleft, svcname);
      return -ETIME;
    };
  };

  /* clen may be zero if cdata is null terminated */
  if ( (cdata != NULL) && (clen == 0) ) clen = strlen(cdata);

  if (!_mwaddress) {
    mwlog(MWLOG_DEBUG1, "not attached");
    return -ENOTCONN;
  };
  
  switch (_mwaddress->protocol) {
    
  case MWSYSVIPC:
    mwlog(MWLOG_DEBUG1, "mwcall using  SYSVIPC");
    hdl = _mwacall_ipc (svcname, cdata, clen, flags);
    if (hdl < 0) return hdl;
    hdl = _mwfetch_ipc (hdl, rdata, rlen, appreturncode, flags);
    mwlog(MWLOG_DEBUG1, "mwcall(): _mwfetch_ipc() returned %d", hdl);
    return hdl;
    
  case MWSRBP:
    mwlog(MWLOG_DEBUG1, "mwcall using  SRBP");
    hdl = _mwacall_srb (svcname, cdata, clen, flags);
    if (hdl < 0) return hdl;
    hdl = _mwfetch_srb (hdl, rdata, rlen, appreturncode, flags);
    return hdl;

  };
  mwlog(MWLOG_ERROR, "mwcall: This can't happen unknown protocol %d", 
	_mwaddress->protocol);

  return -EFAULT;
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
  mwlog(MWLOG_DEBUG1, "mwbegin called timeout = %f", fsec);
  if (fsec <= 0) return -EINVAL;

  gettimeofday(&now, NULL);
  s = fabs(fsec);
  ss = fsec - s;
  
  deadline.tv_sec = now.tv_sec + (int) s;
  deadline.tv_usec = now.tv_usec + (int) (ss * 1000000); /*micro secs*/
  mwlog(MWLOG_DEBUG3, "mwbegin deadline is at %d.%d",
	deadline.tv_sec, deadline.tv_usec); 
  return 0;
};

int mwcommit()
{
  mwlog(MWLOG_DEBUG1, "mwcommit:");

  deadline.tv_sec = 0;
  deadline.tv_usec = 0;
};

int mwabort()
{
  mwlog(MWLOG_DEBUG1, "mwabort:");

  deadline.tv_sec = 0;
  deadline.tv_usec = 0;
};

/**********************************************************************
 * Memory allocation 
 **********************************************************************/

/* for IPC we use shmalloc. For network we use std malloc */
void * mwalloc(int size) 
{
  if (size < 1) return NULL;
  if ( !_mwaddress || (_mwaddress->protocol != MWSYSVIPC)) {
    mwlog(MWLOG_DEBUG3, "mwalloc: using malloc");
    return malloc(size);
  } else {
    mwlog(MWLOG_DEBUG3, "mwalloc: using _mwalloc");
    return _mwalloc(size);
  };
};

void * mwrealloc(void * adr, int newsize) {

  if (_mwshmcheck(adr) == -1) {
    mwlog(MWLOG_DEBUG3, "mwrealloc: using realloc");
    return realloc(adr, newsize);
  } else {
    mwlog(MWLOG_DEBUG3, "mwrealloc: using _mwrealloc");
    return _mwrealloc(adr, newsize);
  };
};


int mwfree(void * adr) 
{
  if (_mwshmcheck(adr) == -1) {
    mwlog(MWLOG_DEBUG3, "mwfree: using free");
    free(adr);
    return 0;
  } else {
    mwlog(MWLOG_DEBUG3, "mwfree: using _mwfree");
    return _mwfree( adr);
  };
};
 
