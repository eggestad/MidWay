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

/** @file
    This is the inlude that is shipped with the library and defines
    everything that is exposed to the user programs.

    Howevere the doxygen tags here are really intended for the
    internal MidWay documentatiom.
*/

#ifndef _MIDWAY_H

#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdint.h>

#define _MIDWAY_H

/**
   The MWID unassigned value.
 */
#define UNASSIGNED  -1

/**
   Service name max length is 32, historic to be tuxedo compatible,
   really should be #MWMAXNAMELEN. This define is the max size plus
   one byte for nul termination.
 */
#define MWMAXSVCNAME (32 + 1)
/// The minumum allocated area needed to hold a name of any other kind, like clientname. 
#define MWMAXNAMELEN (64 + 1)

/**
   @ingroup mwid 
   A typedef to create a type to hold a server id  */
typedef int SERVERID;
/**
   @ingroup 
   mwid A typedef to create a type to hold a client id 
*/
typedef int CLIENTID;
/**
   @ingroup mwid 
   A typedef to create a type to hold a gateway id
*/
typedef int GATEWAYID;
/**
   @ingroup mwid 
   A typedef to create a type to hold a service id
*/
typedef int SERVICEID;
/**
   @ingroup mwid 
   A typedef to create a type to hold a conversational service id 
*/
typedef int CONVID;

/**
   The type for the handle returned by mwcall()
*/
typedef int32_t mwhandle_t;

/** 
    The struct passed a service handler.

    This struct contain all available info about a service call, and
    is the only passed argument to a service function/handler.
*/
typedef struct {
  mwhandle_t  handle;
  CLIENTID cltid;
  GATEWAYID gwid;
  SERVERID srvid;
  SERVICEID svcid;
  char * data;
  size_t datalen;
  int flags;
  int appreturncode;
  char service[MWMAXSVCNAME];

  time_t deadline; /* deadline here is pretty much a timeval struct. */
  int   udeadline; /* see gettimeofday(); */

   int authentication;
   char username[MWMAXNAMELEN];
   char clientname[MWMAXNAMELEN];
} mwsvcinfo;  


/****************** FLAGS ***************/
/* Flags for mw(a)call/mwfetch/mwreply */
#define MWNOREPLY    0x00000001 //!< A service call shall not have any reply 
#define MWNOBLOCK    0x00000002 //!< If a system call would block, return with EAGAIN. 
#define MWNOTIME     0x00000004 //!< Do not use the timeout value set by mwbegin(() (FUTURE USE)
#define MWSIGRST     0x00000008 //!< Signal restert, if a system call was interrupted, restart
#define MWNOTRAN     0x00000010 //!< Do not include service call in an ungoing transaction (FUTURE USE)
#define MWUNIQUE     0x00000020 //!< the provided service name shall be unique (FUTURE USE)
#define MWMULTIPLE   0x00000040 //!< Multiple replies, mwfetch shall not concatinate all replies. 

/* the IPC only flag was introduced for the GW, inorder to ensure that
incomming service calls aren't routed back out. As such this flag may
dissappear whenever remote domains are implemented. However it *might*
be a nice feature for high perf apps, that want to make sure local
services only are called.  (in this case the service in question
should begin with . ) */
#define MWIPCONLY    0x00000080 /* only allow local IPC services to e selected */

#define MWFASTPATH   0x00000100
#define MWSAFEPATH   0x00000200
 
/* Flags for conv */
#define MWCONV       0x00010000
#define MWSTDIO      0x00020000

/* flags for events */
#define MWEVSTRING   0x00000000 // exact matches
#define MWEVGLOB     0x00100000 // matches according to normal shell filename wildcards ?* etc (see fnmatch)
#define MWEVREGEXP   0x00200000 // POSIX regular expression (see regex(7))
#define MWEVEREGEXP  0x00400000 // extended regular express ----- || -----

/* flags for mwattach() */
#define MWCLIENT     0x00000000
#define MWNOTCLIENT  0x01000000 /* for internal use, not legal */
#define MWSERVER     0x02000000
#define MWSERVERONLY 0x03000000
#define MWGATEWAY    0x04000000

/* for use with mwlog() */
#define MWLOG_FATAL     0
#define MWLOG_ERROR     1
#define MWLOG_WARNING   2
#define MWLOG_ALERT     3
#define MWLOG_INFO      4
#define MWLOG_DEBUG     5
#define MWLOG_DEBUG1    6
#define MWLOG_DEBUG2    7
#define MWLOG_DEBUG3    8
#define MWLOG_DEBUG4    9

/* authentication methods */
#define MWAUTH_NONE     0
#define MWAUTH_PASSWORD 1   // plain old password
#define MWAUTH_X509     2   // x509 certificate (ssl/tls level auth
#define MWAUTH_KRB5     3   // kerberos


/* for OS's (like mac osx that are missing some errno values */

#ifndef ELAST
#define ELAST 1000
#endif

#ifndef EBADR
#define EBADR ELAST+1
#endif

#ifndef ENAVAIL
#define ENAVAIL ELAST+2
#endif

#ifndef EUCLEAN
#define EUCLEAN ELAST+3
#endif

