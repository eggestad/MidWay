/*
  The MidWay API
  Copyright (C) 2001 Terje Eggestad

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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <netdb.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

#include <MidWay.h>
#include <SRBprotocol.h>
#include <multicast.h>
#include <connection.h>

/* some funcs that operation on a Connection * are used with teh UDP
   socket.  we need this peudo var for these calls. */
Connection pseudoconn = { 
  .fd              = -1, 
  .rejects         =  0,
  .domain          =  NULL, 
  .version         =  0, 
  .peeraddr_string =  "query send", 
  .messagebuffer   =  NULL,
  .type            =  CONN_TYPE_MCAST
};  

static struct ip_mreq mr;

static int use_broadcast = USE_BROADCAST;
static int use_multicast = USE_MULTICAST;

int _mw_setmcastaddr(void)
{
  mr.imr_interface.s_addr = (uint32_t) htonl(INADDR_ANY);

  inet_pton(AF_INET, MCASTADDRESS_V4 , &mr.imr_multiaddr);

  DEBUG1(" mcast addr %#x ifaddr %#x", 
	 mr.imr_multiaddr.s_addr, mr.imr_interface.s_addr);

  return 0;
};

/* TODO: add member ship to all interfaces, and set TTL higher */
int _mw_initmcast(int s)
{
  int rc, val;
  _mw_setmcastaddr();
  
  if (use_broadcast) {
     val = 1;
     rc = setsockopt(s, SOL_SOCKET, SO_BROADCAST, &val, 4);
     DEBUG1("set broadcast opt returned %d errno =%d", rc, errno);   
  } 

  if (use_multicast) {
     rc = setsockopt (s, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mr, sizeof(struct ip_mreq));
     DEBUG1("add membership returned %d errno =%d", rc, errno);
     
     rc = setsockopt (s, IPPROTO_IP, IP_MULTICAST_IF, &mr.imr_interface, 
		      sizeof(struct in_addr));
     DEBUG1("add multicast_if returned %d errno =%d", rc, errno);
  } 
  return rc;
};



int _mw_sendunicast_old (int s, struct  in_addr * iaddr, char * payload)
{
  int rc, plen, err;
  struct sockaddr_in to;

  to.sin_family = AF_INET;
  to.sin_port = htons(SRB_BROKER_PORT);
  to.sin_addr = *iaddr;
  
  plen = strlen(payload);
  DEBUG1("send unicast on fd=%d", s);
  pseudoconn.fd = s;
  _mw_srb_trace(SRB_TRACE_OUT, &pseudoconn, payload, plen);
  pseudoconn.fd = -1;

  errno = ENONET;
  err = 0;
  rc = -1;

  char buf[1024];
  DEBUG1 ("addr4  = %s\n", inet_ntop(to.sin_family, &to.sin_addr, buf, 1024));
  rc = sendto (s,  payload, plen , 0, (struct sockaddr *)&to, sizeof(struct sockaddr_in));
  DEBUG1("sent unicast rc = %d errno = %d", rc, errno);
 
  return rc;
};

int _mw_sendmcast (int s, char * payload) {
   int val = 0, rc;
   char * env;
  
   if ( (env = getenv ("MW_USE_BROADCAST")) ) {
      use_broadcast = atoi(env) ? 1 : 0;
   };
   
   struct sockaddr_in to;
   to.sin_family = AF_INET;
   to.sin_port = htons(SRB_BROKER_PORT);

   // default query localhost
   to.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
   
   if (use_broadcast) {
      DEBUG1("using broadcast");
      rc = setsockopt(s, SOL_SOCKET, SO_BROADCAST, &val, 4);
      DEBUG1("set broadcast opt returned %d errno =%d", rc, errno);
      to.sin_addr.s_addr = htonl(INADDR_BROADCAST);
   }  else if (use_multicast) {
      rc =  _mw_setmcastaddr();
      DEBUG1("set multicast opt returned %d errno =%d", rc, errno);
      if (rc == -1) return rc;
      memcpy(&to.sin_addr,  &mr.imr_multiaddr.s_addr, sizeof(struct in_addr));
   }
   
   rc = _mw_sendunicast(s, to, payload);
   if (rc == -1) {
      if ( (errno ==  ENETUNREACH) || (errno ==  ENONET) ) {
	 DEBUG1("Multi/broadcast failed probably because we're on a standalone node, "
		"doing unicast to loopback");
	 errno = 0;
	 to.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	 rc = _mw_sendunicast(s, to, payload);
	 DEBUG1("sendto loopback returned %d errno=%d", rc, errno);
      } else {
	 DEBUG1("sendto failed with unexpected  errno=%d", errno);
      }
   };
   return rc;
}

