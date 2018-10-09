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
 * $Log$
 * Revision 1.18  2005/06/25 15:36:22  eggestad
 * missed a doxygen tag ob event->returncode
 *
 * Revision 1.17  2005/06/25 12:05:32  eggestad
 * - added doxygen doc
 * - prototype for _mw_get_caller_mwid()
 *
 * Revision 1.16  2004/12/29 19:59:01  eggestad
 * handle datatype fixup
 *
 * Revision 1.15  2004/11/17 20:47:15  eggestad
 * updated struct for IPC coexist for 32 and 64 bit apps
 *
 * Revision 1.14  2004/08/10 19:38:10  eggestad
 * - ipcmessages is now 32/64 bit interchangeable
 * - added messages for large buffer alloc
 *
 * Revision 1.13  2004/03/20 18:57:47  eggestad
 * - Added events for SRB clients and proppagation via the gateways
 * - added a mwevent client for sending and subscribing/watching events
 * - fix some residial bugs for new mwfetch() api
 *
 * Revision 1.12  2004/03/01 12:56:35  eggestad
 * change in mwfetch() params\
 *
 * Revision 1.11  2002/10/20 18:09:39  eggestad
 * added instance to the Call struct. Needed later for gateway to gateway calls
 *
 * Revision 1.10  2002/10/17 22:04:24  eggestad
 * - added more field to _mwacallipc() needed for gateway to gateway calls
 * - Call struct now also has callerid and hops fields
 *
 * Revision 1.9  2002/10/03 21:01:38  eggestad
 * - new prototypes for (un)provide_for_id()
 *
 * Revision 1.8  2002/09/22 23:01:16  eggestad
 * fixup policy on *ID's. All ids has the mask bit set, and purified the consept of index (new macros) that has the mask bit cleared.
 *
 * Revision 1.7  2002/08/09 20:50:15  eggestad
 * A Major update for implemetation of events and Task API
 *
 * Revision 1.6  2002/07/07 22:45:48  eggestad
 * *** empty log message ***
 *
 * Revision 1.5  2002/02/17 13:48:01  eggestad
 * added prototypes for _mw_ipc_(un)provide()
 *
 */

#ifndef IPCMESSAGES_H
#define IPCMESSAGES_H

#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>

#include <MidWay.h>

/** @file
    ipcmessages.h defines the structs for all the messages sent on IPC message queues. 
    We also define all the special values needed for indicator fields.

*/
/* the message numbers are inspired by ISO8583, it seemed fitting ;)
   (inside joke really, I used to work a lot with creditcard message
   protocols. */

/// mtype for an attach request
#define ATTACHREQ 0x0800
/// mtype for an attach reply
#define ATTACHRPL 0x0810
/// mtype for an detach request
#define DETACHREQ 0x0820
/// mtype for an detach reply
#define DETACHRPL 0x0830

///  used by _mw_ipcsend_attach(), indicate that the mwmber may use client API
#define MWIPCCLIENT   0x01
///  used by _mw_ipcsend_attach(), indicate that the mwmber may use server API
#define MWIPCSERVER   0x02
///  used by _mw_ipcsend_attach(), indicate that the mwmber is a gateway
#define MWIPCGATEWAY  0x04
///  used by the gateway to inducate a proxied client
#define MWNETCLIENT   0x10

/* flags used in messages */
#define MWFORCE          0x10
#define MWGATEWAYCLIENT  0x100
#define MWGATEWAYSERVICE 0x200

/** the attach message struct, use for req/reply in attach and detach
    pid is for informational use only, in future threaded client/servers
    pid tends to be diffrent for each thread. For example on 
    linux where the clone(2) sys call is used, or thread id.

    \b Note that #ATTACHREQ and #DETACHREQ is legal only \e to mwd() while
    #ATTACHRPL and #DETACHRPL is legal only \e from mwd().

*/
struct attach
{
   long mtype; ///< #ATTACHREQ, #ATTACHRPL, #DETACHREQ, or #DETACHRPL

   int32_t ipcqid; ///< The requesters ipc message queue is
   pid_t pid;      ///< the requesters pid

   int32_t server;   ///< requester want to be a server, #TRUE/#FALSE 
   char srvname[MWMAXNAMELEN]; ///< server name (only is server == #TRUE
   SERVERID srvid; ///< the assigned serverid, reply only unused in request

   /** requester want to be a client, #TRUE/#FALSE, Setting this to false is usualm it mean that 
       the member can't use client API lige mwcall() mwforward/( is still OK. 
   */
   int32_t client;  
   char cltname[MWMAXNAMELEN]; ///< A descriptive client name for the client, used only in request with client == #TRUE
   CLIENTID cltid; ///< the assigned serverid, reply only unused in request

