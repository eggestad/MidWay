.\" .ig
.\".pl 10.0i
.\".po 0
.\".ll 7.2i
.\".lt 7.2i
.\".nr LL 7.2i
.\".nr LT 7.2i
.\" ..
.ds LF Eggestad
.ds RF PUTFFHERE[Page %]
.ds CF
.ds LH RFC nnnn
.ds RH 1 April 2000
.ds CH Service Request Broker Protocol
.hy 0
.ad l
.in 0
Network Working Group                                     Terje Eggestad
Request for Comments: nnnn                                          NONE
                                                           22 June  2000


.ce
Service Request Broker Protocol Version 0.9

.ti 0
Status of this Memo

.fi
.in 3
This memo describes a Service Request Broker (SRB) protocol, which is 
implemented by MidWay.  This memo is under development and MUST be
regarded as an experimental, not recommended standard.  Distribution
of this memo is unlimited.

.ti 0
Overview and Rational
 
This is the description of the TCP/IP protocol.

Why not IIOP? K.I.S.S.! I wanted somthing simpler.

ISO OSI defined a protocol called OSITP, that should implement any of
the same features, but you have to buy the spec. and I be dammed if
I'mm pay for it!

keywords are to be interpreted as described in RFC 2119.

.ti 0
Discussion

We use the same protocol for gateways and clients, but some parameters
are different on some commands.

This protocol is loosely based on http[1] and URI[2], or is inspired by
these, depending of your point of view.


We require a reliable connection to implement the protocol over.
It may or may not be a a octet stream (TCP), but it must be in sequence (!?!)

Service calls shall only be passed in the same direction as the TCP
sessions was established, unless calling side passes REVERSE=YES in SRB INIT.

.ti 1
.ti 0
Glossary

SRB - Service Request Broker.

MidWay - is an implementation of a Service Request Broker

Instance - Really a consept from MidWay, a manager that manages a set
of servers that provides services in a specific domain.

Domain - THe whole purpose of the SRB protocol is to access and share
services among instances. Instead of configuring import/exports of
services manually, instances can be made a part of one or more
domains. All the interconnect and import/export of instances and their
services happen automatically. This is in parallell to the domain
concept in NIS(aka Yellowpages).

Service - A service is semantically very similar to a function or
procedure in programming languages. A service has a name, and is
callable. It takes optionally an octet buffer as an argumnet and
optionally return an octet buffer along with mandatory return code,
signaling success or failure. 

Import - The process of making a service in a remote instance
available in the local instance. Thisis done thru gateways. 

Export - The process of making a service in the local instance
available in remote instances. This is done thru gateways. 

Broker - a daemon on a node that keep track of all the instances on a
node, and can mediate the connection between clients and a gateway..
It is however not required for operation, if the instance is listening
on it own TCP port, and the client know of the port.

Gateway(GW) - a daemon running as a part of an instance, and provide
an instance with the ability to accept network clients. The GW can
also make an instance a member of a domain, and connects all other
GW's in the domain, and provides it's instance services to all the
other GW, thus making a service available in other instances.


NIS - Network information system (RFC: ?)

MTU - Maximun Transfer Unit

XDM - X Display Manager

srb message - 

.ti 0 
Instances

Each instance need to have an unique instance name of the form
ipaddr:port or ipaddr/ipckey. This field is limited to 64 octets, the
numerical IP address shall be used. The IPv6 address shall be used in its
[abcd:...] form, IPv4 in aaa.bbb.ccc.ddd. 

NB: We assume that it's not possible for two hosts where one is using
the IPv4 mapped IPv4 address and the other is using the same IPv4
address. AN instance must pick either.

The 64 octet limit may cause problem with POSIX ipc paths instead of
syv ipc keys.

.ti 0
Domains 

The consept of domain is introduced in order to have running instances
on diffrent nodes that are either providing the same services, or are
interconnected. This is similar to the consept of domain in NIS. This
allows a client to ask for any node providing a gived domain, and
attach it self to one. This is done in a similar manner to X11's XDM,
sending an UDP packet to a given port.  The UDP packet shall contain a
SRB READY
.I request
with the requested domain. All brokers that has an instance with the
given domain respond with UDP packet(s) containing SRB READY
responds(s). Weither the broker keep one reply per packet, or bundles
them in UDP packets is optional.

