
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
 * Revision 1.20  2004/04/12 23:05:24  eggestad
 * debug format fixes (wrong format string and missing args)
 *
 * Revision 1.19  2004/04/08 10:34:06  eggestad
 * introduced a struct with pointers to the functions implementing the midway functions
 * for a given protocol.
 * This is in preparation for be able to do configure with/without spesific protocol.
 * This creates a new internal API each protocol must addhere to.
 *
 * Revision 1.18  2003/12/11 14:18:03  eggestad
 * added mwlistsvc for IPC
 *
 * Revision 1.17  2003/06/12 07:43:21  eggestad
 * added MWIPCONLY flag to _mwipcacall, to force local service
 *
 * Revision 1.16  2003/03/26 01:59:33  cstup
 * Another fix to the same DEBUG line.
 *
 * Revision 1.15  2003/03/25 02:29:52  cstup
 * Fixed DEBUG statement in _mw_attach_ipc()
 *
 * Revision 1.14  2002/11/18 00:11:38  eggestad
 * - _mw_ipcmaininfo prtotype fixup
 *
 * Revision 1.13  2002/10/07 00:04:40  eggestad
 * - _mw_get_server_by_serviceid() named _mw_get_provider_by_serviceid() that also retun gateways.
 * - _mw_get_service_providers() obsoleted
 * - _mw_get_services_byname() now can return the number(n) in the returned list,
 *
 * Revision 1.12  2002/10/03 21:09:08  eggestad
 * - _mw_get_service_providers() never worked, eternal loop
 * - _mw_get_services_byname() had rand() on the wrong spot,
 *   table was randomly traversed
 *
 * Revision 1.11  2002/09/29 17:37:54  eggestad
 * improved the _mw_get[client|server|service|gateway]entry functions and removed duplicates in mwd.c
 *
 * Revision 1.10  2002/09/22 22:49:23  eggestad
 * Added new function that return list of all providers of a service
 *
 * Revision 1.9  2002/09/04 07:13:31  eggestad
 * mwd now sends an event on service (un)provide
 *
 * Revision 1.8  2002/08/09 20:50:15  eggestad
 * A Major update for implemetation of events and Task API
 *
 * Revision 1.7  2002/07/07 22:35:20  eggestad
 * *** empty log message ***
 *
 * Revision 1.6  2002/02/17 14:09:58  eggestad
 * clean compile fix
 *
 * Revision 1.5  2001/09/15 23:59:05  eggestad
 * Proper includes and other clean compile fixes
 *
 * Revision 1.4  2000/09/21 18:36:36  eggestad
 * server crashed if IPC client dissapeared while service call was prosessed.
 *
 * Revision 1.3  2000/08/31 21:50:48  eggestad
 * DEBUG level set propper.
 *
 * Revision 1.2  2000/07/20 19:23:40  eggestad
 * Changes needed for SRB clients and gateways.
 *
 * Revision 1.1.1.1  2000/03/21 21:04:11  eggestad
 * Initial Release
 *
 * Revision 1.1.1.1  2000/01/16 23:20:12  terje
 * MidWay
 *
 */

#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/time.h>
#include <signal.h>
#include <fnmatch.h>

#include <MidWay.h>
#include <ipctables.h>
#include <ipcmessages.h>

static char * RCSId UNUSED = "$Id$";
static char * RCSName UNUSED = "$Name$"; /* CVS TAG */


static ipcmaininfo  * ipcmain = NULL; 
static cliententry  * clttbl  = NULL;
static serverentry  * srvtbl  = NULL;
static serviceentry * svctbl  = NULL;
static gatewayentry * gwtbl   = NULL;
static conv_entry   * convtbl = NULL;

static int my_mqid = -1;

static SERVERID  myserverid  = UNASSIGNED;
static CLIENTID  myclientid  = UNASSIGNED;
static GATEWAYID mygatewayid = UNASSIGNED;


/* I don't want tp make ipcmain a global var, and _mw_attach_ipc are
   never called in mwd, thus this function are for mwd only */

void _mw_set_shmadr (ipcmaininfo * im, cliententry * clt, serverentry * srv, 
		     serviceentry * svc, gatewayentry * gw, conv_entry * conv)
{
  ipcmain = im ;
  clttbl  = clt;  
  srvtbl  = srv;
  svctbl  = svc;
  gwtbl   = gw;
  convtbl = conv;
};