int _mw_sendunicast (int s, struct sockaddr_in to, char * payload)
{
  int rc, plen;
  to.sin_family = AF_INET;
  if (to.sin_port == 0)
     to.sin_port = htons(SRB_BROKER_PORT);

  plen = strlen(payload);
  DEBUG1("sending query on fd=%d", s);
  pseudoconn.fd = s;
  _mw_srb_trace(SRB_TRACE_OUT, &pseudoconn, payload, plen);
  pseudoconn.fd = -1;

  errno = 0;
  rc = 0;
  char buf[1024];
  DEBUG1 ("addr4  = %s\n", inet_ntop(to.sin_family, &to.sin_addr, buf, 1024));
  rc = sendto (s,  payload, plen , 0, (struct sockaddr *)&to, sizeof(struct sockaddr_in));
  DEBUG1("sendto returned %d errno=%d", rc, errno);

  return rc;
};

// we maintain a list of known instances, after a sendmcastquery. This
// is to prevent multiple replies from the same instance from
// _mw_getmcastreply().

static char * known_instances_list = NULL;
static int known_instances_alloc = 0;
static int known_instances = 0;

static char * make_SRBready_req(const char * domain, const char * instance) {
   SRBmessage srbreq;
   static char buffer[SRBMESSAGEMAXLEN];
   strcpy(srbreq.command, SRB_READY);
   srbreq.marker = SRB_REQUESTMARKER;
   srbreq.map = NULL;
   
   srbreq.map = urlmapadd(srbreq.map, SRB_VERSION, NULL);
   srbreq.map = urlmapadd(srbreq.map, SRB_DOMAIN, NULL);
   srbreq.map = urlmapadd(srbreq.map, SRB_INSTANCE, NULL);
   
   urlmapset(srbreq.map, SRB_VERSION, SRBPROTOCOLVERSION);
   if (domain) urlmapset(srbreq.map, SRB_DOMAIN, domain);
   if (instance) urlmapset(srbreq.map, SRB_INSTANCE, instance);
   
   int len = _mw_srbencodemessage(&srbreq, buffer, SRBMESSAGEMAXLEN);
   urlmapfree(srbreq.map);
   return buffer;
}

int _mw_sendmcastquery(int s, const char * domain, const char * instance) {
   char * buffer;
   _mw_initmcast(s);
   int rc;
  buffer = make_SRBready_req( domain,  instance);
  rc = _mw_sendmcast (s, buffer);

  // clear the list of known instances, new query, a new unique list
  if (known_instances_alloc == 0) {
     known_instances_alloc = 16;
     known_instances_list = malloc(sizeof(char) * known_instances_alloc * MWMAXNAMELEN);
  };
  known_instances = 0;

  return rc;
}

int _mw_sendunicastquery(int s, struct sockaddr_in to,
			    const char * domain, const char * instance)
{

  char * buffer;
  int rc, len;

  buffer = make_SRBready_req( domain,  instance);
  rc = _mw_sendunicast (s, to, buffer);

  // clear the list of known instances, new query, a new unique list
  if (known_instances_alloc == 0) {
     known_instances_alloc = 16;
     known_instances_list = malloc(sizeof(char) * known_instances_alloc * MWMAXNAMELEN);
  };
  known_instances = 0;

  return rc;
};
  