The broker shall handle multicast packets on address 225.0.0.100.



                                                                    x
        +------+                                +------+            x
        |client|                                |client|            x 
        +------+                                +------+            x
           |                                       |                x
           |                                       |                x
           |             +---------------------+   |                x
           |             |                     |   |                x
          +------+       |                   +------+            x
          |broker|       |                   |broker|            x
          +------+       |                   +------+            x
           |             |                     |   |                x
           |   +---------+                     |   |                x
           |   |                               |   |                x
         +-------+                           +-------+           x
         |gateway|                           |gateway|           x
         +-------+                           +-------+           x
                                                                    x
                                                                    x



.ti 0 
Error handling

If a srb message is so malformated that it can't even be parsed, it shall
be rejected with a REJECT message, signifying at what byte offset into the 
message the error occured, and the reason. In the SRB INIT message, the
parties may instruct the other side to suppress these messages, and just
silently ignore them.

If a srb message misses a field, or has an illegal value the message
shall be returned with a reject marker '~', and the field CAUSEFIELD
and REASONCODE must be set.  If the value of the field was the source
of the error a CAUSEVALUE must be set.  An optional field REASON may
be set, containing a free text describing the cause.

.ti 0
Discovery 

During the discovery using UDP multicast or unicasts, it is the src
address in the reply that shall be used to contact the broker, NOT the
address in the instancename. This will cause problems with multi-homed
hosts with firewalling capability, or multi-homed hosts without
forwarding/routing capability. The instance will normally pick the ip
address from it's first interface.


.ti 0
Large Amount of Data

In the case where a service call or event send a lot of data, it must
be chunked, or it will interfere with the transmission time of other
calls. Just imagine a 10 Mbyte data block. An implementation should
make sure that SVCDATA messages are interleaved with other traffic.

I've specified that a DATA field shall not exceed 3000 token, which
may be as long as 9000 octets, if every token is url escaped. With the
other fields in SVCCALL leaves a max message size of about
10000. Since many TCP implementation has a send/receive queue of 16k,
we don't want to get near that.

Then the DATA to to big to send complete with the SVCCALL message, the
DATACHUNKS filed shall be set, and its value shall be the number of
SVCDATA messages that will follow. The SVCDATA must come in sequence,
and eah will also have the DATACHUNKS field set, and the last one
shall have it set to 0.

Alert readers may realize that requiring to know the number of chunks
prior to the SVCCALL message prevents starting to send anything before
the call data is complete. It is tempting to do this in order to start
the server early. However this is a pitfall, by doing so the server
may be standing idle while waiting for more data, and you are in
reality hoging the server. It is a major effect of an SRB to prevent
this type of behavior, and the SRB would just queue up the data until
complete anyway.  Allowing early send, just uses memory on the server,
who's memory is more precious that the clients. It also increases the
risk of memory leak on the server.  Note that there are no provision
in the protocol to recall messages, again deliberatlu so.  Consider
what happens is the client encounters an error after sending only
partly the data. SVCDATA messages exist only as a mean to interleave
large data with other traffic.

It is recommended that an implementation do a MTU discovery on the
interface for TCP, and select a datachunk size (in bytes) that ensures
that the message fits in a single IP packet. That will ensures that
the message is not just to big and causes the TCP stack to chop it
into a large and a very small packet. This is only a performance
issue.

.ti 1
Handles
A handle is a reference number used in order to link messages
together, such as with SVCCALL and SVCDATA. In the case of gateways,
where a SVCCALL is received from one gateway and retransmitted to
another, a gateway may replace the handle. There is a possibility that
a gateway could receive two SVCCALL from two different gateways or
clients with the same handle.  The gateway would then send two
SVCCALLs with identical handles to a next gateway (or internally to a
server)..


A general message is described by the ABNF spec:

.TS
center, tab(;);
l l
l l.
message =; command marker 
; [parameter 
;  [ *parameterseparator 
;    *parameter]] 
;CRLF
command =; "SRB INIT" 
;| "SRB READY" 
;| "SVCCALL" 
;| "SVCDATA" 
;| "TERM" 
;| "PROVIDE" 
;| "UNPROVIDE"
;| "HELLO"
;| "EVENT"
;| "SUBSCRIBE"
;| "UNBSCRIBE"
;| "REJECT"
marker = ;requestmarker 
;| responsemarker 
;| notificationmarker
;| rejectmarker
requestmarker =; "?"
responsemarker =; "."
notificationmarker =; "!"
rejectmarker =; "~"
parameter =; parametername "=" [ parametervalue ]
parameterseparator =; "&"
parametername =; "AGENT"
;| "AGENTVERSION"
;| "APPLICATIONRC"
;| "AUTHENTICATION"
;| "CAUSE"
;| "CAUSEFIELD"
;| "CAUSEVALUE"
;| "CLIENTNAME"
;| "CONVERSATIONAL"
;| "DATA"
;| "DATACHUNKS"
;| "DOMAIN"
;| "EVENTNAMEEVENTID"
;| "GLOBALTRANID"
;| "GRACE"
;| "HANDLE"
;| "HOPS"
;| "INSTANCE"
;| "LOCALTIME"
;| "MAXHOPS"
;| "MESSAGE"
;| "MORE"
;| "MULTIPLE"
;| "NAME"
;| "OFFSET"
;| "OS"
;| "PASSWORD"
;| "REASON"
;| "REASONCODE"
;| "REJECTS"
;| "REMOTETIME"
;| "RETURNCODE"
;| "REVERSE"
;| "SECTOLIVE"
;| "SVCNAME"
;| "TYPE"
;| "UNIQUE"
;| "USER"
;| "VERSION"
parametervalue =; 1*token
token =; ALPHA | DIGIT | escapedoctet
escapedoctet =; "%" 2HEXDIG
.TE
.fi
.in 3

Legal parameter names and values are dependent on command and given in
separate tables.

parameters not described here are sliently ignored.


.ti 1
SRB INIT 
Request/Response ?.

Must be the first message from conneting side, is either declined or
accepted.  Note that a decline is different than a reject. The cause
field must not be confused with the field used with rejects.

We're a bit different uses if this, depending on the connecting side
is a client or gateway. When connection GW to GW the connceting side
MUST set DOMAIN, PEERDOMAIN, and INSTANCE. GW to GW connections are
also expected to go thru the broker. If there are more instances for
the same domain on a host the INSTANCE field enable the broker to
forward the connection to the right GW. If an instance is apart of
more than one domain, they will still have the same instance name, and
the broker need the DOMAIN field to forward the connection to the
right GW.

The PEERDOMAIN is the domain the connecting GW is apart of, and is
needed by the connected gateway to correctly do authention, and
import/exports.


.TS
box, tab(;);
l | l | l | l | l |
-----
l | l | l | l | l |.
Parameter name;value ABNF;req;rpy;comment
VERSION;1*DIGIT "%2E" 1*DIGIT;m;;"1%2E0" (1.0) 
DOMAIN;1*64token;o;;
PEERDOMAIN;1*64token;o;; In case of GW to GW only.
INSTANCE;1*64token;o;;
TYPE;"client"|"gateway";m; 
NAME;1*64token;m;;Client name if 
;;;;type = client, 
;;;;instancename if 
;;;;type = gateway
REVERSE;"yes"|"NO";o;;Allow reverse 
;;;;calls
REJECTS;"YES"|"no";o;;No error handling
AUTHENTICATION;"NONE"|"PASSWORD"|"X509";o;;
ENCRYPTION;"NONE"|"SSLv3"|"TLSv1";o;;
USER;1*token;;;if auth!=NONE
PASS;1*token;;;
AGENT;1*token;oi;; e.g. "MidWay"
AGENTVERSION;1*token;oi;;
OS;1*token;oi;
RETURNCODE;1*3digit;n/a;m   
CAUSE;1*token;n/a;oi;the cause RC!=0  
.TE

.ti 2
Authentication

If AUTH is set to PASSWORD a normal user/password must be set in their
respective fields. The exeception being is ENCRYPTION is set also, se
below. 

.ti 2
Encryption

If the ENCRYPTION field is set by the initiating side, the server side
may reject it by replying ENCRYPT-PROTOCOL-NOT-SUPPORTED, at which the
client may try again with another encryption protocol or try without
encryption. X509 authentication is only supported with encryption.