int _mw_attach_ipc(key_t key, int type)
{
  int mainid, readonly;

  extern struct segmenthdr * _mwHeapInfo;
  /* already connected,  */
  if (ipcmain != NULL) {
    Warning(	  "Attempted to attach shm segments while already attached");
    return -EISCONN; 
  };

  /* if we're a clients we shall attach all the shared memory tables readonly */
  if (type & MWIPCSERVER)  readonly = 0;
  else readonly = SHM_RDONLY;

  DEBUG1("_mw_attachipc(key=%d, type=%s(%d)) readonly=%d", 
	key, type==MWIPCSERVER?"SERVER":"CLIENT", type, readonly);

  /* THREAD MUTEX BEGIN */
  /* NOTE: below the returns in the if(error) do not release the MUTEX */


  mainid = shmget(key, 0, 0);
  if (mainid == -1) {
    Error("There are no shared memory for key = 0x%x reason %s",
	  key, strerror(errno));
    return -errno;
  };
  DEBUG1("main shm info table has id %d",mainid);

  ipcmain = (ipcmaininfo *) shmat(mainid, (void *) 0, readonly);
  if ( (ipcmain == NULL) || (ipcmain == (void *) -1) ) {
    Error("Failed to attach shared memory with id = %d reason %s",
	  mainid, strerror(errno));
    ipcmain = NULL;
    /* MUTEX END */
    return -errno;
  };
  DEBUG1("main shm info table attached at %p",ipcmain);


  /* attaching all the other tables */

  clttbl = shmat(ipcmain->clttbl_ipcid, NULL, readonly);
  if (clttbl == (void *) -1) {
    Error("Failed to attach client table with id = %d reason %s",
	  ipcmain->clttbl_ipcid, strerror(errno));
    return -errno;
  };
  DEBUG1("client table attached at %p",clttbl);
  
  srvtbl = shmat(ipcmain->srvtbl_ipcid, NULL, readonly);
  if (srvtbl == (void *) -1) {
    Error("Failed to attach Server table with id = %d reason %s",
	  ipcmain->srvtbl_ipcid, strerror(errno));
    return -errno;
  };
  DEBUG1("Server table attached at %p",srvtbl);
  
  svctbl = shmat(ipcmain->svctbl_ipcid, NULL, readonly);
  if (svctbl == (void *) -1) {
    Error("Failed to attach service table with id = %d reason %s",
	  ipcmain->svctbl_ipcid, strerror(errno));
    return -errno;
  };
  DEBUG1("service table attached at %p",svctbl);
  
  gwtbl = shmat(ipcmain->gwtbl_ipcid, NULL, readonly);
  if (gwtbl == (void *) -1) {
    Error("Failed to attach gateway table with id = %d reason %s",
	  ipcmain->gwtbl_ipcid, strerror(errno));
    return -errno;
  };
  DEBUG1("gateway table attached at %p",gwtbl);
  
  convtbl = shmat(ipcmain->convtbl_ipcid, NULL, readonly);
  if (convtbl == (void *) -1) {
    Error("Failed to attach convserver table with id = %d reason %s",
	  ipcmain->convtbl_ipcid, strerror(errno));
    return -errno;
  };
  DEBUG1("convserver table attached at %p",convtbl);

  my_mqid = msgget(IPC_PRIVATE, 0622); /* perm = rw--w--w- */ 
  if (my_mqid == -1) {
    Error("Failed to create an ipc queue for this process reason=%d",
	  errno);
    return -errno;
  };
  DEBUG1("My request/reply queue has IPC id %d", my_mqid );

  /* Note flag, the heap is never readonly. */
  _mwHeapInfo = shmat (ipcmain->heap_ipcid, NULL, 0);
  if (_mwHeapInfo != NULL) {
    DEBUG1("heap attached");
  };
  /* THREAD MUTEX ENDS */
  return 0;
};

#include <stdio.h>

void
_mw_detach_ipc(void)
{
  extern struct segmenthdr * _mwHeapInfo;
  
  shmdt(clttbl);
  shmdt(srvtbl);
  shmdt(svctbl);
  shmdt(gwtbl);
  shmdt(convtbl);

  shmdt(_mwHeapInfo);
  _mwHeapInfo = NULL;
  
  shmdt(ipcmain);
  ipcmain = NULL;
  myserverid = UNASSIGNED;
  myclientid= UNASSIGNED;

  msgctl(my_mqid, IPC_RMID, NULL);
  my_mqid = UNASSIGNED;
};
  
