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

static char * RCSId = "$Id$";
static char * RCSName = "$Name$"; /* CVS TAG */

/* 
 * $Log$
 * Revision 1.1  2001/09/15 23:40:09  eggestad
 * added the broker daemon
 *
 */

#include <stdio.h>
#include <errno.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/uio.h>

#include <MidWay.h>
#include <SRBprotocol.h>

#include "../mwbd/mwbd.h"

static int ipckey = 501;

int connectbroker(char * domain, char * instance)
{
  int rc, iOpt, optlen;
  int unix_socket = -1;
  SRBmessage srbmsg, * srbmsg_ready;
  char _instance[MWMAXNAMELEN]; 
  char buffer[SRBMESSAGEMAXLEN];
  struct sockaddr_un unixsockaddr;  

  if ((instance == NULL) || (domain == NULL)) {
    errno = EINVAL;
    return -1;
  };


  unixsockaddr.sun_family = AF_UNIX;
  strncpy(unixsockaddr.sun_path, UNIXSOCKETPATH, 100);

  unix_socket = socket(PF_UNIX, SOCK_STREAM, 0);
  if (unix_socket == -1) {
    mwlog(MWLOG_ERROR, "socket for unix failed errno=%d\n", errno);
    return -1;
  };
  rc = connect (unix_socket, (struct sockaddr *)&unixsockaddr, sizeof(struct sockaddr_un));
  if (rc == -1) {
    mwlog(MWLOG_WARNING, "broker is not running");
    return -1;
  };
  mwlog(MWLOG_DEBUG1, "connected to broker on fd=%d", unix_socket);
 
  mwlog(MWLOG_INFO, "connected to broker at %s \n",unixsockaddr.sun_path);

  /* first we wait for broker et send SRB READY */
  /* TODO add timeout */
  rc = read(unix_socket, buffer, SRBMESSAGEMAXLEN);
  
  if (rc == -1) return -1;
  _mw_srb_trace(SRB_TRACE_IN, unix_socket, buffer, rc);
  srbmsg_ready = _mw_srbdecodemessage(buffer);
  if (srbmsg_ready == NULL) {
    mwlog(MWLOG_DEBUG1, "failed to decode message from broker");
    errno = EBADMSG;
    close(unix_socket);
    return -1;
  };
  if ( (strcmp(srbmsg_ready->command, SRB_READY) != 0) || 
       (srbmsg_ready->marker != SRB_NOTIFICATIONMARKER)) {
    mwlog(MWLOG_DEBUG1, "unexpected message from broker");
    errno = EBADMSG;
    close(unix_socket);
    return -1;
  };
  
  mwlog(MWLOG_DEBUG1, "got greeting from broker");
  if (srbmsg_ready->map) urlmapfree(srbmsg_ready->map);
  free (srbmsg_ready);
  
  strcpy(srbmsg.command, SRB_READY);
  srbmsg.marker = SRB_NOTIFICATIONMARKER;
  srbmsg.map = NULL;
  srbmsg.map = urlmapadd(srbmsg.map, SRB_VERSION, SRBPROTOCOLVERSION);
  srbmsg.map = urlmapadd(srbmsg.map, SRB_DOMAIN, domain);
  srbmsg.map = urlmapadd(srbmsg.map, SRB_INSTANCE, instance);
  rc = _mw_srbsendmessage(unix_socket, &srbmsg);

  urlmapfree(srbmsg.map);

  if (rc < 0) {
    errno = -rc;
    return -1;
  };
  mwlog(MWLOG_DEBUG1, "broker connected on fd %d", unix_socket);
  return unix_socket;
}


int read_with_fd(int s, char * message, int len, int * fd)
{
  int rc;
  struct msghdr msg;
  struct iovec iov[1];
  struct cmsghdr * cmsg;
  char buffer[CMSG_SPACE(sizeof(int))*2];

  msg.msg_name = NULL;
  msg.msg_namelen = 0;
  msg.msg_iov = iov;
  msg.msg_iovlen = 1;;
  msg.msg_control = buffer;
  msg.msg_controllen = CMSG_SPACE(sizeof(int))*2;
  msg.msg_flags = 0;

  iov[0].iov_base = message;
  iov[0].iov_len = len;

  cmsg = CMSG_FIRSTHDR(&msg);
  cmsg->cmsg_len = msg.msg_controllen;
  cmsg->cmsg_level = SOL_SOCKET;
  cmsg->cmsg_type = SCM_RIGHTS;
  
  rc = 0;
  rc = recvmsg(s, &msg, 0);
  mwlog(MWLOG_DEBUG1, "received from broker rc=%d errno=%d", rc, errno);
  if (rc == -1) return rc;

  
  cmsg = CMSG_FIRSTHDR(&msg);
  if ( (cmsg == NULL) || cmsg->cmsg_len < CMSG_LEN(sizeof(int)) ) {
    mwlog(MWLOG_DEBUG1,  "can't find cmsg");
    
    return rc;
  };

  
  /* if caller din't want a a fd, close it */
  if (fd != NULL) {
    *fd = * (int *) CMSG_DATA(cmsg);
    mwlog(MWLOG_DEBUG1, "received from broker \"%s\" fd=%d rc=%d errno=%d\n", 
	  message, *fd, rc, errno);
  } else {
    close( * (int *) CMSG_DATA(cmsg));
    mwlog(MWLOG_DEBUG1, "received from broker \"%s\" rc=%d errno=%d\n", 
	  message,  rc, errno);
    
  };
  
  return rc;
}