   GATEWAYID gwid; ///< if not #UNASSIGED, the request is from a gateway on behalf on a network client, if set server must be #FALSE

   int32_t flags; ///< flags for mwd(), currently unused
   /** the return from mwd(). 0 on success, else an neg errno, -ESHUTDOWN if mwd() has
       begun shutdown, -EUCLEAN, if a non ipc same uid member tries to
       be a server. +++
   */
   int32_t returncode; 
};

/// A type for the attach message
typedef struct attach Attach;

/// mtype for a provide request
#define PROVIDEREQ   0x0600
/// mtype for a provide reply
#define PROVIDERPL   0x0610
/// mtype for an unprovide request
#define UNPROVIDEREQ 0x0620
/// mtype for an unprovide reply
#define UNPROVIDERPL 0x0630

/** the provide message struct. Used for requesting provide or
    unprovide of services to mwd, where mwd() add the service to the
    IPC tables.

    \b Note that #PROVIDEREQ and #UNPROVIDEREQ is legal only \e to
    mwd() while #PROVIDERPL and #UNPROVIDERPL is legal only \e from
    mwd().

*/
struct provide
{
   long mtype; ///< one of #PROVIDEREQ, #UNPROVIDEREQ, #PROVIDERPL, or #UNPROVIDERPL
  
   SERVERID srvid; ///< requesting servers MWID, either srdid or gwid must be set
   SERVICEID svcid; ///< if mwd() id acceped, the assigned services MWID, ignored on request
   GATEWAYID gwid; ///< requesting gateways MWID, either srdid or gwid must be set

   char svcname[MWMAXSVCNAME]; ///< the service name, must addhere to name restriction.

   int32_t cost; ///< the routing cost of the service, used by gateways, ignored if srvid != #UNASSIGNED
   int32_t flags; ///< for future use, #MWCONV will be set here if service is conversatinal
   int32_t returncode; ///< 0 on success, -errno or else. 
};

/// A type for the provide message
typedef struct provide Provide;


/// mtype for service request
#define SVCCALL    0x0100
/// mtyoe for service forward (@todo obsolete?)
#define SVCFORWARD 0x0101 
/// mptye for service reply
#define SVCREPLY   0x0110


/**
   The struct used to send service request and reply. 
   
   The message may have attached data in a sharded memory segment. 

   I'm unlikely to use SVCFORWARD on sysvipc.  The reason is that a
   server waiting for something to do don't care if it is a call or
   forward, a reply goes to cltid not matter. Maybe we should have a
   opcode in the messages.  

   The key is that a server when started have only two states it can
   be in, Idle where it waits for a SVCCALL, and callwait (mwfetch())
   where it waits for a SVCREPLY.

   In Future versions supporting XA style transactions, this may have
   to change in some way.  However also in future versions where we
   have POSIX IPC, messages don't have type id.
*/
struct call
{
   long mtype; ///< one of #SVCCALL (#SVCFORWARD) or #SVCREPLY
   /** the call handle, used by the client to distinguish between
       calls, must be returned as is by the server.*/ 
   mwhandle_t handle;
 
   CLIENTID cltid; ///< requesters client MWID
   GATEWAYID gwid; ///< if client is proxied bye a gatewaym the gateways MWID, if set, replies are routed to this MWID. 

   SERVERID srvid; ///< the MWID of the server that is sending the reply, ignored in requests, but is set in forward
   SERVICEID svcid; ///< the service id we're calling, must be set on request

   int32_t forwardcount; ///< the forward count @todo unused. 

   char service[MWMAXSVCNAME]; ///< the service name we're calling , note that the server uses the svcid not this string to find the right service handler function. 
   char origservice[MWMAXSVCNAME]; ///< the original service name, in case of forward @todo unused. 
  
   int64_t issued; ///< the unix time in when the service call was made
   int32_t uissued; ///< the micro seconds into the \a issued second. 
   /** the limit on millisec for how long the client will wait for a
       reply. servers that receive a expired request will dicard
       it. */
   int32_t timeout; 

   int32_t datasegmentid; ///< the data segement id for the data (see shmalloc.h), ignored id datalen == 0
   int64_t data;  ///< the offset into the data segment we'll find the data, ignored if datalen == 0
   int64_t datalen; ///< the length of data in bytes, if 0 there is no attached data. 
   
   int32_t appreturncode; ///< the appreturncode replied with mwreply, and returned my mwfetch()/mwcall()

   int32_t flags; ///< may be #MWNOREPLY

   /// @todo clientname ??