int _mw_my_mqid()
{
  return my_mqid;
};

int _mw_mwd_mqid()
{
  if (ipcmain == NULL) return -EILSEQ;
  DEBUG1("lookup of mwd msgqid gave %d from ipcmain at %p", 
	ipcmain->mwd_mqid, ipcmain);
  return ipcmain->mwd_mqid;
};

/* functions dealing with ID's */

void _mw_set_my_serverid(SERVERID sid)
{
  myserverid = MWSERVERMASK | sid;
  return ;
};

void _mw_set_my_clientid(CLIENTID cid)
{
  myclientid = MWCLIENTMASK | cid;
  return ;
};

void _mw_set_my_gatewayid(GATEWAYID gwid)
{
  mygatewayid = MWGATEWAYMASK | gwid;
  return;
};

SERVERID _mw_get_my_serverid()
{
  return   myserverid ;
};

CLIENTID _mw_get_my_clientid()
{
  return myclientid;
};

GATEWAYID _mw_get_my_gatewayid()
{
  return mygatewayid;
};

MWID _mw_get_my_mwid(void)
{
  if (myclientid != UNASSIGNED) return myclientid;
  if (myserverid != UNASSIGNED) return myserverid;
  if (mygatewayid != UNASSIGNED) return mygatewayid;
  return UNASSIGNED;
}

/* functions dealing with shm info */

ipcmaininfo * _mw_ipcmaininfo(void)
{
  return ipcmain;
};

cliententry * _mw_getcliententry(int i)
{
  if (clttbl == NULL) return NULL;
  if (i != 0) {
    i =  CLTID2IDX(i);
    if (i == UNASSIGNED) return NULL;
    if (i > ipcmain->clttbl_length) return NULL;
  };
  return & clttbl[i];
};

serverentry * _mw_getserverentry(int i)
{
  if (srvtbl == NULL) return NULL;
  if (i != 0) {
    i =  SRVID2IDX(i);
    if (i == UNASSIGNED) return NULL;
    if (i > ipcmain->srvtbl_length) return NULL;
  };
  return & srvtbl[i];
};

serviceentry * _mw_getserviceentry(int i)
{
  if (svctbl == NULL) return NULL;
  if (i != 0) {
    i =  SVCID2IDX(i);
    if (i == UNASSIGNED) return NULL;
    if (i > ipcmain->svctbl_length) return NULL;
  };
  return & svctbl[i];
};

gatewayentry * _mw_getgatewayentry(int i)
{
  if (gwtbl == NULL) return NULL;
  if (i != 0) {
    i =  GWID2IDX(i);
    if (i == UNASSIGNED) return NULL;
    if (i > ipcmain->gwtbl_length) return NULL;
  };
  return & gwtbl[i];
};

#ifdef CONV
conv_entry * _mw_getconv_entry(int i)
{
  if (convtbl == NULL) return NULL;
  i &= MWINDEXMASK;
  if ( (i >= 0) && (i <= ipcmain->convtbl_ipcid) )
    return & convtbl[i];
  else 
    return NULL;
};
#endif

/*
  Lookup functions to shmtables for info about other members 
  in the MidWay system.
*/
cliententry  * _mw_get_client_byid (CLIENTID cltid)
{
  int index;
  cliententry * tblent; 

  if (ipcmain == NULL) { 
    return NULL;
  };
  if (cltid & MWCLIENTMASK != MWCLIENTMASK) {
    return NULL;
  };

  index = cltid & MWINDEXMASK;
  if (index >= ipcmain->clttbl_length) 
    return NULL;
  
  tblent = & clttbl[index];
  if (tblent->status == UNASSIGNED) {
    return 0;
  };
  return tblent;
};

serverentry  * _mw_get_server_byid (SERVERID srvid)
{
  int index;
  serverentry * srvent; 

  if (ipcmain == NULL) { 
    DEBUG1("_mw_get_server_byid called while ipcmain == NULL");
    return NULL;
  };
  if (srvid & MWSERVERMASK != MWSERVERMASK) {
    DEBUG1("_mw_get_server_byid srvid & MWSERVERMASK %d != %d", srvid & MWSERVERMASK, MWSERVERMASK);
    return NULL;
  };
  index = srvid & MWINDEXMASK;
  if (index >= ipcmain->srvtbl_length) {
    DEBUG1("_mw_get_server_byid index to big %d > %d", index, ipcmain->srvtbl_length);
    return NULL;
  }
  
  srvent = & srvtbl[index];
  if (srvent->status == UNASSIGNED) {
    DEBUG1("_mw_get_server_byid serverid is not in use");
    return NULL;
  };
  return srvent;
};

