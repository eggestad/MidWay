/*
  MidWay
  Copyright (C) 2001 Terje Eggestad

  MidWay is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License as
  published by the Free Software Foundation; either version 2 of the
  License, or (at your option) any later version.
  
  MidWay is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.
  
  You should have received a copy of the GNU General Public License 
  along with the MidWay distribution; see the file COPYING.  If not,
  write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA. 
*/

/* 
 * $Log$
 * Revision 1.4  2002/07/07 22:33:41  eggestad
 * We now operate on Connection structs not filedesc.
 *
 * Revision 1.3  2001/10/16 16:18:09  eggestad
 * Fixed for ia64, and 64 bit in general
 *
 * Revision 1.2  2001/10/03 22:55:22  eggestad
 * plugged file desc leak
 *
 * Revision 1.1  2001/09/15 23:40:09  eggestad
 * added the broker daemon
 *
 */

#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>

#include <MidWay.h>
#include <SRBprotocol.h>
#include <multicast.h>
#include <connection.h>

#define _MWBD_C
#include "mwbd.h"
#include "mwbd_sockets.h"

#define AGENT "MidWay broker daemon"

static char * RCSId UNUSED = "$Id$";
static char * RCSName UNUSED = "$Name$"; /* CVS TAG */


/* notes

   This is the broker daemon, it accepts connectsions from both
   gateways and clients and relay the connections from the clients to
   the gateways based on the DOMAIN and INSTANCE fields in the SRB
   INIT.  The relaying is done by unix descriptor passing.

   The broker shall also rely to UDP SRB READY requests for discovery,
   and that includes multicast.

   I'm generally unhappy with the code here, I should've had a layer
   that fetches a complete message from a peer, then have a switch on
   the commmand, and then have seperate funcs dealing with the
   different message commands.

   the trouble is that the code here is pretty small, and the amount
   of data needed to ba passed thru the layers is unconfortable high.  

*/

int debugging = 0;

extern struct fd_info * gw_root, * gw_tail;

/* all function that send and recv SRB messages operate on a
   Connection struct *.  we really only work on fd's in mwbd, so we
   have this pseudo connection for whenever we call _mw_srb* */
static Connection pseudoconn = { 
  fd:            -1, 
  rejects:        1,
  domain:        NULL, 
  version:       0.0, 
  messagebuffer: NULL,
  role:          -1
};  


void Exit(char * reason, int exitcode)
{
  closeall();
  if (reason)
    info("%s", reason);
  exit(exitcode);
};

/* for new tcp connections, we call accept_tcp() who figures out which
   gw that shall gave it, and passes it on, or just rejects it.

   TODO: partiall messages and timeout!
*/

int send_srb_ready_not(int fd)
{
  SRBmessage srbmsg;
  int rc;

  pseudoconn.fd = fd;

  strcpy(srbmsg.command, SRB_READY);
  srbmsg.marker = SRB_NOTIFICATIONMARKER;
  srbmsg.map = NULL;
  srbmsg.map = urlmapadd(srbmsg.map, SRB_VERSION, SRBPROTOCOLVERSION);
  srbmsg.map = urlmapadd(srbmsg.map, SRB_AGENT, AGENT);
  srbmsg.map = urlmapadd(srbmsg.map, SRB_AGENTVERSION, RCSName);
  rc = _mw_srbsendmessage(&pseudoconn, &srbmsg);
  urlmapfree(srbmsg.map);
  return rc; 
};
 
static char buffer[SRBMESSAGEMAXLEN];