NOTE: if AUTHENTICATION is set to PASSWORD and ENCRYPTION is set on,
the PASSWORD shall NOT be set! For the very simple reason that the
password would be in plain text. The SRB INIT reply will have
RETURNCODE=RETRY, and the SRB INIT shall be resent with the PASSWORD
set AFTER SSL/TLS handshakes so that the session is encrypted.

Immediately after the SRB INIT reply the SSL, or TLS handshakes take
place, and the normal SRB protocol resumes after SSL/TLS success.

If the SSL/TLS handshakes fail, the connection is broken.

.ti 1
SRB READY
event !, Request/Response ?.

Events are only used by a SRB server/broker, connected side, which
SHALL write this message out at the time the TCP connection is
established.

The request/resonses are ONLY used by multicast discovery queries and,
their responses. The MC request are listen for by the broker, and the
broker SHALL send one UDP packet for each instance that matches the
domain/instance fields in the request. Request MAY be send to unicast
addresses, but must still be a UDP packet to the SRB port.

.TS
box, tab(;);
l | l | l | l | l |
-----
l | l | l | l | l |.
Parameter name;value ABNF;req;rpy;comment
VERSION;DIGIT "%2E" DIGIT;m;;"1.0" 
DOMAIN;1*64token;o;;
INSTANCE;1*64token;o;;
AGENT;1*64token;oi;;e.g. "MidWay"         
AGENTVERSION;1*64token;oi;;
.TE


.ti 1
TERM
Request/Response ?.

Implicit UNPROVIDE for everything. Called side replies when alle call
request are accounted for.  (!!)

.TS
box, tab(;);
l | l | l | l | l |
-----
l | l | l | l | l |.
Parameter name;value ABNF;req;rpy;comment
GRACE;1*DIGIT;m;;
.TE

.ti 1
PROVIDE
event !

Shall not be sent to clients. This is sent from one gateway to another
to notify peer about a service available in the gateway. After the
peer may send service requests to this service. SERVICE and INSTANCE
form an unique touple. Repeat is legal and shall be ignored. A PROVIDE
is revoked with UNPROVIDE. Gateways shall distribute service names
imported from other gateways but only the shortest path to that
service.

.TS
box, tab(;);
l | l | l | l | l |
-----
l | l | l | l | l |.
Parameter name;value ABNF;req;rpy;comment
SVCNAME;1*32token;m;;
INSTANCE;1*64token;m;;See **
HOPS;1*9DIGIT;m;;
CONVERSATIONAL;"yes""NO";o;;Reserved Future use
DOMAIN;1*64token;o;;(??)
UNIQUE;"yes"|"NO";o;;
.TE

.ti 1
UNPROVIDE
event !

same as PROVIDE


.ti 1
HELLO  
Request/Response ?.

Used as a ping, shall not be be sent to clients.

.TS
box, tab(;);
l | l | l | l | l |
-----
l | l | l | l | l |.
Parameter name;value ABNF;req;rpy;comment
INSTANCE;1*64token;m;;See **
DOMAIN;1*64token;o;;
LOCALTIME;1*DIGIT "." 6DIGIT;r;;
REMOTETIME;1*DIGIT "." 6DIGIT;o;r;
.TE



.ti 1
SVCCALL
Request/Response/event ?.! 

If the Event marker is used, it is a flag that signal that the client
don't want a reply, not even an acknowledge that the call was even
processed. In this case the handle field is ignored and may be ommited.

.TS
box, tab(;);
l | l | l | l | l |
-----
l | l | l | l | l |.
Parameter name;value ABNF;req;rpy;comment
SVCNAME;1*32token;m;m;
DATA;1*3000token;o;o;
DATACHUNKS;1*DIGIT;o;o;See large 
;;;;amount of data.
INSTANCE;1*64token;m;m;See **
DOMAIN;1*64token;o;n/a;
CLIENTNAME;1*64token;oi;n/a;
HANDLE;8HEXDIG;o;m;
MULTIPLE;"yes"|"NO";o;n/a;
MORE;"yes"|"NO";n/a;o;Used only if 
;;;;MULTIPLE was set 
;;;;and server may 
;;;;execute multiple 
;;;;mwreply().
HOPS;1*10DIGIT;m;m;Used to prevent 
;;;;looping
MAXHOPS;1*10DIGIT;o;n/a;---- || ----
SECTOLIVE;1*10DIGIT;o;o;dft 0 (infinite)
GLOBALTRANID;1*DIGIT;o;o;Reserver for 
;;;;future use (XA)
RETURNCODE;3DIGIT;n/a;m;
APPLICATIONRC;1*10DIGIT;n/a;m;
.TE


