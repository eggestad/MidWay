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
 * Revision 1.1  2000/08/31 19:37:30  eggestad
 * This file is used for implementing SRB client side
 *
 *
 */

static struct timeval deadline = {0, 0};

mwaddress_t * _mwaddress;
/*
  fastpath flag. This spesify weither programs are passed a copy
  of the data passed thru shared memoery or a pointer to shared
  memory directly.
  (Does it need to be global??)
*/
int _mw_fastpath = 0;

/* the call handle, it is inc'ed everytime we need a new, and randomly
   assigned the first time.*/
int handle = -1;
static int nexthandle(void)
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
  if (rc == 0) return -EISCONN;;
  if (rc != 1) return rc;

  errno = 0;
  _mwaddress = _mwdecode_url(url);
  if (_mwaddress == NULL) {
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
  /* input sanyty checking, everywhere else we depend on params to be sane. */
  if ( (data == NULL ) || (datalen < 0) || (svcname == NULL) ) 
    return -EINVAL;
  
  mwlog(MWLOG_DEBUG1, "mwacall called for service %.32s", svcname);
  
  /* datalen may be zero if data is null terminated */
  if (datalen == 0) datalen = strlen(data);

  if (!_mwaddress) {
    mwlog(MWLOG_DEBUG1, "not attached");
    return -ENOTCONN;
  };

  handle = nexthandle();
  switch (_mwaddress->protocol) {
    
  case MWSYSVIPC:

    return _mwacall_ipc(handle, data, datalen, flags);
    
  case MWSRBP:
    return _mwacall_srb(handle, data, datalen, flags);
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
  int handle;
  /* input sanyty checking, everywhere else we depend on params to be sane. */
  if ( (cdata == NULL ) || (clen < 0) || (svcname == NULL) ) 
    return -EINVAL;
  if  ( !(flags & MWNOREPLY) && ((rlen == NULL) || (rdata == NULL)))
    return -EINVAL; 

  mwlog(MWLOG_DEBUG1, "mwcall called for service %.32s", svcname);

  /* clen may be zero if cdata is null terminated */
  if (clen == 0) clen = strlen(cdata);

  if (!_mwaddress) {
    mwlog(MWLOG_DEBUG1, "not attached");
    return -ENOTCONN;
  };
  handle = nexthandle();
  switch (_mwaddress->protocol) {
    
  case MWSYSVIPC:
    hdl = _mwacallipc (handle, svcname, cdata, clen, &deadline, flags);
    if (hdl < 0) return hdl;
    hdl = _mwfetchipc (handle, rdata, rlen, appreturncode, flags);
    return hdl;
    
  case MWSRBP:
    hdl = _mwacall_srb (handle, svcname, cdata, clen, &deadline, flags);
    if (hdl < 0) return hdl;
    hdl = _mwfetch_srb (handle, rdata, rlen, appreturncode, flags);
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

float _mw_leftOfDeadline()
{
  return deadline.tv_sec + (deadline.tv_usec / 1000000);
};

/**********************************************************************
 * Memory allocation 
 **********************************************************************/

/* for IPC we use shmalloc. For network we use std malloc */
void * mwalloc(int size) 
{
  if (size < 1) return NULL;
  if ( !_mwaddress || (_mwaddress->proto != MWSYSVIPC)) {
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
    return free(adr);
  } else {
    mwlog(MWLOG_DEBUG3, "mwfree: using _mwfree");
    return _mwfree( adr);
  };
};
 
