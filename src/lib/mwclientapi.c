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
 * Revision 1.8  2002/07/07 22:35:20  eggestad
 * *** empty log message ***
 *
 * Revision 1.7  2002/02/17 14:11:45  eggestad
 * MWMORE is no longer a flag
 *
 * Revision 1.6  2001/10/03 22:46:51  eggestad
 * Added timeout errno for attach
 *
 * Revision 1.5  2001/08/29 20:38:47  eggestad
 * added RCS keys
 *
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
#include <ipctables.h>
#include <shmalloc.h>
#include <mwclientipcapi.h>
#include <SRBclient.h>

static char * RCSId UNUSED = "$Id$";

static struct timeval deadline = {0, 0};

static mwaddress_t * _mwaddress;


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

int _mw_isattached(void)
{
  if (_mwaddress == NULL) return 0;
  return 1;
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
  DEBUG3("_mwsystemstate returned %d", rc);
  if (rc == 0) return -EISCONN;;
  if (rc != -ENAVAIL) return rc;

  if (url == NULL) {
    url = getenv ("MWADDRESS");
  };

  errno = 0;
  _mwaddress = _mwdecode_url(url);
  if (_mwaddress == NULL) {
    if (errno == ETIME) {
      return -EHOSTDOWN;
    };
    Error("The URL %s is invalid, decode returned %d", url, errno);
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
    Error("Attempting to attach as a server on a protocol other than IPC.");
      return -EINVAL;
  };

  /* protocol is IPC */
  if (_mwaddress->protocol == MWSYSVIPC) {

    type = 0;
    if (flags & MWSERVER)       type |= MWIPCSERVER;
    if (!(flags & MWNOTCLIENT)) type |= MWIPCCLIENT;
    if ((type < 1) || (type > 3)) return -EINVAL;

    DEBUG("attaching to IPC name=%s, IPCKEY=0x%x type=%#x", 
	  name, _mwaddress->sysvipckey, type);
    rc = _mwattachipc(type, name, _mwaddress->sysvipckey);
    if (rc != 0) {
      DEBUG("attaching to IPC failed with rc=%d",rc);
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
  int proto;

  DEBUG1("mwdetach: %s", _mwaddress?"detaching":"not attached");

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
  Error("mwdetach: This can't happen unknown protocol %d", proto);

};

int mwfetch(int handle, char ** data, int * len, int * appreturncode, int flags) 
{
  int rc;

  if ( (data == NULL) || (len == NULL) || (appreturncode == NULL) )
    return -EINVAL;

  DEBUG1("mwfetch called handle = %#x", handle);

  if (!_mwaddress) {
    DEBUG1("not attached");
    return -ENOTCONN;
  };

  /* if client didn't set MWMULTIPLE, the client is expecting one and
     only one reply, we must concat multiiple replies */
  if (! (flags & MWMULTIPLE)) {
    char * pdata = NULL;
    int pdatalen = 0;
    int first = 1;
 
    * len = 0;

    do {
      switch (_mwaddress->protocol) {
	
      case MWSYSVIPC:
	
	rc = _mwfetchipc(handle, &pdata, &pdatalen, appreturncode, flags);
	break;

      case MWSRBP:
	rc = _mwfetch_srb(handle, &pdata, &pdatalen, appreturncode, flags);
	break;

      default:
	Error("mwfetch: This can't happen unknown protocol %d", 
	      _mwaddress->protocol);
	return -EINVAL;
      };
      DEBUG1("protocol level fetch returned %d datalen = %d", rc, pdatalen);

      /* if error */
      if (rc == MWFAIL) {
	return rc;
      };

      /* shortcut, iff one reply there is no copy needed. */
      if ( (rc != MWMORE) && (first) ) {
	*data = pdata;
	*len = pdatalen;	
	return rc;
      };
      
      first = 0;
      
      *data = realloc(*data, *len + pdatalen + 1);
      DEBUG1("Adding reply: *data = %p appending at %p old len = %d new len = %d",
	    *data, (*data)+(*len), *len, pdatalen);
      memcpy((*data)+(*len), pdata, pdatalen);
      *len += pdatalen;
      
    }  while(rc == MWMORE);

    (*data)[*len]  = '\0';
    return rc;
  };
      
  /* multiple replies handeled in user code, we return the first reply */
  switch (_mwaddress->protocol) {
      
  case MWSYSVIPC:
    
    rc = _mwfetchipc(handle, data, len, appreturncode, flags);
    return rc; 
    
  case MWSRBP:
    rc = _mwfetch_srb(handle, data, len, appreturncode, flags);
    return rc;
  };
  Error("mwfetch: This can't happen unknown protocol %d", 
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
  
  DEBUG1("mwacall called for service %.32s", svcname);

  if ( _mw_deadline(NULL, &timeleft)) {
    /* we should maybe say that a few ms before a deadline, we call it expired? */
    if (timeleft <= 0.0) {
      /* we've already timed out */
      DEBUG1("call to %s was made %d ms after deadline had expired", 
	    timeleft, svcname);
      return -ETIME;
    };
  };

  /* datalen may be zero if data is null terminated */
  if ( (data != NULL ) && (datalen == 0) ) datalen = strlen(data);

  if (!_mwaddress) {
    DEBUG1("not attached");
    return -ENOTCONN;
  };

  handle = _mw_nexthandle();
  switch (_mwaddress->protocol) {
    
  case MWSYSVIPC:

    return _mwacall_ipc(svcname, data, datalen, flags);
    
  case MWSRBP:
    return _mwacall_srb(svcname, data, datalen, flags);
  };
  Error("mwacall: This can't happen unknown protocol %d", 
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
  if  ( (flags & MWMULTIPLE) )
    return -EINVAL; 

  DEBUG1("mwcall called for service %.32s", svcname);

  hdl = mwacall(svcname, cdata, clen, flags);
  DEBUG1("mwcall(): mwacall() returned handle %d", hdl);
  return mwfetch (hdl, rdata, rlen, appreturncode, flags);

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
  DEBUG1("mwbegin called timeout = %f", fsec);
  if (fsec <= 0) return -EINVAL;

  gettimeofday(&now, NULL);
  s = fabs(fsec);
  ss = fsec - s;
  
  deadline.tv_sec = now.tv_sec + (int) s;
  deadline.tv_usec = now.tv_usec + (int) (ss * 1000000); /*micro secs*/
  DEBUG3("mwbegin deadline is at %d.%d",
	deadline.tv_sec, deadline.tv_usec); 
  return 0;
};

int mwcommit()
{
  DEBUG1("mwcommit:");

  deadline.tv_sec = 0;
  deadline.tv_usec = 0;
};

int mwabort()
{
  DEBUG1("mwabort:");

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
    DEBUG3("mwalloc: using malloc");
    return malloc(size);
  } else {
    DEBUG3("mwalloc: using _mwalloc");
    return _mwalloc(size);
  };
};

void * mwrealloc(void * adr, int newsize) {

  if (_mwshmcheck(adr) == -1) {
    DEBUG3("mwrealloc: using realloc");
    return realloc(adr, newsize);
  } else {
    DEBUG3("mwrealloc: using _mwrealloc");
    return _mwrealloc(adr, newsize);
  };
};


int mwfree(void * adr) 
{
  if (_mwshmcheck(adr) == -1) {
    DEBUG3("mwfree: using free");
    free(adr);
    return 0;
  } else {
    DEBUG3("mwfree: using _mwfree");
    return _mwfree( adr);
  };
};
 