   char instance[MWMAXNAMELEN]; ///< the originating instance of the request, ignored if gwid == #UNASSIGNED
   char domainname[MWMAXNAMELEN]; ///< the originating domain of the request, ignored if gwid == #UNASSIGNED
   MWID callerid; ///< @todo obsolete  @obsolete
   int32_t hops; ///< the number of gateways teh request has passed thru, used by gateways to scrub routing loops. 

   int32_t returncode; ///< one of #MWSUCCESS, #MWFAIL, #MWMORE
};

/// we have a type for the call message. 
typedef struct call  Call;

#define CONNECTREQ    0x0200
#define CONNECTRPL    0x0210
#define DISCONNECTREQ 0x0220
#define DISCONNECTRPL 0x0230
#define SENDREQ       0x0240
#define SENDRPL       0x0250

struct conversation
{
   long mtype; 
  
   CLIENTID cltid;
   GATEWAYID gwid;
   SERVERID srvid;
   SERVICEID svcid;

   char service[MWMAXSVCNAME];

   int64_t data;
   int64_t datalen;

   int32_t flags;

   CONVID convid;
   int32_t returncode;
};

typedef struct conversation  Converse;;



/**
   administrative request/notifications from mwd to servers/gateways and from
   mwadm to mwd.  Used to notify shutdown event. 

   Should this be a special event instead in the general event service
   planned....?  */
typedef struct {
   long mtype; ///< mtypoe must be #ADMREQ
   int32_t opcode; ///< one of #ADMSHUTDOWN, #ADMSTARTSRV, #ADMSTOPSRV, or #ADMRELOADSRV
   CLIENTID cltid; ///< the requesting client id
   int32_t delay; ///< the grace time to shutdown, one used in a shutdown command to mwd()
} Administrative;

/// the mtype for an administrative command
#define ADMREQ 0x1000
/* opcodes */
/// go into shutdown state. 
#define ADMSHUTDOWN  0x10
#define ADMSTARTSRV  0x20
#define ADMSTOPSRV   0x21
#define ADMRELOADSRV 0x22


/**
   Large buffer allocs. Used to request mwd() to create a buffer file in the
   buffer directory (see ipcmain), which is later mmap'ed  */

typedef struct {
   long mtype; ///< on e of #ALLOCREQ, #ALLOCRPL, or #FREEREQ
   MWID mwid;  ///< requesters MWID
   int64_t size; ///< size of requested buffer
   int64_t pages; ///< the actuall number of pages in the buffer file, reply only
   int32_t bufferid; ///< the segement id of the allocated buffer, reply only
} Alloc;

#define ALLOCREQ 0x1200 ///< request mwd() to free a shared buffer file
#define ALLOCRPL 0x1210 ///< a reply to a ALLOCREQ
#define FREEREQ  0x1220 ///< request mwd() to free a shared buffer file
#define FREERPL  0x1230 ///< unused as of now, mwd shall not reply to a free request. 



/* The struct for the event message. used to both subscribe and
   unsubscribe as well as the events them selves. Subscriptions
   request may only be sent to mwd(), and thus subscribtion replies
   may one be sent from mwd().
   
   In the "normal" case of an event, the event can't be "". The data is
   however optional, and the user and group/clientname is optional as
   well.  (first byte is \0 if empty.  flag is not used. The data, if
   passed, is now owned by mwd, and the event sender must not free it.
   The mwd will let it be and send a event message to all subscribers,
   all pointing to the same date buffer.  Therefore recipient of event
   must ack in order for the mwd to know when it's OK to free the
   buffer. The mwd do not ack to the original sender. 

   Events may be sent to a specific userid and/or client name. 

   Gateways may route events between instances in a domain, but
   currently not between domains (inter domain remains unimplemented
   in mwgwd(). )

   In the case of subscribe or unsubscribe, A string, glob, or regexp
   is passed as the event that the mwd will use to match any event to
   any event.  if the event is empty, the mathc string must be passed
   in e data buffer, and data may not be 0. Since regexp may be quite
   huge, the event field may be too small. The flag must be set
   appropriately if the match string is not a string but a glob or
   regexp. 

*/
struct event {
   /** one of #EVENT #EVENTACK #EVENTSUBSCRIBEREQ #EVENTSUBSCRIBERPL
       #EVENTUNSUBSCRIBEREQ #EVENTUNSUBSCRIBERPL
   */
   long mtype; 
   char event[MWMAXNAMELEN]; ///< the event name if #EVENT or a string/glob/regexp in #EVENTSUBSCRIBEREQ
   int32_t eventid; ///< a sender id unique id, used to avoid routing loops, #EVENT only
   int32_t subscriptionid; ///< the id for this  subscription id, set by subscriber.
   int32_t senderid; ///< the sender id of an #EVENT or EVENTSUBSCRIBEREQ/#EVENTUNSUBSCRIBEREQ
  