void read_from_client(int fd, struct fd_info * cinfo)
{
  int buflen, rc, n;
  struct fd_info * gwinfo;
  SRBmessage * srbmsg = NULL;
  char * domain = NULL, * instance = NULL, ver[8];;

  pseudoconn.fd = fd;

  debug("reading data from client on %d", fd);
  buflen = read(fd, buffer, SRBMESSAGEMAXLEN);
  if (buflen == -1) return;
  if (buflen == SRBMESSAGEMAXLEN) {
    error("received a message exceeding %d bytes, discarding and disconnecting", 
	  SRBMESSAGEMAXLEN);
    _mw_srbsendreject_sz(&pseudoconn, "Message too long", SRBMESSAGEMAXLEN);
    goto clean;
  };
  srbmsg = _mw_srbdecodemessage(buffer);
  if (srbmsg == NULL) {
    error("received an undecodable message");
    _mw_srbsendreject_sz(&pseudoconn, "Expected SRB INIT?....", 0);
    goto clean;
  };
  _mw_srb_trace(SRB_TRACE_IN, &pseudoconn, buffer, buflen);

  cinfo->lastused = time(NULL);

  /* TODO: do partial and multiple messages here (sometime) */

  /* if SRB READY request */
  if ( (strcmp(srbmsg->command, SRB_READY) == 0) && 
       (srbmsg->marker ==  SRB_REQUESTMARKER) ) {
    SRBmessage srbmsg_reply;
    
    debug("processing a SRB READY request");

    _mw_srb_init(&srbmsg_reply, SRB_READY, SRB_RESPONSEMARKER, NULL);


    for (gwinfo = gw_root; gwinfo != NULL; gwinfo = gwinfo->next) {
      sprintf(ver, "%d.%d",   gwinfo->majorversion, gwinfo->minorversion);
      _mw_srb_setfield(&srbmsg_reply, SRB_VERSION, ver);
      _mw_srb_setfield(&srbmsg_reply, SRB_DOMAIN, gwinfo->domain);
      _mw_srb_setfield(&srbmsg_reply, SRB_INSTANCE, gwinfo->instance);
      rc = _mw_srbsendmessage(&pseudoconn, &srbmsg_reply);
    };
    _mw_srb_setfield(&srbmsg_reply, SRB_VERSION, SRBPROTOCOLVERSION);
    _mw_srb_delfield(&srbmsg_reply, SRB_DOMAIN);
    _mw_srb_delfield(&srbmsg_reply, SRB_INSTANCE);
    _mw_srb_setfield(&srbmsg_reply, SRB_AGENT, AGENT);
    _mw_srb_setfield(&srbmsg_reply, SRB_AGENTVERSION, RCSName);
    rc = _mw_srbsendmessage(&pseudoconn, &srbmsg_reply);
    
    return;
  };

  /* the only legal message now is SRB INIT */
  if (strcmp(srbmsg->command, SRB_INIT) != 0) {
    error("received a message with command %s not %s", srbmsg->command, SRB_INIT);
    _mw_srbsendreject(&pseudoconn, srbmsg, NULL, NULL, SRB_PROTO_NOINIT);
    goto clean;
  };
  if (srbmsg->marker !=  SRB_REQUESTMARKER) {
    error("received a message with command %s but not a request", srbmsg->command);
    _mw_srbsendreject(&pseudoconn, srbmsg, NULL, NULL, SRB_PROTO_NOTREQUEST);
    goto clean;
  };
  
  n = urlmapget(srbmsg->map, SRB_DOMAIN);
  if (n != -1) domain = srbmsg->map[n].value;

  n = urlmapget(srbmsg->map, SRB_INSTANCE);
  if (n != -1) instance = srbmsg->map[n].value;

  if ( (instance == NULL)  && (domain == NULL) ){
    error("received a %s message without domain", srbmsg->command);
    _mw_srbsendreject(&pseudoconn, srbmsg, SRB_INSTANCE, NULL, SRB_PROTO_FIELDMISSING);
    goto clean;
  };
  
 retry:    
  /* TODO: need to to version check ??? */
  gwinfo = find_gw(domain, instance);
  
  if (gwinfo == NULL) {
    error("client requested domain %s & instance %s, but not available", 
	  domain?domain:"", instance?instance:"");     
    _mw_srbsendreject(&pseudoconn, srbmsg, NULL, NULL, SRB_PROTO_NOGATEWAY_AVAILABLE); 
    goto clean; // do not disconnect here
  };
  
  pseudoconn.fd = gwinfo->fd;
  _mw_srb_trace(SRB_TRACE_OUT, &pseudoconn, buffer, buflen);

  debug("about to send fd with SRB init to gw");
  // markfd(fd);
  rc = sendfd(fd, gwinfo->fd, buffer, buflen); // must be moved to read_client();
  assert(rc == buflen);
  if (rc != buflen) {
    error("sendfd returned %d excpected %d", rc, buflen);
    goto retry;
  };

  debug("Ok fd passed on cleaning up client");
  /* ok fd is now passwd to gateway, and we're closing. */
  
 clean:

  del_c(fd);
  unmarkfd(fd);
  close(fd);
  if (srbmsg && srbmsg->map) urlmapfree(srbmsg->map);
  free(srbmsg);
  return;
};