int _mw_getmcastreply(int s, instanceinfo * reply, float timeout)
{
  char buffer[SRBMESSAGEMAXLEN+1];
  int i, n, rc;
  socklen_t slen;
  size_t len;
  struct timeval tv;
  fd_set rfdset;
  struct sockaddr_in peeraddr;
  char * mapval, buf[256];
  SRBmessage * srbrpy;

  if (reply == NULL) {
    errno = EINVAL;
    return -1;
  };

  if (timeout < 0.0) {
    errno = EINVAL;
    return -1;
  };

  FD_ZERO(&rfdset);
  FD_SET(s, &rfdset);
  
  tv.tv_sec = (int) timeout;
  tv.tv_usec  = ( timeout - ((float) tv.tv_sec)) * 1000000;
  DEBUG1("waiting for udp packet on %d for %ld.%ld (%f) secs", 
	s, tv.tv_sec, tv.tv_usec, timeout);

  n = select(s+1, &rfdset, NULL, NULL, &tv);
  DEBUG1("select returned %d", n);

  if (n == 0) {
    errno = ETIME;
    return -1;
  };
  if (n == -1) return -1;

  if (n != 1) {
    Error("in _mw_getmcastreply select returned %d ready fd's " 
	  "but only one possible", n);
    errno = EFAULT;
    return -1;
  };
  
  if (!FD_ISSET(s, &rfdset)) {
    Error("in _mw_getmcastreply select return wrong fd ready");
    errno = EFAULT;
    return -1;
  };

  len = SRBMESSAGEMAXLEN;
  slen = sizeof (struct sockaddr_in);
  rc =recvfrom (s, buffer, len, 0, 
		(struct sockaddr *) &peeraddr, &slen);
  DEBUG1("recvfrom returned %d", rc);
  if (rc < 1) return -1;

  buffer[rc] = '\0';  
  _mw_srb_trace(SRB_TRACE_IN, &pseudoconn, buffer, rc);
  DEBUG1("got a message %d bytes long", rc);

  srbrpy = _mw_srbdecodemessage(&pseudoconn, buffer, rc);
  if (srbrpy == NULL) {
    DEBUG1("failed to decode, discarding");
    errno = EBADMSG;
    return -1;
  };
  
  if ( (strcmp(srbrpy->command, SRB_READY) != 0) ||
       (srbrpy->marker != SRB_RESPONSEMARKER) ) {
    DEBUG1("unexpected reply %s%c:..., discarding", 
	  srbrpy->command, srbrpy->marker);
    errno = EBADMSG;
    return -1;
  };

  // first we the the instance name 
  mapval = urlmapgetvalue(srbrpy->map, SRB_INSTANCE) ;
  if (!mapval) {
     errno = EBADMSG;
     return -1;
  };

  for (i = 0; i < known_instances; i++) {
     DEBUG1("comparing %s with %s", known_instances_list+(i*MWMAXNAMELEN), mapval);
     if (strncmp(known_instances_list+(i*MWMAXNAMELEN), mapval, MWMAXNAMELEN) == 0) {
	DEBUG1("got a duplicate reply from %s", mapval);
	return _mw_getmcastreply(s, reply, timeout);
     };
  };
  
  // extend the list if necessary
  if (known_instances_alloc <= known_instances+1) {
     known_instances_alloc *= 2;
     known_instances_list = realloc(known_instances_list, sizeof(char) * known_instances_alloc * MWMAXNAMELEN);
  };
  
  strncpy(known_instances_list+(known_instances*MWMAXNAMELEN), mapval, MWMAXNAMELEN);
  strncpy(reply->instance, mapval, MWMAXNAMELEN);  
  known_instances++;

  memcpy(&reply->address, &peeraddr, sizeof(struct sockaddr_in));

  mapval = urlmapgetvalue(srbrpy->map, SRB_VERSION) ;
  if (mapval)
    strncpy(reply->version, mapval, 7);
  
  mapval = urlmapgetvalue(srbrpy->map, SRB_DOMAIN) ;
  if (mapval)
    strncpy(reply->domain, mapval, MWMAXNAMELEN);
  
  DEBUG1("mcast reply: peer=%s:%d, ver=%s domain=%s instance=%s", 
	inet_ntop(AF_INET, &peeraddr.sin_addr, buf, 256), 
	ntohs(peeraddr.sin_port), 
	reply->version, reply->domain, reply->instance);

  urlmapfree(srbrpy->map);
  free(srbrpy);
  
  return 0;
};

  
instanceinfo * mwbrokerquery(const char * domain, const char * instance)
{
  instanceinfo * gws = NULL;
  int idx = 0;
  int s, rc;


  s = socket(AF_INET, SOCK_DGRAM, 0);
  if (s == -1) return NULL;

  rc = _mw_sendmcastquery(s, domain, instance);
  if (rc == -1) {
    close(s);
    return NULL;
  };

  rc = 0;
  /* we wait for 2-3 secs on replies */
  while (rc == 0) { 
    gws = realloc(gws, sizeof(instanceinfo)*(idx + 2));
    rc =  _mw_getmcastreply(s, &gws[idx], 2.0);
    DEBUG("getting reply %d rc=%d", idx, rc);
    if (rc == -1) {
      switch (errno) {
      case EBADMSG:
	rc = 0;
      };
      continue;	  
    };
    idx++;
  } 

  DEBUG("we got %d replies", idx);
  
  if (gws)
    memset(&gws[idx], '\0', sizeof(instanceinfo));

  close(s);
  return gws;
};
  