   int32_t datasegmentid; ///< if there is associated data, the segment id where the buffer is, ignored if datalen == 0
   int64_t data;   ///< the offset into the segment where the data is, ignored if datalen == 0
   int64_t datalen; ///< the length of the data in bytes. 
  
   char username[MWMAXNAMELEN]; ///< the specific userid the event is sent to, all users if username is ""
   char clientname[MWMAXNAMELEN]; ///< the specific clientname the event is sent to, all clients if clientname is ""
  
   int32_t flags; ///< if a gatway received the event from a peer in the sname 
   int32_t returncode; ///< 0 on success or -errno
};

/// if a gatway received the event from a peer we don't send the event to other peer.
#define MWEVENTPEERGENERATED 0x10000000

/// an event sent from a poster to mwd and from mwd to all the recipients. 
#define EVENT                0x400
/// sent from a recipient to mwd when the associated buffer may be freed. 
#define EVENTACK             0x410
/// subscribe to an event
#define EVENTSUBSCRIBEREQ    0x420
/// reply to a subscribe to an event
#define EVENTSUBSCRIBERPL    0x430
/// unsubscribe to a previously subscription
#define EVENTUNSUBSCRIBEREQ  0x440
/// reply to an unsubscribe request
#define EVENTUNSUBSCRIBERPL  0x450

/// we define a type for the EVent struct
typedef struct event Event;
  
/// an union for all IPC messages, used for finding the max message size
typedef union { 
   Attach at;
   Provide prov;
   Call call;
   Converse conv;
   Administrative admin;
   Alloc alloc;
   Event ev;
} _union_ipc_messages;

/** Here we  calculate  MWMSGMAX. */
#define MWMSGMAX sizeof(_union_ipc_messages)

/// the MWID for mwd(), always 0
#define MWD_ID (MWID) 0

/*
 *  lowlevel ipcmsg send & receive
 */
int _mw_ipc_putmessage(int mwid, const char *data, size_t len,  int flags);
int _mw_ipc_getmessage(char * data, size_t *len, int type, int flags);

/*
 * functions used by client requests.
 */
int _mw_ipcsend_attach(int att_type, const char * name, int flags);
int _mw_ipcsend_detach(int force);
int _mw_ipcsend_detach_indirect(CLIENTID cid, SERVERID sid, int force);

int _mw_ipcsend_provide_for_id(MWID mwid, const char * servicename, int cost, int flags);
int _mw_ipcsend_provide(const char * servicename, int cost, int flags);
SERVICEID _mw_ipc_provide(const char * servicename, int flags);

int _mw_ipcsend_unprovide_for_id(MWID mwid, const char * servicename,  SERVICEID svcid);
int _mw_ipcsend_unprovide(const char * servicename, int flags);
int _mw_ipc_unprovide(const char * servicename,  SERVICEID svcid);

int _mwacallipc (const char * svcname, const char * data, size_t datalen, int flags, 
		 MWID mwid, const char * instance, const char * domain, MWID callerid, int hops);
int _mwfetchipc (mwhandle_t * handle, char ** data, size_t * len, int * appreturncode, int flags);

MWID _mw_get_caller_mwid(Call *);
int _mw_ipcconnect(const char * servicename, const char * socketpath, int flags);
int _mw_ipcdisconnect(int fd);

/* event API */
int _mw_ipc_subscribe(const char * pattern, int subid, int flags);
int _mw_ipcsend_subscribe (const char * pattern, int subid, int flags);
int _mw_ipc_unsubscribe(int subid);
int _mw_ipcsend_unsubscribe (int subid);
int _mw_ipcsend_event(const char * event, const char * data, size_t datalen, const char * username,const  char * clientname, MWID fromid, int remoteflag);
int _mw_ipc_getevent(Event * ev);

/* additionsl for servers */
int _mw_ipcdorequest(void);
int _mw_ipcreply(mwhandle_t handle, const char * data, size_t len, int flags);
int _mw_ipcforward(const char * servicename, const char * data, size_t len, int flags);

/* additional administrative for mwd */

int _mw_ipcblock(SERVERID srvid, SERVICEID svcid);
int _mw_ipcunblock(SERVERID srvid, SERVICEID svcid);

int _mwCurrentMessageQueueLength(void);

int _mw_shutdown_mwd(int delay);

/* for debugging */
void  _mw_dumpmesg(void * mesg);

#endif /* IPCMESSAGES_H */