/* handles multicast packets from peers and clients who are trying to
   find all gw's */
static void  do_udp_dgram(int sd)
{
  static char ver[8];
  static char buffer[SRBMESSAGEMAXLEN];
  static char addrbuf[100];
  struct sockaddr_in from;
  char * instance, * domain;
  int rc, i;
  socklen_t fromlen;
  struct fd_info * gwinfo;
  SRBmessage srbmsg_reply, * srbmsg_req;

  fromlen = sizeof(struct sockaddr_in);
  rc = recvfrom(sd, buffer, 1024, 0, (struct sockaddr *)&from, &fromlen);
  inet_ntop(AF_INET, &from.sin_addr, addrbuf, 100);
  debug ("from socket %d, we got a message %s sender %s:%d (%d)", 
	sd, buffer, addrbuf , ntohs(from.sin_port), fromlen);

  pseudoconn.fd = sd;    

  _mw_srb_trace(SRB_TRACE_IN, &pseudoconn, buffer, rc);
  
  srbmsg_req = _mw_srbdecodemessage(buffer);
  if (srbmsg_req == NULL) {
    debug("unintelligble messgage, ignoring");
    return;
  };

  if ( (strcmp(srbmsg_req->command, SRB_READY) != 0) ||
       srbmsg_req->marker != SRB_REQUESTMARKER) {
    debug("Illegal message %s%c..., ignoring", srbmsg_req->command, srbmsg_req->marker);
    return;
  };

  i = urlmapget(srbmsg_req->map, SRB_INSTANCE);
  if (i == -1) instance = NULL;
  else instance = srbmsg_req->map[i].value;

  i = urlmapget(srbmsg_req->map, SRB_DOMAIN);
  if (i == -1) domain = NULL;
  else domain = srbmsg_req->map[i].value;

  strcpy(srbmsg_reply.command, SRB_READY);
  srbmsg_reply.marker = SRB_RESPONSEMARKER;
  srbmsg_reply.map = NULL;
  
  srbmsg_reply.map = urlmapadd(srbmsg_reply.map, SRB_VERSION, NULL);
  srbmsg_reply.map = urlmapadd(srbmsg_reply.map, SRB_DOMAIN, NULL);
  srbmsg_reply.map = urlmapadd(srbmsg_reply.map, SRB_INSTANCE, NULL);

  for (gwinfo = gw_root; gwinfo != NULL; gwinfo = gwinfo->next) {

    /* no version checking here, we leave that to the client */
    if (domain && (strcmp(domain, gwinfo->domain) != 0)) continue;
    if (instance && (strcmp(instance, gwinfo->instance) != 0)) continue;

    sprintf(ver, "%d.%d",   gwinfo->majorversion, gwinfo->minorversion);
    urlmapset(srbmsg_reply.map, SRB_VERSION, ver);
    urlmapset(srbmsg_reply.map, SRB_DOMAIN, gwinfo->domain);
    urlmapset(srbmsg_reply.map, SRB_INSTANCE, gwinfo->instance);
    rc = _mw_srbencodemessage(&srbmsg_reply, buffer, SRBMESSAGEMAXLEN);
    if (rc == -1) {
      error("failed to encode SRB READY response for instance %s"
	    "domain %s, version %d.%d: reason %s", 
	    gwinfo->instance, gwinfo->domain, 
	    gwinfo->majorversion, gwinfo->minorversion, strerror(errno));
      continue;
    }

    _mw_srb_trace(SRB_TRACE_OUT, &pseudoconn, buffer, rc);
    inet_ntop(AF_INET, &from.sin_addr, addrbuf, 100);

    debug ("from socket %d, we sending reply to sender %s:%d (%d)", 
	  sd, addrbuf , ntohs(from.sin_port), fromlen);
    rc = sendto(sd, buffer, rc, 0, (struct sockaddr *)&from, fromlen);
    debug("sendto returned %d errno=%d", rc, errno);
  };
  return ;
};

