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

#ifndef _SRBPROTOCOL_H
#define _SRBPROTOCOL_H

#include <connection.h>
#include <stdio.h>
#include <urlencode.h>

#define SRBPROTOCOLVERSION 	"0.9"

#define SRBMESSAGEMAXLEN 	3500

#ifdef DEBUG
#define SRB_BROKER_PORT         1102
#else 
#define SRB_BROKER_PORT         102
#endif

#define SRB_DEFAULT_PORT 	11000

/*************************************************
 * SRB commands
 *************************************************/
#define SRB_INIT	"SRB INIT"
#define SRB_READY 	"SRB READY"
#define SRB_SVCCALL 	"SVCCALL"  
#define SRB_SVCDATA 	"SVCDATA"  
#define SRB_TERM 	"TERM"  
#define SRB_PROVIDE 	"PROVIDE"  
#define SRB_UNPROVIDE 	"UNPROVIDE"
#define SRB_HELLO 	"HELLO"
#define SRB_EVENT 	"EVENT"
#define SRB_SUBSCRIBE 	"SUBSCRIBE"
#define SRB_UNSUBSCRIBE "UNSUBSCRIBE"
#define SRB_REJECT 	"REJECT"

/*************************************************
 * SRB command markers
 *************************************************/
#define SRB_REQUESTMARKER 	'?'
#define SRB_RESPONSEMARKER 	'.'
#define SRB_NOTIFICATIONMARKER 	'!'
#define SRB_REJECTMARKER 	'~'


/*************************************************
 * fleld names 
 *************************************************/
#define SRB_AGENT 		"AGENT"
#define SRB_AGENTVERSION 	"AGENTVERSION"
#define SRB_APPLICATIONRC 	"APPLICATIONRC"
#define SRB_AUTHENTICATION 	"AUTHENTICATION"
#define SRB_CAUSE 		"CAUSE"
#define SRB_CAUSEFIELD 		"CAUSEFIELD"
#define SRB_CAUSEVALUE 		"CAUSEVALUE"
#define SRB_CLIENTNAME 		"CLIENTNAME"
#define SRB_CALLERID 		"CALLERID"
#define SRB_CONVERSATIONAL	"CONVERSATIONAL"
#define SRB_COST 		"COST"
#define SRB_DATA 		"DATA"
#define SRB_DATACHUNKS 		"DATACHUNKS"
#define SRB_DOMAIN 		"DOMAIN"
#define SRB_EVENTNAMEEVENTID 	"EVENTNAMEEVENTID"
#define SRB_GLOBALTRANID 	"GLOBALTRANID"
#define SRB_GRACE 		"GRACE"
#define SRB_HANDLE 		"HANDLE"
#define SRB_HOPS 		"HOPS"
#define SRB_INSTANCE 		"INSTANCE"
#define SRB_LOCALTIME 		"LOCALTIME"
#define SRB_MAXHOPS 		"MAXHOPS"
#define SRB_MESSAGE 		"MESSAGE"
#define SRB_MORE 		"MORE"
#define SRB_MULTIPLE 		"MULTIPLE"
#define SRB_NAME 		"NAME"
#define SRB_NOREPLY 		"NOREPLY"
#define SRB_OFFSET 		"OFFSET"
#define SRB_OS 			"OS"
#define SRB_PASSWORD 		"PASSWORD"
#define SRB_PEERDOMAIN 		"PEERDOMAIN"
#define SRB_REASON 		"REASON"
#define SRB_REASONCODE 		"REASONCODE"
#define SRB_REJECTS 		"REJECTS"
#define SRB_REMOTETIME 		"REMOTETIME"
#define SRB_RETURNCODE 		"RETURNCODE"
#define SRB_REVERSE 		"REVERSE"
#define SRB_SECTOLIVE 		"SECTOLIVE"
#define SRB_SVCNAME 		"SVCNAME"
#define SRB_TYPE 		"TYPE"
#define SRB_UNIQUE 		"UNIQUE"
#define SRB_USER 		"USER"
#define SRB_VERSION 		"VERSION"


/*************************************************
 * fleld values 
 *************************************************/

#define SRB_TYPE_CLIENT			"CLIENT"
#define SRB_TYPE_GATEWAY		"GATEWAY"
/* authentication schemes. */
#define SRB_AUTH_NONE			"NONE"
#define SRB_AUTH_PASS			"PASS"
#define SRB_AUTH_X509			"x509"

/* YES and NO, almost an overkill */
#define SRB_YES   			"YES"
#define SRB_NO  			"NO"

/* roles */
#define SRB_ROLE_CLIENT   		1
#define SRB_ROLE_GATEWAY  		2