serviceentry * _mw_get_service_byid (SERVICEID svcid)
{
  int index;
  serviceentry * svcent; 

  if (ipcmain == NULL) { 
    return NULL;
  };
  if (SVCID(svcid) == UNASSIGNED) {
    return NULL;
  };
  index = SVCID2IDX(svcid);
  if (index >= ipcmain->svctbl_length) 
    return NULL;
  
  svcent = & svctbl[index];
  if (svcent->server == UNASSIGNED) {
    return 0;
  };
  return svcent;
};

MWID _mw_get_provider_by_serviceid (SERVICEID svcid)
{
  int index;

  if (ipcmain == NULL) { 
    return UNASSIGNED;
  };
  if (SVCID(svcid) == UNASSIGNED) {
    return UNASSIGNED;
  };
  index = SVCID2IDX(svcid);
  if (index >= ipcmain->svctbl_length) 
    return UNASSIGNED;
  
  if (svctbl[index].server >= 0) return svctbl[index].server;
  if (svctbl[index].gateway >= 0) return svctbl[index].gateway;
  return UNASSIGNED;
};

/* return a list of all MWID's (only gateways and servers that provide
   the given service */

#ifdef OBSOLETE
MWID * _mw_get_service_providers(char * svcname, int convflag)
{
  SERVICEID * slist;
  int index = 0, n;
  MWID  llist[ipcmain->svctbl_length+1];
  MWID  plist[ipcmain->svctbl_length+1]; 
  MWID  flist[ipcmain->svctbl_length+1];
  MWID * rlist, * rins; 

  if (ipcmain == NULL) { 
    return NULL;
  };

  slist = _mw_get_services_byname(svcname, convflag);
  if (slist == NULL) return NULL;

  list = malloc(sizeof(MWID) * (ipcmain->svctbl_length+1)); 
  list = malloc(sizeof(MWID) * (ipcmain->svctbl_length+1)); 
  list = malloc(sizeof(MWID) * (ipcmain->svctbl_length+1)); 
 
  for (index = 0; index < ipcmain->svctbl_length+1; index++) 
    rlist[index] = UNASSIGNED;

  for(index = 0; slist[index] != UNASSIGNED; index++) {
    
    switch(svctbl[index].location) {

    case GWLOCAL:
      llist[nl] = svctbl[index].server;
      nl++;
      break;

      // we should sort these on cost, but... 
    case GWPEER:
      plist[np] = svctbl[index].gateway;
      np++;
      break;

    case GWREMOTE:
      flist[nf] = svctbl[index].gateway;
      nf++;
      break;
    };
  };

  free(slist);

  if (nl + np + nf == 0) return NULL;
  
  rlist = malloc(sizeof(MWID) * (nl + np + nf+1)); 
  rins = rlist;

  DEBUG1("service %s is provided by %d local servers %d peers, %d foreign", 
	svcname, nl, np, nf);

  for (i = 0; i < nl; i++) {
    DEBUG1(" server %d", SRVID2IDX(llist[i]));
    *(rins++) = llist[i];
  };
  for (i = 0; i < np; i++) {
    DEBUG1(" peer %d", GWID2IDX(plist[i]));
    *(rins++) = plist[i];
  };
  for (i = 0; i < nf; i++) {
    DEBUG1(" foreign %d", GWID2IDX(plist[i]));
    *(rins++) = flist[i];
  };

  *rins = UNASSIGNED;
  
  return rlist;
};
#endif