.ti 1
SVCDATA
event !
used in the case DATACHUNKS is set in SVCCALL, this is the way we 
send large amount of data.

.TS
box, tab(;);
l | l | l | l | l |
-----
l | l | l | l | l |.
Parameter name;value ABNF;req;rpy;comment
SVCNAME;1*32token;m;m;
DATA;1*3000token;o;o;
DATACHUNKS;1*DIGIT;o;o;See large 
;;;;amount of data.
INSTANCE;1*64token;m;m;See **
HANDLE;8HEXDIG;o;m;optional only if
;;;;NOREPLY=YES
.TE


.ti 1
SUBSCRIBE
event !
Subscribe to an event with given regexp, shall not be sent to clients.

.TS
box, tab(;);
l | l | l | l | l |
-----
l | l | l | l | l |.
Parameter name;value ABNF;req;rpy;comment
NAME;1*token;m;;regexp for a event name
DOMAIN;1*64token;o;;from which domain
.TE

.ti 1
UNSUBSCRIBE
event!

same as SUBSCRIBE



.ti 1
EVENT  
event !

.TS
box, tab(;);
l | l | l | l | l |
-----
l | l | l | l | l |.
Parameter name;value ABNF;req;rpy;comment
NAME;1*64token;m;;
INSTANCE;1*64token;m;;See **
EVENTID;8HEXDIG;m;;
DATA;1*3000token;o;;
DOMAIN;1*64token;o;;(??)
GROUP;1*64token;o;;
USER;1*64token;o;;
.TE


.ti 1
REJECT
event !

If there is a message that is completely unreadable, sendit back
completey and complain.  Unless REJECT=NO in SRB INIT.

.TS
box, tab(;);
l | l | l | l | l |
-----
l | l | l | l | l |.
Parameter name;value ABNF;req;rpy;comment
REASONCODE;3DIGIT;m;;
REASON;1*64token;oi;;
OFFSET;*DIGIT;m;;must come before MESSAGE
MESSAGE;1*1000token;m;;all or just beginning?? 
;;;;(offset + 10?)
CAUSEFIELD;1*token;o;;
CAUSEVALUE;1*token;o;;
.TE


.ti 0
Security Considerations

The security considerations can be divided into two categories. On is
authentication which is discussed in a separate section.

The other is the fact that we are asking a machine to execute a piece
of one on our behalf. In fact, every protcol is designed to do pretty
much just that, but the SRB protcol, and the consept of a SRB is
designed to do this as safely as possible.

The protocol itself has no length headers, shall not accept binary
data, even if it causes the implemenentations to be less than optiomal
efficient.

The SRB consept ensures that only a minimal set of code actually used
in processing a request, and that it is not normally done as the
superuser, nor do it require any change of userid when executing.

.ti 0
Full Copyright Statement

Copyright (C) The Internet Society (2000).  All Rights Reserved.

This document and translations of it may be copied and furnished to
others, and derivative works that comment on or otherwise explain it
or assist in its implmentation may be prepared, copied, published and
distributed, in whole or in part, without restriction of any kind,
provided that the above copyright notice and this paragraph are
included on all such copies and derivative works.  However, this
document itself may not be modified in any way, such as by removing
the copyright notice or references to the Internet Society or other
Internet organizations, except as needed for the purpose of developing
Internet standards in which case the procedures for copyrights defined
in the Internet Standards process must be followed, or as required to
translate it into languages other than English.

The limited permissions granted above are perpetual and will not be
revoked by the Internet Society or its successors or assigns.

This document and the information contained herein is provided on an
"AS IS" basis and THE INTERNET SOCIETY AND THE INTERNET ENGINEERING
TASK FORCE DISCLAIMS ALL WARRANTIES, EXPRESS OR IMPLIED, INCLUDING BUT
NOT LIMITED TO ANY WARRANTY THAT THE USE OF THE INFORMATION HEREIN
WILL NOT INFRINGE ANY RIGHTS OR ANY IMPLIED WARRANTIES OF
MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE."

.ti 0
References

[1] RFC 2616

[2] RFC 2396

.ti 0
Author's Address