/* do read from gateway side. */
void recv_gw_data(int fd, struct fd_info * gwinfo)
{
  char buffer[SRBMESSAGEMAXLEN], * msg, * msgend;
  int rc, len, inbuffer, n;
  SRBmessage * srbmsg;

  pseudoconn.fd = fd;    
  len = read(fd, buffer, SRBMESSAGEMAXLEN);

  if (len < 0) {
    error ("error while reading data from gw!, errno = %d", errno);
    unmarkfd(fd);
    del_gw(fd);
    close(fd);
    return;
  }

  if (!len) {
    info ("gateway with domain=%s and instance = %s disconnected", 
	  gwinfo->domain, gwinfo->instance);
    del_gw(fd);
    unmarkfd(fd);
    close(fd);
    return;
  }

  gwinfo = find_gw_byfd(fd);
  if (gwinfo == NULL) {
    error ("received data from unknown fd!, Can't happen");
    unmarkfd(fd);
    del_gw(fd);
    close(fd);
    return;
  }
  
  debug("read %d bytes of data", len);
  _mw_srb_trace(SRB_TRACE_IN, &pseudoconn, buffer, len);
  
  /* now we may have prev incomplete data in gwindo, if so append the
     new buffer, and process. If not process from buffer */
  if (gwinfo->incomplete_mesg) {
    len += strlen(gwinfo->incomplete_mesg);
    gwinfo->incomplete_mesg = (char * ) realloc(gwinfo->incomplete_mesg, len);
    strcat (gwinfo->incomplete_mesg, buffer);
    buffer[0] = '\0';
    msg = gwinfo->incomplete_mesg;
    inbuffer = 0;
  } else {
    msg = buffer;
    inbuffer = 1;
  };

  /* now for each complete message, processit. if any remaining stuff,
     copy it and place in .

     Why don't I keep the buffer in the buffer in the gwinfo?  because
     we *might* never end up on \r\n boundries on reads. the resulting
     realloc()s and memmoves would offset all perf gains. What we
     really want is a version of _mw_srbdecodemessage() that take an
     iovec as input. */

  pseudoconn.fd = gwinfo->fd;    

  while(len != 0) {
    debug("beginning parse loop inbuffer=%s len=%d", inbuffer?"yes":"no", len);
    msgend = strstr(msg, "\r\n");
    /* if  incomplete  the remaing go to the incomplete buffer*/
    if (!msgend) {
      debug("no complete message found");
      if (msg == gwinfo->incomplete_mesg) return;
      if (inbuffer) {
	assert(gwinfo->incomplete_mesg == NULL);
	gwinfo->incomplete_mesg = malloc(len);
	assert(gwinfo->incomplete_mesg != NULL);
	memcpy(gwinfo->incomplete_mesg, buffer, len);
	return;
      } else if (msg != gwinfo->incomplete_mesg) 
	memmove(msg, gwinfo->incomplete_mesg, len);
      return;
    };


    srbmsg = _mw_srbdecodemessage(buffer);

    if (srbmsg == NULL) {
      error ("got an incomprehensible message");
      goto close_n_out;
    };

    if ((strcmp(&srbmsg->command[0], SRB_TERM) == 0) &&
	(srbmsg->marker == SRB_NOTIFICATIONMARKER)) {
      info ("Instance %s recalls domain %s, version %d.%d", 
	  gwinfo->instance, gwinfo->domain, 
	  gwinfo->majorversion, gwinfo->minorversion);
      goto close_n_out;
    };
    if ((strcmp(srbmsg->command, SRB_READY) != 0) ||
	(srbmsg->marker != SRB_NOTIFICATIONMARKER)) {
      error("Got an unecpected message from ...");
      _mw_srbsendreject (&pseudoconn, srbmsg, NULL, NULL, SRB_PROTO_UNEXPECTED);
      goto close_n_out;
    };
    
    n = urlmapget(srbmsg->map, SRB_DOMAIN);
    if ( n == -1) {
      error ("domain missing in SRB READY message");
      _mw_srbsendreject (&pseudoconn, srbmsg, SRB_DOMAIN, NULL, SRB_PROTO_FIELDMISSING);
      goto close_n_out;
    };
    strncpy(gwinfo->domain, srbmsg->map[n].value, MWMAXNAMELEN);
    strcat(gwinfo->domain, "");
    n = urlmapget(srbmsg->map, SRB_INSTANCE);
    if ( n == -1) {
      error ("instance missing in SRB READY message");
      _mw_srbsendreject (&pseudoconn, srbmsg, SRB_INSTANCE, NULL, SRB_PROTO_FIELDMISSING);
      goto close_n_out;
    };
    strncpy(gwinfo->instance, srbmsg->map[n].value, MWMAXNAMELEN);
    strcat(gwinfo->domain, "");

    n = urlmapget(srbmsg->map, SRB_VERSION);
    if ( n == -1) {
      error ("version missing in SRB READY message");
      _mw_srbsendreject (&pseudoconn, srbmsg, SRB_VERSION, NULL, SRB_PROTO_FIELDMISSING);
      goto close_n_out;
    };
    rc = sscanf(srbmsg->map[n].value, "%d.%d", 
		&gwinfo->majorversion, 
		&gwinfo->minorversion);
    if (rc != 2) {
      error ("wrong version (%s) in SRB READY message",srbmsg->map[n].value );
      _mw_srbsendreject (&pseudoconn, srbmsg, SRB_VERSION, NULL, SRB_PROTO_ILLEGALVALUE);
      goto close_n_out;
    };
    info ("Instance %s provides domain %s, version %d.%d", 
	  gwinfo->instance, gwinfo->domain, 
	  gwinfo->majorversion, gwinfo->minorversion);
    len -= (long) (msgend -  msg) +2;
    msg = msgend + 2;
  };

  if (gwinfo->incomplete_mesg) {
    free(gwinfo->incomplete_mesg);
    gwinfo->incomplete_mesg = NULL;
  };
  return;

 close_n_out:
  debug("close out on fd %d", fd);
  del_gw(fd);
  unmarkfd(fd);
  close (fd);
  return;     
};