/* return the list of sericeid's of the given service */
SERVICEID * _mw_get_services_byname (char * svcname, int * N, int flags)
{
  SERVICEID * slist;
  int type, i, index, n = 0, x;
  int convflag = flags & MWCONV;
  int ipc_only_flag = flags & MWIPCONLY;

  DEBUG3("getting the list of services that provide %s conv=%d ipconly=%d", 
	 svcname, convflag, ipc_only_flag);
  if (N != NULL) *N = 0;

  if (ipcmain == NULL) { 
    return NULL;
  };

  slist = malloc(sizeof(MWID) * (ipcmain->svctbl_length+1)); 

  if (convflag)
    type = MWCONVSVC;
  else 
    type = MWCALLSVC;

  DEBUG3("clearing the slist");
  for (index = 0; index < ipcmain->svctbl_length+1; index++) 
    slist[index] = UNASSIGNED;

  x = 1+(rand() % ipcmain->svctbl_length);
  DEBUG3("we start at the random entry in the svctbl %d", x);

  for (i = 0; i < ipcmain->svctbl_length; i++) {

    /* we begin in  a random place in the table, in  order not to give
       the first service first in the list every time */
    index = (x + i) % ipcmain->svctbl_length;

    if (svctbl[index].type == UNASSIGNED) continue;
    if (svctbl[index].type != type) continue;

    DEBUG3("checking index %d service %s type = %d", 
	   index, svctbl[index].servicename, svctbl[index].type);

    /* for GW, to make sure we're not routing to a imported service */
    if (ipc_only_flag) 
       if (svctbl[index].location != GWLOCAL) continue;

    if (strncmp(svctbl[index].servicename, svcname, MWMAXSVCNAME) == 0) {
      DEBUG3("adding svcentry with index %d n = %d", index, n);
      slist[n++] = IDX2SVCID(index);
    };
  };
    
  if (N != NULL) *N = n;

  if (n != 0) return slist;
  
  free(slist);
  return NULL; 
};

/* return the ulist of unique service names that matches glob */

/* plist is set t to point at a char **, and the list is allocated
   with a single malloc */
int _mw_list_services_byglob (char * glob, char *** plist, int inflags)
{
   int * namelens;
   char ** list, ** rlist;
   char * svcname, * tmp;
   int i, index, n = 0, l, x, rc;
   int flags = FNM_PERIOD;
  
   if (ipcmain == NULL) { 
      return -EFAULT;
   };

   if (plist == NULL) return -EINVAL;
   if (glob == NULL) glob = "*";

   DEBUG3("glob is \"%s\"", glob);
   list = malloc(sizeof(char *) * (ipcmain->svctbl_length+1)); 
   namelens = malloc(sizeof(int) * (ipcmain->svctbl_length+1)); 
   
   DEBUG3("clearing the lists");
   for (index = 0; index < ipcmain->svctbl_length+1; index++) {
      list[index] = NULL;
      namelens[index] = 0;
   };

   for (index = 0; index < ipcmain->svctbl_length; index++) {
      
      if (svctbl[index].type == UNASSIGNED) continue;

      svcname = svctbl[index].servicename;
      
      DEBUG3("checking index %d service %s type = %d", 
	     index, svcname, svctbl[index].type);
     


      rc = fnmatch(glob, svcname, flags);
      DEBUG3("fnmatch returned %d No match = %d", rc, FNM_NOMATCH);
      if (rc == FNM_NOMATCH) continue;
      if (rc != 0) {
	 Warning ("fnmatch failed due to illegal glob pattern");
	 return -EFAULT;
      };
      
      l = strlen (svcname);

      /* now we check to see if we've found it before */
      x = 0;
      DEBUG3("checking the %d matches we got for duplicates", n); 
      for (i = 0; i < n; i++) {
	 if (l != namelens[i]) continue;
	 if (strcmp(list[i], svcname) == 0) {
	    x = 1;
	    break;
	 };
      };
      if (x) continue;
      
      /* new to the list, now add */ 
      list[n] = svcname;
      namelens[n] = l;
      n++;
   };
   DEBUG3 ("the matches %d", n);

   /* calc the size for the return buffer which hold a null terminated
      pointer array at the head, and all the strings (nul term'ed)
      consecutively following */   
   for (l = 0, i = 0; i < n; i++) {
      l += namelens[i];
   };
   l += n;
   DEBUG3 ("the total string length is %d", l);
   
   l +=  sizeof(char*) * (n+1) + 4;
   rlist = malloc(l); // adding 4 bytes as a paranoia safty 
   DEBUG3 ("the total buffer length is %d", l);
   
   // tmp points to the place to put the strings
   tmp = ((char *) rlist) + sizeof(char*) * (n+1);  
   for (l = 0, i = 0; i < n; i++) {
      rlist[i] = tmp;
      strncpy(tmp, list[i],  namelens[i]);
      tmp += namelens[i];
      tmp[0] = '\0';
      tmp++;
   };

   free(list);
   free(namelens);
   
   rlist[n] = NULL;
   *plist = rlist;
   DEBUG("Completes");  
   return n; 
};


  /* depreciated */
