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

   
#ifndef _MIDWAY

#include <time.h>

#define _MIDWAY

#define UNASSIGNED  -1

/* Service name length is max 32, historic to be 
 * tuxedo compatible, really should be the same here.
 */
#define MWMAXSVCNAME 32 + 1
#define MWMAXNAMELEN 64 + 1

typedef int SERVERID;
typedef int CLIENTID;
typedef int GATEWAYID;
typedef int SERVICEID;
typedef int CONVID;
/* The max number of each ID are limitet to 24 bits, we use the top 
 * 8 bit to disdingush among them, the lower 24 bits are the index into 
 * their respective tables. */
#define MWSERVERMASK     0x01000000
#define MWCLIENTMASK     0x02000000
#define MWGATEWAYMASK    0x04000000
#define MWSERVICEMASK    0x08000000
#define MWINDEXMASK      0x00FFFFFF


typedef struct {
  int handle;
  CLIENTID cltid;
  SERVERID srvid;
  SERVICEID svcid;
  char * data;
  int datalen;
  int flags;
  int appreturncode;
  char service[MWMAXSVCNAME];

  time_t deadline; /* deadline here is pretty much a timeval struct. */
  int   udeadline; /* see gettimeofday(); */
} mwsvcinfo;  


/****************** FLAGS ***************/
/* Flags for mw(a)call */
#define MWNOREPLY    0x00000001
#define MWNOBLOCK    0x00000002
#define MWNOTIME     0x00000004
#define MWSIGRST     0x00000008
#define MWNOTRAN     0x00000010
#define MWUNIQUE     0x00000020

#define MWFASTPATH   0x00000100
#define MWSAFEPATH   0x00000200
 
/* Flags*/
#define MWCONV       0x00010000
#define MWSTDIO      0x00020000

/* flags for mwattch() */
#define MWCLIENT     0x00000000
#define MWNOTCLIENT  0x01000000 /* for internal use, not legal */
#define MWSERVER     0x02000000
#define MWSERVERONLY 0x03000000
#define MWGATEWAY    0x04000000

/* for use with mwlog(), got idea from CA/Unicenter Agentworks */
#define MWLOG_ERROR     0
#define MWLOG_WARNING   1
#define MWLOG_INFO      2
#define MWLOG_DEBUG     3
#define MWLOG_DEBUG1    4
#define MWLOG_DEBUG2    5
#define MWLOG_DEBUG3    6
#define MWLOG_DEBUG4    7

/* API prototypes */

/* API for connecting to a server domain */

#ifdef	__cplusplus
extern "C" {
#endif
  
  
  /* 
     If address is 0 (zero) we use the MWURL or MWADDRESS
     env var to get a attach string. URL encoded:
     
     ! midway:// or mwp:// (MidWay Protocol) ?
     midway://path           # for future use of POSIX 1b (4) IPC.
     midway://hostname:port  # TCP/IP.
     http://hostname:port       # TCP/IP, limited functionality
     ipc://[[instancename][@userid]|key]

     For servers only IPC is legal.

     on ipc:// the default is the default instance for the current userid.
     Either a hard ipc key may be specified, or an instancename@userid.
     If userid is ommitted, current, is used, if instancename is omitted, 
     the default is used. If userid is spesified, only the current is legal 
     servers.
  */
  
  /* if all args are NULL and MWATTACH are not set, we look for 
     ~/midway/anonymous/key
     if only name is set we look for 
     ~/midway/name/key
  */
  int mwattach(char * adr, char * name, 
	       char * username, char * password, int flags);
  int mwdetach();

  /* Client call API. (mwfetch has additional functoniality in servers. */
  int mwcall(char * svcname, 
	     char * cdata, int clen, 
	     char ** rdata, int * rlen, 
	     int * appreturncode, int flags);
  int mwacall(char * svcname, char * data, int len, int flags);
  int mwfetch(int handle, char ** data, int * len, int * appreturncode, int flags);

  /* server API */
  int mwforward(char * service, char * data, int len, int flags);
  int mwreply(char * rdata, int rlen, int returncode, int appreturncode);
#define mwreturn(rd,rl,rc,ac) mwreply( rd,rl,rc,ac); return rc

  int mwprovide(char * service, int (*svcfunc)(mwsvcinfo *), int flags);
  int mwunprovide(char * service);

  int mwMainLoop();
  int mwservicerequest(int flags);

  /* conversational API */

  /* transactional API */
  int mwbegin(float sec, int flags);
  int mwcommit();
  int mwabort();

  /* shared memory buffer memory management */
  void * mwalloc(int size);
  void * mwrealloc(void *, int size);
  int mwfree(void *);

  /* task API */

  /* internal logging API */
  void mwsetlogprefix(char * fileprefix);
  int mwsetloglevel(int level);
  void mwlog(int level, char * format, ...);


#ifdef	__cplusplus
}
#endif

/* MidWay spesific error codes. errno may not be limitet to < 255 even
 if they usually see to be.  _mw_*() functions usually return the
 errno number negative on error no MidWay specific error numbers must
 not collide with /usr/include/asm/errno.h. MidWay never uses the
 global errno variable in order to be thread safe. In the cases errno
 is needed from library/syscalls a threax mutex must be added. Thus in
 order to make a thread safe MidWay lib a compile time option must be
 given. As well as a thread lib for linking. */

/*
#define EMW_INDEX 		1000
#define EMWSHUTDOWN 		(EMW_INDEX + 1)
#define EMWTBLFULL 		(EMW_INDEX + 2)
#define EMWNOMEM		(EMW_INDEX + 3)
#define EMWILREQ		(EMW_INDEX + 4)
#define EMWLINKLIB		(EMW_INDEX + 5)
#define EMWSIZE			(EMW_INDEX + 6)
#define EMWCONNECTED		(EMW_INDEX + 7)
#define EMWNOTCONNECTED		(EMW_INDEX + 8)
#define EMWMSGIO		(EMW_INDEX + 9)
#define EMWCANTHAPPEN		(EMW_INDEX + 10)

#define EMWNYI			(EMW_INDEX + 99)
*/
#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE 
#define TRUE 1
#endif

#ifndef MWFAIL
#define MWFAIL 0
#endif

#ifndef MWSUCCESS
#define MWSUCCESS 1
#endif


#endif /* _MIDWAY */