#ifndef EBADRQC
#define EBADRQC ELAST+4
#endif

#ifndef ENONET
#define ENONET ELAST+5
#endif

#ifndef ERESTART
#define ERESTART ELAST+6
#endif

#ifndef ELIBACC
#define ELIBACC ELAST+6
#endif



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

  int mwsetcred(int authtype, const char * username, ...);
   /*
  int mwchkauth(void);
  int (*getcredCB)(int authtype, ...);
  int mwregistergetcredCB(int (*getcredCB)(int authtype, ...));
   */
  int mwattach(const char * adr, const char * name, int flags);
  int mwdetach(void);
  char * mwgeturl(void);

  /* Client call API. (mwfetch has additional functoniality in servers. */
  int mwcall(const char * svcname, 
	     const char * cdata, size_t clen, 
	     char ** rdata, size_t * rlen, 
	     int * appreturncode, int flags);
  int mwacall(const char * svcname, const char * data, size_t len, int flags);
  int mwfetch(int * handle, char ** data, size_t * len, int * appreturncode, int flags);
   
   /* return a list of service names matching glob. What plist points
      to shall be free'ed with a single free */
  int mwlistsvc (char * glob, char *** plist, int flags);

  /* server API */
  int mwforward(const char * service, const char * data, size_t len, int flags);
  int mwreply(const char * rdata, size_t rlen, int returncode, int appreturncode, int flags);
#define mwreturn(rd,rl,rc,ac) mwreply( rd,rl,rc,ac, 0); return rc

  int mwprovide(const char * service, int (*svcfunc)(mwsvcinfo *), int flags);
  int mwunprovide(const char * service);

  int mwMainLoop(int);
  int mwservicerequest(int flags);

  /* conversational API */

  /* transactional API */
  int mwbegin(float sec, int flags);
  int mwcommit(void);
  int mwabort(void);

  /* shared memory buffer memory management */
  void * mwalloc(size_t size);
  void * mwrealloc(void *, size_t size);
  int mwfree(void *);

  /* task API */
#define PTASKMAGIC 0x00effe00
  typedef long PTask;
  typedef int (*taskproto_t)(PTask);

  PTask _mwaddtaskdelayed(taskproto_t func, char * name, double interval, double initialdelay);
  PTask _mwaddtask(taskproto_t func, char * name, double interval);
#define mwaddtaskdelayed(f, i, d) _mwaddtaskdelayed(f, #f, i, d)
#define mwaddtask(f, i) _mwaddtask(f, #f, i)

  int mwwaketask(PTask t);
  int mwdotasks(void) ;
  int mwsettaskinterval (PTask pt, double interval) ;
  int mwsuspendtask(PTask pt);
  int mwresumetask(PTask pt);
  // helpers, these wrap around sigprocmask, and manipulate the set of blocked signals. 
  void mwblocksigalarm(void);
  void mwunblocksigalarm(void);


  /* internal logging API */
  void mwsetlogprefix(const char * fileprefix);
  void mwopenlog(const char * progname, const char * fileprefix, int loglevel);
  int mwsetloglevel(int level);


#ifdef __GNUC__
/* gcc hack in order to get wrong arg type in mwlog() */
#define FORMAT_PRINTF __attribute__ ((format (printf, 2, 3)))
#else
#define FORMAT_PRINTF 
#endif
  void mwlog(int level, const char * format, ...) FORMAT_PRINTF;


  /* event API */
  int mwsubscribe(const char * pattern, int flags);
  int mwsubscribeCB(const char * pattern, int flags, void (*func)(const char * eventname, const char * data, size_t datalen));
  int mwunsubscribe(int subscriptionid);
  int mwevent(const char * eventname, const char * data, size_t datalen, const char * username, const char * clientname);
#define mweventbcast(eventname, data, datalen) mwevent( eventname, data, datalen, NULL, NULL)
  void mwrecvevents(void);

  /* discovery api */
  
  typedef struct  {
    char version[8];
    char instance[MWMAXNAMELEN];
    char domain[MWMAXNAMELEN];
    struct sockaddr address;
  } instanceinfo;

  instanceinfo * mwbrokerquery(const char * domain, const char * instance);
  

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


#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE 
#define TRUE 1
#endif

#define MWFAIL    0
#define MWSUCCESS 1
#define MWMORE    2



// the following is internal API's only meant used for modules/plugins
// implementing the MidWay API in another language. See the Perl and
// Python modules, and the C implementations that is within MidWay itself.

int _mw_isattached(void);
int _mwsystemstate(void);

void _mw_incprovided(void);
void _mw_decprovided(void);

int _mwsubscribe(const char * pattern, int subid, int flags);
int _mwunsubscribe(int subid);

int _mw_tasksignalled(void);

SERVICEID _mw_ipc_provide(const char * servicename, int flags);
int _mw_ipcsend_unprovide(const char * servicename,  SERVICEID svcid);

mwsvcinfo * _mwGetServiceRequest(int);
typedef void (*mw_do_event_handler_t) (int subid, const char * event, const char * data, int datalen);


#endif /* _MIDWAY */

/* Emacs C indention
Local variables:
c-basic-offset: 2
End:
*/