SERVICEID _mw_get_service_byname (char * svcname, int convflag)
{
  int index, type, selectedid = UNASSIGNED;
#ifdef MSGCTLSTATFIX
  int rc, qlen;
  struct msqid_ds this_mq_stat, last_mq_stat;
#endif

  serviceentry * lasttblent = NULL; 
  serverentry * lastsrv = NULL, * thissrv = NULL;

  if (ipcmain == NULL) { 
    return UNASSIGNED;
  };

  if (convflag)
    type = MWCONVSVC;
  else 
    type = MWCALLSVC;

  for (index = 0; index < ipcmain->svctbl_length; index++) {
    /* if the table entry is not in use, skip */
    if (svctbl[index].server == UNASSIGNED) 
      continue;
    /* conversational or not test */
    if (svctbl[index].type != type) 
      continue;
    /* if the name fits, then so does the shoe. */
    if (strncmp(svctbl[index].servicename, svcname, MWMAXSVCNAME) 
	== 0) {
      /* now we must select which serviceid to return
	 THere may be many servers providing this service.
	 First of all local servers are preferred 
	 (in Ver 1.X there are only locals. but)
	 We what the server that are likely to service fastet*/ 
      thissrv = _mw_get_server_byid(svctbl[index].server);
      if (thissrv == NULL) continue;
      if (thissrv->nowserving < 0) {
	/* If the server is idle there is no one else that can do it
	   faster */
	return index | MWSERVICEMASK;
      }

      /* MSGCTLSTATFIX shall be set iff msgctl(..., IPC_STAT, ...) becomes
	 legal if queue not readable, but writeable. 
	 Needs kernel fix, this only a problem with msgctl(), not shmctl(), or semctl()

	 A consequence of the lack of this fix is that we migth return from here, 
	 with reference to a server how has non writable queue, or has its queue removed.
	 I really should never happen, but I want to 
      */
      if (lasttblent == NULL) {
	/* first find */
#ifdef MSGCTLSTATFIX
	rc = msgctl(thissrv->mqid, IPC_STAT, &last_mq_stat);
	if (rc == 0) {
#endif 
	  lasttblent = & svctbl[index];
	  selectedid = index;
	  lastsrv = thissrv;
#ifdef MSGCTLSTATFIX
	}
	/* short cut to exit if found server is idle*/
	if ( (last_mq_stat.msg_qnum == 0) 
	     && (thissrv->nowserving == UNASSIGNED) ) 
	  break;
	else 
	  continue;
#endif 
      };

      /* the call to this function is done before the message is constucted, 
	 thus the time delay  *may*  cause this decision out to be dated.
	 see _mwacallipc()
      */

#ifdef MSGCTLSTATFIX
      rc = msgctl(thissrv->mqid, IPC_STAT, &this_mq_stat);
      if (rc != 0) continue;
      if (this_mq_stat.msg_qnum < last_mq_stat.msg_qnum) {
	memcpy(&last_mq_stat, &this_mq_stat, sizeof(struct msqid_ds));
#endif 
	lasttblent = & svctbl[index];
	selectedid = index;
	lastsrv = thissrv;
#ifdef MSGCTLSTATFIX
      };
#endif 
    }
  }
  if (selectedid == UNASSIGNED) return -ENOENT;
  return selectedid | MWSERVICEMASK;
};


gatewayentry * _mw_get_gateway_table (void)
{  
  return gwtbl;
};

gatewayentry * _mw_get_gateway_byid (GATEWAYID gwid)
{
  int index;
  gatewayentry * gwent; 

  if (ipcmain == NULL) { 
    DEBUG1("_mw_get_gateway_byid called while ipcmain == NULL");
    return NULL;
  };
  if (gwid & MWSERVERMASK != MWSERVERMASK) {
    DEBUG1("_mw_get_gateway_byid gwid & MWGATEWAYMASK %d != %d", gwid & MWSERVERMASK, MWSERVERMASK);
    return NULL;
  };
  index = gwid & MWINDEXMASK;
  if (index >= ipcmain->gwtbl_length) {
    DEBUG1("_mw_get_gateway_byid index to big %d > %d", index, ipcmain->gwtbl_length);
    return NULL;
  }
  
  gwent = & gwtbl[index];
  if (gwent->status == UNASSIGNED) {
    DEBUG1("_mw_get_gateway_byid gatewayid is not in use");
    return NULL;
  };
  return gwent;
};

