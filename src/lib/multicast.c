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


static char * RCSId = "$Id$";
static char * RCSName = "$Name$"; /* CVS TAG */

/* 
 * $Log$
 * Revision 1.1  2001/09/15 23:40:09  eggestad
 * added the broker daemon
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netdb.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

#include <MidWay.h>
#include <SRBprotocol.h>
#include <multicast.h>

static struct ip_mreq mr;

int _mw_setmcastaddr(void)
{
  mr.imr_interface.s_addr = (in_addr_t) htonl(INADDR_ANY);

  inet_pton(AF_INET, MCASTADDRESS_V4 , &mr.imr_multiaddr);

  mwlog(MWLOG_DEBUG1, " mcast addr %#x ifaddr %#x", 
	 mr.imr_multiaddr.s_addr, mr.imr_interface.s_addr);
  return 0;
};

/* TODO: add member ship to all interfaces, and set TTL higher */
int _mw_initmcast(int s)
{
  int rc;

  rc = setsockopt (s, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mr, sizeof(struct ip_mreq));
  mwlog(MWLOG_DEBUG1, "add membership returned %d errno =%d", rc, errno)
;
  rc = setsockopt (s, IPPROTO_IP, IP_MULTICAST_IF, &mr.imr_interface, 
		   sizeof(struct in_addr));
  mwlog(MWLOG_DEBUG1, "add multicast_if returned %d errno =%d", rc, errno);

  return rc;
};


int _mw_sendmcast (int s, char * payload, int plen)
{
  int rc;
  struct sockaddr_in to;

  to.sin_family = AF_INET;
  to.sin_port = htons(SRB_BROKER_PORT);
  memcpy(&to.sin_addr,  &mr.imr_multiaddr.s_addr, sizeof(struct in_addr));

  if (plen == 0) plen = strlen(payload);

  _mw_srb_trace(SRB_TRACE_OUT, s, payload, plen);
  rc = sendto (s,  payload, plen , 0, (struct sockaddr *)&to, sizeof(struct sockaddr_in));

  mwlog(MWLOG_DEBUG1, "_mw_sendmcast returned %d", rc);
  return rc;
};

int _mw_sendmcastquery(int s, char * domain, char * instance)
{
  SRBmessage srbreq;
  char buffer[SRBMESSAGEMAXLEN];
  int rc, len;

  strcpy(srbreq.command, SRB_READY);
  srbreq.marker = SRB_REQUESTMARKER;
  srbreq.map = NULL;

  srbreq.map = urlmapadd(srbreq.map, SRB_VERSION, NULL);
  srbreq.map = urlmapadd(srbreq.map, SRB_DOMAIN, NULL);
  srbreq.map = urlmapadd(srbreq.map, SRB_INSTANCE, NULL);

  urlmapset(srbreq.map, SRB_VERSION, SRBPROTOCOLVERSION);
  if (domain) urlmapset(srbreq.map, SRB_DOMAIN, domain);
  if (instance) urlmapset(srbreq.map, SRB_INSTANCE, instance);

  len = _mw_srbencodemessage(&srbreq, buffer, SRBMESSAGEMAXLEN);
  rc = _mw_sendmcast (s, buffer, len);
  urlmapfree(srbreq.map);

  return rc;
};
  
int _mw_getmcastreply(int s, instanceinfo * reply, float timeout)
{
  char buffer[SRBMESSAGEMAXLEN];
  int n, len, slen, rc;
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
  mwlog(MWLOG_DEBUG1, "waiting for udp packet on %d for %d.%d (%f) secs", 
	s, tv.tv_sec, tv.tv_usec, timeout);

  n = select(s+1, &rfdset, NULL, NULL, &tv);
  mwlog(MWLOG_DEBUG1, "select returned %d", n);

  if (n == 0) {
    errno = ETIME;
    return -1;
  };
  if (n == -1) return -1;

  if (n != 1) {
    mwlog(MWLOG_ERROR, "in _mw_getmcastreply select returned %d ready fd's " 
	  "but only one possible", n);
    errno = EFAULT;
    return -1;
  };
  
  if (!FD_ISSET(s, &rfdset)) {
    mwlog(MWLOG_ERROR, "in _mw_getmcastreply select return wrong fd ready");
    errno = EFAULT;
    return -1;
  };

  len = SRBMESSAGEMAXLEN;
  slen = sizeof (struct sockaddr_in);
  rc =recvfrom (s, buffer, len, 0, 
		(struct sockaddr *) &peeraddr, &slen);
  mwlog(MWLOG_DEBUG1, "recvfrom returned %d", rc);

  if (rc < 1) return -1;

  _mw_srb_trace(SRB_TRACE_IN, s, buffer, rc);
  mwlog(MWLOG_DEBUG1, "got a message %d bytes long", rc);

  srbrpy = _mw_srbdecodemessage(buffer);
  if (srbrpy == NULL) {
    mwlog(MWLOG_DEBUG1, "failed to decode, discarding");
    errno = EBADMSG;
    return -1;
  };
  
  if ( (strcmp(srbrpy->command, SRB_READY) != 0) ||
       (srbrpy->marker != SRB_RESPONSEMARKER) ) {
    mwlog(MWLOG_DEBUG1, "unexpected reply %s%c:..., discarding", 
	  srbrpy->command, srbrpy->marker);
    errno = EBADMSG;
    return -1;
  };


  memcpy(&reply->address, &peeraddr, sizeof(struct sockaddr_in));

  mapval = urlmapgetvalue(srbrpy->map, SRB_VERSION) ;
  if (mapval)
    strncpy(reply->version, mapval, 7);
  
  mapval = urlmapgetvalue(srbrpy->map, SRB_INSTANCE) ;
  if (mapval)
    strncpy(reply->instance, mapval, MWMAXNAMELEN);
  
  mapval = urlmapgetvalue(srbrpy->map, SRB_DOMAIN) ;
  if (mapval)
    strncpy(reply->domain, mapval, MWMAXNAMELEN);
  
  mwlog(MWLOG_DEBUG1, "mcast reply: peer=%s:%d, ver=%s domain=%s instance=%s", 
	inet_ntop(AF_INET, &peeraddr.sin_addr, buf, 256), 
	ntohs(peeraddr.sin_port), 
	reply->version, reply->domain, reply->instance);

  urlmapfree(srbrpy->map);
  free(srbrpy);
  
  return 0;
};

  
instanceinfo * mwbrokerquery(char * domain, char * instance)
{
  instanceinfo * gws = NULL;
  int idx = 0;
  int s, flags, rc;


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
    mwlog(MWLOG_DEBUG, "getting reply %d rc=%d", idx, rc);
    if (rc == -1) {
      switch (errno) {
      case EBADMSG:
	rc = 0;
      };
      continue;	  
    };
    idx++;
  } 

  mwlog(MWLOG_DEBUG, "we got %d replies", idx);
  
  if (gws)
    memset(&gws[idx], '\0', sizeof(instanceinfo));

  close(s);
  return gws;
};
  



