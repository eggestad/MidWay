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

#include <urlencode.h>

#define SRBPROTOCOLVERSION 	"0.9"

#define SRBMESSAGEMAXLEN 	3500

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
#define SRB_CONVERSATIONAL	"CONVERSATIONAL"
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
#define SRB_AUTH_UNIX			"unix"
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
} SRBmessage;


#ifndef _SRB_CLIENT_C
/*extern char _mw_srbmessagebuffer;*/
#endif

SRBmessage * _mw_srbdecodemessage(char * message);
int _mw_srbsendmessage(int fd, SRBmessage * srbmsg);


int _mw_srbsendreject(int fd, SRBmessage * srbmsg, 
		       char * causefield, char * causevalue, 
		       int rc);
int _mw_srbsendreject_sz(int fd, char *message, int offset) ;
int _mw_srbsendterm(int fd, int grace);
int _mw_srbsendinit(int fd, char * user, char * password, 
		    char * name, char * domain);


int _mw_get_returncode(urlmap * map);
int _mw_srb_checksrbcall(int fd, SRBmessage * srbmsg) ;

void _mw_srb_trace(int dir_in, int fd, char * message, int messagelen);

char * _mw_srb_reason(int rc);
int _mw_errno2srbrc(int err);

#endif /* _SRBPROTOCOL_H */