/* given a MW id, return the messaeg queue id for the id, or -ENOENT if
   No agent with that ID, or -EBADR if mwid is out of range. */
int _mw_get_mqid_by_mwid(int dest)
{
  int qid;
  serverentry * srvent;
  cliententry * cltent;
  gatewayentry * gwent;

  if (dest & MWSERVERMASK) {
    srvent = _mw_get_server_byid(dest);
    if (srvent) 
      qid = srvent->mqid;
    else 
      qid = -1;
  } else if (dest & MWCLIENTMASK) {
    cltent = _mw_get_client_byid(dest);
    if (cltent)
      qid = cltent->mqid;
    else 
      qid = -1;
  } else if (dest & MWGATEWAYMASK) {
    gwent = _mw_get_gateway_byid(dest);
    if (gwent)
      qid = gwent->mqid;
    else 
      qid = -1;
  } else if (dest == 0) {
    /* 0 means mwd. */
    qid = _mw_mwd_mqid();
  } else {
    return -EBADR;
  };
  if (qid == -1) return -ENOENT;
  return qid;
};

/* overall health of the MW instance. 
   returns: 
   0 OK
   -ENAVAIL not connected;
   -ESHUTDOWN if mwd has marked ipcmain shutdown or dead.
   -ECONNREFUSED if mwd is in the progress or booting up.
   -EUCLEAN local error (a can't happen)
   -ECONNABORTED if mwd requester has died abruptly.

   * should we clean up on error?
*/

int _mwsystemstate(void)
{
  int rc; 

  if (ipcmain  == NULL) return -ENAVAIL;
  if ( (ipcmain->status == MWSHUTDOWN) || (ipcmain->status == MWSHUTDOWN) )
    return -ESHUTDOWN;
  if (ipcmain->status == MWBOOTING) return -ECONNREFUSED;
  
  /* this really should never come true */
  if ( (clttbl == NULL) || (gwtbl == NULL) ||
       (srvtbl == NULL) || (svctbl == NULL) ) return -EUCLEAN;
  
  /* test of if mwd requester is running */
  rc = kill (ipcmain->mwdpid, 0);  
  if  ( ! ( (rc == 0) || (errno == EPERM) )) return -ECONNABORTED;

  return 0;
};
  
/*
  Statistics: NB: this applies only to servers.
  THis is done by calling _mw_stat_begin() when something to 
  are fetched of the input queue, and _mw_stat_end() else.

  We also have an implicit task for servers that issue an _mw_stat_update()
  every 60 seconds.

  */

void _mw_set_my_status(char * status)
{
  serverentry * myentry = NULL;

  myentry = _mw_get_server_byid (myserverid);

  if (myentry == NULL) {
    Warning("Failed to set my status = \"%s\", probably not attached",
	  status == NULL ? "(idle)": status);
    return;
  };

  /* prevent accidental overwrite with too long statuses. */
  if ((status != NULL) && (strlen(status) >= MWMAXNAMELEN) )
    status[MWMAXNAMELEN-1] = '\0';

  if (status == NULL) {
    myentry->status = MWREADY;
    strncpy(myentry->statustext, "(idle)", MWMAXNAMELEN);
    DEBUG1("Returning to idle state");
  } else if (strcmp(status, SHUTDOWN) == 0) {
    myentry->status = MWSHUTDOWN;
    strncpy(myentry->statustext, SHUTDOWN, MWMAXNAMELEN);
    DEBUG1("Status is now SHUTDOWN");
  } else {
    myentry->status = MWBUSY;
    /* status is a service name which is always legal length. */
    strncpy(myentry->statustext, status, MWMAXNAMELEN);
    DEBUG1("Starting service %s", status);
  }
  return;
};

void _mw_update_stats(int qlen, int waitmsec, int servmsec)
{
  /* As above, making decaying stats is a challenge. mwd must make the decay, 
     and thus the update..
     I haven't yet figured out how to pass the updae data to mwd.
  */
  
  DEBUG1("Last service request waited %d millisec to be processes and was  completed in %d millisecs. Message queue has now %d pending requests.", waitmsec, servmsec, qlen);
  return;
};