static int startok = -1;
static int going_down = 0;

void sig_usr1(int sig)
{
  startok = 1;
  return;
};

void sig_chld(int sig)
{
  startok = 0;
  return;
};

void sig_term(int sig)
{
  info ("going down on signal %d", sig);
  going_down = 1;
};

int daemonize(void)
{
  pid_t p;

  
  p = fork();

  if (p == -1) {
    error("fork failed, reason %s", strerror(errno));
    exit(1);
  };

  if (p > 0) {
    while(startok == -1) pause();          
    if (startok == 0) exit(2);
    if (startok == 1) exit(0);
    error("impossible exit status! can't happen");
    exit(9);
  };
	
  fclose(stdin);
  //freopen("/var/log/mwbd.log", "a", stdout);
  //freopen("/var/log/mwbd.log", "a", stderr);
  if (!debugging)
    fclose(stderr);
  fclose(stdout);

  return p;
};

void usage(char * prog)
{
  fprintf(stderr, "%s: [-t] [-d]\n" 
	  "  -t : enable trace on %s or %s\n "
	  "  -d : enable debugging in syslog\n", TRACEFILE1, TRACEFILE2);
  exit(1);
};

int main(int argc, char ** argv)
{
  int rc, c, fd = 1, opr = -1, trace = 0;
  pid_t parent;
  struct fd_info * fdinfo;
  struct sigaction sa;
  FILE * tracefile = NULL;

  /* mmmm, nasty but such a simple way to test an fd for the speciall
     cases of udp packes and new connections. */
  extern int tcp_socket;
  extern int unix_socket;
  extern int udp_socket;

  sigfillset(&sa.sa_mask);
  sa.sa_flags = 0;

  /* graceful exit. */
  sa.sa_handler = sig_term;
  sigaction(SIGTERM, &sa, NULL);
  sigaction(SIGQUIT, &sa, NULL);
  sigaction(SIGINT, &sa, NULL);

  /* signals during daemonic startup, if sigusr1, the daemon started OK, 
     if sigchld, error */
  sa.sa_handler = sig_usr1;
  sigaction(SIGUSR1, &sa, NULL);

  sa.sa_flags = SA_NOCLDSTOP;
  sa.sa_handler = sig_term;
  sigaction(SIGINT, &sa, NULL);

  while( (c = getopt(argc, argv, "td")) != EOF) {
    
    switch (c) {

    case 't':
      trace = 1;
      break;

    case 'd':
      debugging = 1;
      break;

    default:
      usage(argv[0]);
      exit(3);
    };
  }

#ifndef DEBUG
  if (getuid() != 0) {
    fprintf(stderr, "%s must be run as root\n", argv[0]);
    exit(4);
  };
  parent = getpid();
  daemonize();
  openlog(argv[0], LOG_PID|LOG_PERROR, LOG_DAEMON);

  chdir ("/");
#endif

  /* DON'T CALL USAGE() BEYOND THIS POINT */

#ifdef DEBUG
  trace = 1;
#endif

  if (trace) {
    tracefile = fopen (TRACEFILE1, "a");
    if (tracefile) info("tracing on %s", TRACEFILE1);
    else  {
      tracefile = fopen (TRACEFILE2, "a");  
      if (tracefile) info("tracing on %s", TRACEFILE2);
      else error ("failed to open either trace file: %s or %s", TRACEFILE1, TRACEFILE2);
    };
    _mw_srb_traceonfile(tracefile);
  };
 
  errno = 0;
  rc = opensockets();
  if (rc != 0) {
    exit(1);
  };

  info("using multicast address %s", MCASTADDRESS_V4);
  _mw_setmcastaddr();
  initmcast();

  /* signal OK startup */
  kill(parent, SIGUSR1);

  while(!going_down) {

    rc = waitdata ( &fd, &opr);
    if (rc <= 0) continue;
    
    debug( "waitdata gave %d files ready and %d ready with %d", 
	   rc, fd, opr);
    
    if (fd == tcp_socket) {
      fd = accept_tcp();
      if (fd == -1) continue;
      add_c(fd);
      rc = send_srb_ready_not(fd);
      if (rc < 0) {
	unmarkfd(fd);
	close (fd);
      } 
      continue;
    };

    if (fd == unix_socket) {
      fd = accept_unix();
      rc = send_srb_ready_not(fd);
      debug( "sending on %d greating returned %d errno=%d", fd, rc, errno);

      continue;
    };

    if (fd == udp_socket) {
      do_udp_dgram(fd);
      continue;
    };
    
    if (opr == EXCEPTREADY) {
      info("connection on fd=%d broken", fd);
      unmarkfd(fd);
      close(fd);
      del_gw(fd);
      continue;
    };

    fdinfo =  find_gw_byfd(fd);
    if (fdinfo) {
      recv_gw_data(fd, fdinfo);
      continue;
    };

    fdinfo =  find_c_byfd(fd);
    if (fdinfo) {
      read_from_client(fd, fdinfo);
      continue;
    };

    error("THIS CAN'T HAPPEN data to read on fd %d, but itn't a known fd", fd);
    unmarkfd(fd);
    close(fd);
  };

  closeall();
  info ("Midway Broker Daemon exits normally");
  exit(0);
}



