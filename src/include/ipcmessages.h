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
#include <sys/types.h>

/* the message numbers are inspired by ISO8583, it seemed fitting ;) */

#define ATTACHREQ 0x0800
#define ATTACHRPL 0x0810
#define DETACHREQ 0x0820
#define DETACHRPL 0x0830

#define MWIPCCLIENT   0x01
#define MWIPCSERVER   0x02
#define MWIPCGATEWAY  0x04
#define MWNETCLIENT   0x10

/* flags used in messages */
#define MWFORCE 0x10
#define MWGATEWAYCLIENT  0x100
#define MWGATEWAYSERVICE 0x200

/* pid is for informational use only, in future threaded client/servers
 * pid tends to be diffrent for each thread. For example on 
 * linux where the clone(2) sys call is used. 
 */
struct attach
{
  long mtype; /* ATTACH & DETACH */

  int ipcqid;
  pid_t pid;

  int server; /* true/false */
  char srvname[MWMAXNAMELEN];
  SERVERID srvid;

  int client; /* true/false */
  char cltname[MWMAXNAMELEN];
  CLIENTID cltid;

  GATEWAYID gwid;

  int flags;
  int returncode;
};

typedef struct attach Attach;

#define PROVIDEREQ   0x0600
#define PROVIDERPL   0x0610
#define UNPROVIDEREQ 0x0620
#define UNPROVIDERPL 0x0630

struct provide
{
  long mtype; 
  
  SERVERID srvid;
  SERVICEID svcid;
  GATEWAYID gwid;

  char svcname[MWMAXSVCNAME];

  int cost;
  int flags;
  int returncode;
};

typedef struct provide Provide;

/* I'm unlikely to use SVCFORWARD on sysvipc. 
   The reason is that a server waiting for something to do don't care
   if it is a call or forward. Maybe we should have a opcode in the messages. 
   The key is that a server when started have only two states it can be in where it 
   awaits a message. Idle where it waits for a SVCCALL, and callwait (mwfetch()) where
   it waits for a SVCREPLY. 

   In Future versions supporting XA style transactions, this may have to change in 
   some way. 
   However also in future versions where we have POSIX IPC, messages don't have type id.
*/

#define SVCCALL    0x0100
#define SVCFORWARD 0x0101 
#define SVCREPLY   0x0110

struct call
{
  long mtype; 
  int handle; 
 
  CLIENTID cltid;
  GATEWAYID gwid;
  SERVERID srvid;
  SERVICEID svcid;

  int forwardcount;

  char service[MWMAXSVCNAME];
  char origservice[MWMAXSVCNAME];
  
  time_t issued;
  int uissued;
  int timeout; /* millisec */

  int data;
  int datalen;
  int appreturncode;

  int flags;

  char domainname[MWMAXNAMELEN];

  int returncode;
};

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

  int data;
  int datalen;

  int flags;

  CONVID convid;
  int returncode;
};

typedef struct conversation  Converse;;


/* administrative  request/notifications from mwd to  members and from
   mwadm  to mwd.  Used  to notify  shutdown event,  Should this  be a
   special event instead in the general event service planned....?  */

#define ADMREQ 0x1000
/* opcodes */
#define ADMSHUTDOWN  0x10
#define ADMSTARTSRV  0x20
#define ADMSTOPSRV   0x21
#define ADMRELOADSRV 0x22

typedef struct {
  long mtype; 
  int opcode; 
  CLIENTID cltid;
  int delay;
} Administrative;


/* the event  message. used to both subscribe  and unsubscribe as well
   as the events them selves */

#define EVENT                0x400
#define EVENTACK             0x410
#define EVENTSUBSCRIBEREQ    0x420
#define EVENTSUBSCRIBERPL    0x430
#define EVENTUNSUBSCRIBEREQ  0x440
#define EVENTUNSUBSCRIBERPL  0x450

/* in the "normal"  case of an event, the event can't  be "". THe data
is  however  optional,   and  the  user  and  group   is  optional  as
well.  (first byte is  \0 if  empty.  flag  is not  used. The  data if
passed is now owned by mwd, and the event sender must not free it. THe
mwd will  let it be and send  a event message to  all subscribers, all
pointing to the  same date buffer. THrefore recipient  of event my ack
then, while the mwd do not ack to the original sender. */

/* In the case of subscribe  or unsubscribe, A string, glo,b or regexp
is passed as the event that the mwd will use to match any event to any
event.  if  the event is empty, the  mathc string must be  passed in e
data buffer,  and data may not be  0. Since regexp may  be quite huge,
the event field  may be too small. The flag  must be set appropriately
if the match string is not a string but a glob or regexp. */


struct event {
  long mtype;
  
  char event[MWMAXNAMELEN];
  int eventid;
  int subscriptionid;
  int senderid;
  
  int data;
  int datalen;
  
  char username[MWMAXNAMELEN];
  char clientname[MWMAXNAMELEN];
  
  int flags;
  int returncode;
};

typedef struct event Event;
  
/* Here we  calculate  MWMSGMAX. */
typedef union { 
  Attach at;
  Provide prov;
  Call call;
  Converse conv;
  Administrative admin;
  Event ev;
} _union_ipc_messages;

#define MWMSGMAX sizeof(_union_ipc_messages)


/*
 *  lowlevel ipcmsg send & receive
 */
int _mw_ipc_putmessage(int mwid, char *data, int len,  int flags);
int _mw_ipc_getmessage(char * data, int *len, int type, int flags);

/*
 * functions used by client requests.
 */
int _mw_ipcsend_attach(int att_type, char * name, int flags);
int _mw_ipcsend_detach(int force);
int _mw_ipcsend_detach_indirect(CLIENTID cid, SERVERID sid, int force);

int _mw_ipcsend_provide(char * servicename, int cost, int flags);
SERVICEID _mw_ipc_provide(char * servicename, int flags);
int _mw_ipcsend_unprovide(char * servicename, int flags);
int _mw_ipc_unprovide(char * servicename,  SERVICEID svcid);

int _mwacallipc (char * svcname, char * data, int datalen, int flags);
int _mwfetchipc (int handle, char ** data, int * len, int * appreturncode, int flags);

int _mw_ipcconnect(char * servicename, char * socketpath, int flags);
int _mw_ipcdisconnect(int fd);

/* event API */
int _mw_ipc_subscribe(char * pattern, int subid, int flags);
int _mw_ipcsend_subscribe (char * pattern, int subid, int flags);
int _mw_ipc_unsubscribe(int subid);
int _mw_ipcsend_unsubscribe (int subid);
int _mw_ipcsend_event(char * event, char * data, int datalen, char * username, char * clientname);
int _mw_ipc_getevent(Event * ev);

/* additionsl for servers */
int _mw_ipcdorequest(void);
int _mw_ipcreply(int handle, char * data, int len, int flags);
int _mw_ipcforward(char * servicename, char * data, int len, int flags);

/* additional administrative for mwd */

int _mw_ipcblock(SERVERID srvid, SERVICEID svcid);
int _mw_ipcunblock(SERVERID srvid, SERVICEID svcid);

int _mwCurrentMessageQueueLength(void);

int _mw_shutdown_mwd(int delay);

/* for debugging */
void  _mw_dumpmesg(void * mesg);

#endif /* IPCMESSAGES_H */