/*************************************************
 * Return codes 
 * 000 ok
 * 100 series
 * 200 series 
 * 300 series are warings that causes message to be ignored, such as duplicates
 * 400 series are errors that cause the message to be rejected.
 * 500 series are errors that cause the connection to be terminated
 *************************************************/

#define SRB_PROTO_OK			0
#define SRB_PROTO_ISCONNECTED		301
#define SRB_PROTO_UNEXPECTED		302
#define SRB_PROTO_FORMAT		400
#define SRB_PROTO_FIELDMISSING		401
#define SRB_PROTO_ILLEGALVALUE		402
#define SRB_PROTO_CLIENT		403
#define SRB_PROTO_NOREVERSE		404
#define SRB_PROTO_AUTHREQUIRED		405
#define SRB_PROTO_AUTHFAILED		406
#define SRB_PROTO_VERNOTSUPP		410
#define SRB_PROTO_NOINIT		411
#define SRB_PROTO_NOTNOTIFICATION	412
#define SRB_PROTO_NOTREQUEST		413
#define SRB_PROTO_NO_SUCH_SERVICE	430
#define SRB_PROTO_SERVICE_FAILED	431
#define SRB_PROTO_NOGATEWAY_AVAILABLE	440
#define SRB_PROTO_GATEWAY_ERROR		500
#define SRB_PROTO_DISCONNECTED		501
#define SRB_PROTO_NOCLIENTS		520
#define SRB_PROTO_NOGATEWAYS		521


#define SRB_PROTO_			1

/* a struct used internally to hold a decoded message */
typedef struct {
   char command[32];
   char marker;
   urlmap * map;
   int payloadlen;
   char * payload;
} SRBmessage;


#ifndef _SRB_CLIENT_C
/*extern char _mw_srbmessagebuffer;*/
#endif

/* these functions deal vith manipulation of SRBmessages */
void _mw_srb_init (SRBmessage * srbmsg, char * command, char marker, ...);
SRBmessage * _mw_srb_create (char * command, char marker, ...);
void _mw_srb_destroy (SRBmessage * srbmsg); 

void _mw_srb_setfield   (SRBmessage * srbmsg, char * key, char * value); 
void _mw_srb_nsetfield  (SRBmessage * srbmsg, char * key, void * value, int vlen); 
void _mw_srb_setfieldi  (SRBmessage * srbmsg, char * key, int value); 
void _mw_srb_setfieldx  (SRBmessage * srbmsg, char * key, unsigned int value); 

char * _mw_srb_getfield (SRBmessage * srbmsg, char * key);
void _mw_srb_delfield   (SRBmessage * srbmsg, char * key); 

/* encode decode */
SRBmessage * _mw_srbdecodemessage(Connection * conn, char * message);
SRBmessage * _mw_srb_recvmessage(Connection * conn, int flags);

int _mw_srbsendmessage(Connection * conn, SRBmessage * srbmsg);
int _mw_srbencodemessage(SRBmessage * srbmsg, char * buffer, int buflen);


int _mw_srbsendreject(Connection * conn, SRBmessage * srbmsg, 
		       char * causefield, char * causevalue, 
		       int rc);
int _mw_srbsendreject_sz(Connection * conn, char *message, int offset) ;
int _mw_srbsendterm(Connection * conn, int grace);
int _mw_srbsendinit(Connection * conn, char * user, char * password, 
		    char * name, char * domain);

int _mw_srbsendcall(Connection * conn, int handle, char * svcname, char * data, int datalen, 
		    int flags);

int _mw_get_returncode(urlmap * map);
int _mw_srb_checksrbcall(Connection * conn, SRBmessage * srbmsg) ;

/* tracing API */
int _mw_srb_traceonfile(FILE * fp);
int _mw_srb_traceon(char * path);
int _mw_srb_traceoff(void );

#define SRB_TRACE_IN 1
#define SRB_TRACE_OUT 0

void _mw_srb_trace(int dir_in, Connection * conn, char * message, int messagelen);


/* conversion func between SRBP error codes, and errno */
char * _mw_srb_reason(int rc);
int _mw_errno2srbrc(int err);



#ifdef DEBUGGING
static inline void dbg_srbprintmap(SRBmessage * srbmsg)
{
  int idx = 0;
  if (mwsetloglevel(-1) >= MWLOG_DEBUG2)  { 
    while(srbmsg->map[idx].key != NULL) {
      DEBUG2("  Field %s(%p) => %s(%p)", 
	     srbmsg->map[idx].key, srbmsg->map[idx].key, srbmsg->map[idx].value, srbmsg->map[idx].value);
      idx++;
    };
  }; 
};
#else 
#define dbg_srbprintmap(x);
#endif

#endif /* _SRBPROTOCOL_H */
