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
  License along with the MidWay distribution; see the file COPYING. If not,
  write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA. 
*/

#ifndef _IPCTABLES_H
#define _IPCTABLES_H

#include <sys/ipc.h>
#include <netinet/in.h>
#include <MidWay.h>

// location
#define GWLOCAL    0
#define GWPEER     1
#define GWREMOTE   2
#define GWCLIENT   3

#define MWAUTHNONE   0
#define MWAUTHUNIX   1
#define MWAUTHPASSWD 2
#define MWAUTHSSL    3

/* status values. */
#define MWREADY    0
#define MWSHUTDOWN 1
#define MWBOOTING  2
#define MWBUSY     3
#define MWDEAD     4
#define MWBLOCKED  5

/* some special strings used to set these statuses via _mw_set_my_status(char *);*/
#define SHUTDOWN  ".(shutdown)"


struct cliententry
{
  int32_t type; /* 0 client only, 1 server
	     * I originally had somethink like this in mind;
	     * MWTYPEIPC, MWTYPETCP, MWTYPEUDP, MWTYPESERVER or MWTYPEGATEWAY 
	     * MWTYPESERVER and MWTYPEGATEWAY implies IPC. 
	     But server only servers don't show up here at all. 
	     remote clients will probably don't either. */
  int32_t location; /* MWLOCAL, MWIMPORTED */
  int32_t status;
  pid_t pid;
  int32_t mqid;
  
  GATEWAYID gwid;;
  SERVERID srvid;

  char clientname[MWMAXNAMELEN];
  char username[MWMAXNAMELEN];
  
  char addr_string[MWMAXNAMELEN];

  int32_t authtype; /* MWAUTHNONE, MWAUTHPASSWD */;
  int64_t authref;
  
  int64_t connected;
  int64_t last_request;
  int32_t idle_timeout;

  int32_t requests;

};
typedef struct cliententry cliententry;

struct serverentry
{

  char servername[MWMAXNAMELEN];
  int32_t mqid;
  pid_t pid;
  
  int32_t status;
  char statustext[MWMAXSVCNAME];
  SERVICEID nowserving;
  
  int32_t mwdblock; /* MWNORMAL MWSHUTDOWN, MWBLOCKED, MWDEAD */;
  int32_t trace;

  int64_t booted;
  int64_t lastsvccall;

  float percbusy1, percbusy5, percbusy15; 
  float count1, count5, count15;
  float qlenbusy1, qlenbusy5, qlenbusy15; 
  float avserv1, avserv5, avserv15; 
  float avwait1, avwait5, avwait15; 
};
typedef struct serverentry serverentry;

/* Service types */
#define MWCALLSVC 1
#define MWCONVSVC 2

struct serviceentry
{
  int32_t type; /* MWCALLSVC, MWCONVSVC or UNASSIGNED */
  int32_t location; /* GW* or UNASSIGNED (unused flag) */;

  SERVERID server;
  GATEWAYID gateway;
  char mwname[MWMAXNAMELEN];
  int32_t cost;
  /* the address for the functions does not really belong here But it
     spares us having a separate table in private memory of the
     server. Of counse it is only valid for location = LOCAL;*/
  void * svcfunc;
  char servicename[MWMAXSVCNAME];

};
typedef struct serviceentry serviceentry;

struct gatewayentry
{
  char instancename[MWMAXNAMELEN];
  char domainname[MWMAXNAMELEN];

  int32_t location; /* GW* */
  int32_t srbrole; /* only used for local */

  int32_t mqid;
  pid_t pid;
  int32_t status ;
  int64_t connected;
  int64_t last_call;

  int32_t trace;
  int32_t mesgsent;
  int32_t mesgrecv;

  int32_t imported_svc;
  int32_t exported_svc;

  char addr_string[MWMAXNAMELEN];

};

typedef struct gatewayentry gatewayentry;

struct conv_entry
{
  int32_t localserver;
  SERVERID srvid;
  GATEWAYID gwid;
  CLIENTID cltid;

  int64_t connected;
  int32_t idle_timeout; /* millisec */;
  int64_t lastsend;
  int32_t bytes_exchanged;
  int32_t sends_exchanged;
};
typedef struct conv_entry conv_entry;


struct ipcmaininfo
{
  char magic[8];
  int32_t vermajor, verminor, patchlevel;
  pid_t mwdpid, mwwdpid;
  int32_t mwd_mqid;
  char mw_instance_name[MWMAXNAMELEN];
  char mw_instance_id[MWMAXNAMELEN];
  char mw_homedir[256];
  char mw_bufferdir[256];

  int32_t status; /* BOOTING, RUNNING, SHUTDOWN*/
  int64_t boottime;
  int64_t lastactive;
  int64_t configlastloaded;
  int64_t shutdowntime;

  int32_t heap_ipcid;
  int32_t clttbl_ipcid;
  int32_t srvtbl_ipcid;
  int32_t svctbl_ipcid;
  int32_t gwtbl_ipcid;
  int32_t convtbl_ipcid;

  int32_t clttbl_length;
  int32_t srvtbl_length;
  int32_t svctbl_length;
  int32_t gwtbl_length;
  int32_t convtbl_length;

  int32_t gwtbl_nextidx;  
  int32_t gwtbl_lock_sem;
};

typedef struct ipcmaininfo ipcmaininfo;

#endif /* _IPCTABLES_H */

/*
 * functions for accessing IPC tables.
 */
ipcmaininfo * _mw_ipcmaininfo(void); 

cliententry  * _mw_getcliententry(int);
serverentry  * _mw_getserverentry(int);
serviceentry * _mw_getserviceentry(int);
gatewayentry * _mw_getgatewayentry(int);
conv_entry   * _mw_getconv_entry(int);


int32_t  _mw_attach_ipc(key_t, int);
void _mw_detach_ipc(void);
void _mw_set_shmadr (ipcmaininfo * im, cliententry * clt, serverentry * srv, 
		     serviceentry * svc, gatewayentry * gw, conv_entry * conv);


void _mw_set_my_serverid(SERVERID);
void _mw_set_my_clientid(CLIENTID);
void _mw_set_my_gatewayid(GATEWAYID);
MWID _mw_get_my_mwid(void);
SERVERID _mw_get_my_serverid(void);
CLIENTID _mw_get_my_clientid(void);
GATEWAYID _mw_get_my_gatewayid(void);

/* Table Lookup functions */
int  _mw_mwd_mqid(void);
int  _mw_my_mqid(void);

/* NB: All these functions return pointer to the shared memory segments
   no need to use free()
*/
cliententry  * _mw_get_client_byid (CLIENTID cltid);
serverentry  * _mw_get_server_byid (SERVERID srvid);
serviceentry * _mw_get_service_byid (SERVICEID svcid);
MWID           _mw_get_provider_by_serviceid (SERVICEID svcid);
SERVICEID      _mw_get_service_byname (char * svcname, int flags); 
SERVICEID    * _mw_get_services_byname (char * svcname, int * n, int convflag);
//MWID         * _mw_get_service_providers(char * svcname, int convflag);
int _mw_list_services_byglob (char * glob, char *** plist, int flags);

gatewayentry * _mw_get_gateway_table (void);
gatewayentry * _mw_get_gateway_byid (GATEWAYID srvid);

int _mw_get_mqid_by_mwid(int mwid);

/* a test to see id the instance is up properly, not booting, in shutdown mode nor dead. */
int _mwsystemstate(void);

/* stat & info functions */
void _mw_set_my_status(char * status);
void _mw_update_stats(int qlen, int waitmsec, int servmsec);
