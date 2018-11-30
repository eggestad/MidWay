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
 * Revision 1.9  2004/04/12 23:05:24  eggestad
 * debug format fixes (wrong format string and missing args)
 *
 * Revision 1.8  2003/06/12 07:24:00  eggestad
 * comment fixup
 *
 * Revision 1.7  2002/11/19 12:43:54  eggestad
 * added attribute printf to mwlog, and fixed all wrong args to mwlog and *printf
 *
 * Revision 1.6  2002/10/17 22:08:24  eggestad
 * - we're now using the mwlog() API
 *
 * Revision 1.5  2002/09/26 22:33:42  eggestad
 * - get an abort on SIGPIPE when a gw died. we now use MSG_NOSIGNAL on sendmsg() when passing the desc.
 *
 * Revision 1.4  2002/07/07 22:33:41  eggestad
 * We now operate on Connection structs not filedesc.
 *
 * Revision 1.3  2002/02/17 14:25:52  eggestad
 * added missing includes
 *
 * Revision 1.2  2001/10/05 14:34:19  eggestad
 * fixes or RH6.2
 *
 * Revision 1.1  2001/09/15 23:40:09  eggestad
 * added the broker daemon
 *
 */

#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <netinet/in.h>

#include <MidWay.h>
#include <SRBprotocol.h>
#include <multicast.h>
#include <connection.h>

#include "mwbd.h"
#include "mwbd_sockets.h"

static char * RCSId UNUSED = "$Id$";

/* the three sockets that never die and their addresses */
static struct sockaddr_in tcpsockaddr;
static struct sockaddr_in udpsockaddr;  
static struct sockaddr_un unixsockaddr;  

int tcp_socket = -1;
int unix_socket = -1;
int udp_socket = -1;

/* We can only handle FDSETSIZE - 3 (--) (~1000) gateways until we
   make a forkable (threads don't help) mwbd. (not anytime soon). A
   multi level daemon system is required.  the primary mwbd accepts
   the connectin and passes it on to the mwbd child that has the
   connection to the mwgwd.*/

static fd_set open_fdset;

static int maxfd = -1;

struct fd_info * gw_root = NULL, * gw_tail = NULL;
struct fd_info * client_root = NULL, * client_tail = NULL;

/* some funcs that operation on a Connection * are used with teh UDP
   socket.  we need this peudo var for these calls. */
static Connection pseudoconn = { 
  .fd =            -1, 
  .rejects =        1,
  .domain =        NULL, 
  .version =       0.0, 
  .messagebuffer = NULL,
  .role =          -1
};  


/* functions that deal with fd sets for select() */

void markfd(int fd)
{
  FD_SET(fd, &open_fdset);
  if (fd > maxfd) maxfd = fd;
  DEBUG("fd=%d is now set for select() maxfd = %d", fd, maxfd);
  return;
};

void unmarkfd(int fd)
{
  int i;
  FD_CLR(fd, &open_fdset);
  DEBUG( "fd=%d is now unset for select()", fd);

  if (fd != maxfd) return;
  DEBUG( "fd==maxfd we must find a new maxfd");

  for (i = maxfd-1; i >= 0; i--) {
    if (FD_ISSET(i, &open_fdset)) {
      maxfd = i; 
      DEBUG( "maxfd is now %d", maxfd);
      return;
    };
  };
  Error( "fd=%d was the last open file", maxfd);
  maxfd = -1;
};

/* Functions that maintail a linked list of info for each fd we have 
   on unix sockets to gateways. ops are add(), del(), find().*/


void dump_fd_list(struct fd_info ** fd_root, struct fd_info ** fd_tail)
{
  int i;
  struct fd_info * gw;
  
  if (fd_root == NULL) {
    DEBUG("no root of fd_info list");
    return;
  };

  if (*fd_root == NULL) {
    DEBUG("empty fd_info list");
    return;
  };
  DEBUG("  ********************************* DUMP BEGIN");
  DEBUG(" root = %p  tail = %p", *fd_root, *fd_tail);
  for (gw = *fd_root, i = 0; gw != NULL; i++, gw = gw->next) {
    DEBUG("  %d %p fd=%d %s %s", i, gw, gw->fd, 
	  (*fd_root == gw)?"<-root":"", 
	  (*fd_tail == gw)?"<-tail":"");
  
  };
  DEBUG("  ********************************* DUMP ENDS");
  return;
};

static void add_fd_info(int fd, struct fd_info ** fd_root, struct fd_info ** fd_tail) 
{
  struct fd_info * newgw;

  newgw = (struct fd_info *) malloc(sizeof(struct fd_info));
  if (!newgw) {
    Error ("out of memory!");
    unmarkfd(fd);
    close(fd);
  };
  newgw->fd = fd;
  newgw->majorversion = 0;
  newgw->minorversion = 0;
  newgw->lastused = time(NULL);;
  memset(newgw->domain, '\0',  MWMAXNAMELEN);
  memset(newgw->instance, '\0', MWMAXNAMELEN);
  newgw->incomplete_mesg = NULL;
  newgw->next = NULL;
  
  DEBUG ("added fd info for %d", fd);
  if (*fd_root == NULL) {
    *fd_root = newgw;
  } else {
    (*fd_tail)->next = newgw;
  };
  *fd_tail = newgw;
};
    
void add_gw(int fd) 
{
  add_fd_info(fd, &gw_root, &gw_tail);
  dump_fd_list(&gw_root, &gw_tail);
};

void add_c(int fd) 
{
  add_fd_info(fd, &client_root, &client_tail);
  dump_fd_list( &client_root, &client_tail);
};

static void del_fd_info(int fd, struct fd_info ** fd_root, struct fd_info ** fd_tail)
{
  struct fd_info *gw, * prev;
  
  if (fd_root == NULL) return;
  
  /* special case of deleting the first gw */
  gw = *fd_root;
  if (gw->fd == fd) {
    DEBUG ("deleting first member in fd_info list");
    /* if the only in the list*/ 
    if ((* fd_tail) ==  gw) {
      * fd_tail = NULL;
      DEBUG ("and the only gw!");
    } 
    *fd_root = gw->next;;
    free(gw);
    return;
  };
  
  for (gw = (*fd_root)->next, prev=*fd_root; gw != NULL; prev = gw,  gw = gw->next) {
    
    if (gw->fd == fd) {
      prev->next = gw->next;
      if (*fd_tail == gw) *fd_tail = prev;
      free (gw);
      return;
    };
    
  };
  Error("attempting to delete a gw reference, not none found!");
  return;
};

void del_gw(int fd)
{
  del_fd_info(fd,  &gw_root, &gw_tail);
  dump_fd_list(&gw_root, &gw_tail);
};

void del_c(int fd)
{
  del_fd_info(fd,  &client_root, &client_tail);
  dump_fd_list( &client_root, &client_tail);
};

struct fd_info * find_gw(char * domain, char *instance)
{
  struct fd_info * gw, * candidate = NULL;
  
  if ( (domain == NULL) && (instance == NULL) ) return NULL;
  
  
  for (gw = gw_root; gw != NULL; gw = gw->next) {

    if (domain) DEBUG("testing if domain \"%s\" != \"%s\", skipping is so", 
			  gw->domain, domain);

    if ((domain) && (strncmp(gw->domain, domain, MWMAXNAMELEN) != 0))
      continue;

    if (instance) DEBUG("testing if instance \"%s\" != \"%s\", skipping is so", 
			    gw->instance, instance);
    if ((instance) && 
	(strncmp(gw->instance, instance, MWMAXNAMELEN) != 0))
      continue;

    DEBUG ("ok instance is a candidate");
    if (!candidate || (candidate->lastused > gw->lastused)) 
      candidate = gw;      
    DEBUG("new candidate selected");
  };

  if (candidate) candidate->lastused = time(NULL);
  return candidate;;
};


static struct fd_info * find_fd_info_byfd(int fd, struct fd_info * root)
{
  struct fd_info * gw;
  
  gw = root;

  while(gw != NULL) {
    DEBUG("  checking if %d == %d", gw->fd, fd);
    if (gw->fd == fd) {
      return gw;
    };
    gw = gw->next;
  };
  
  return NULL;
};

struct fd_info * find_gw_byfd(int fd)
{
  DEBUG("looking for %d among the gatways", fd);
  return find_fd_info_byfd(fd, gw_root);
};

struct fd_info * find_c_byfd(int fd)
{
  DEBUG("looking for %d among the clients", fd);
  return find_fd_info_byfd(fd, client_root);
};


/* functions called on initialization and termination */

void closeall(void)
{
  struct fd_info * fdi; 
  if (tcp_socket != -1) close(tcp_socket);
  if (udp_socket != -1) close(udp_socket);
  if (unix_socket != -1) {
    close(unix_socket);
    unlink(unixsockaddr.sun_path);
  };

  for (fdi = client_root; fdi != NULL; fdi = fdi->next) {
    pseudoconn.fd = fdi->fd;
    _mw_srbsendterm(&pseudoconn, 0);
    unmarkfd(fdi->fd);
    /* we shoudl delete, but I think we can allow a mem leak here, 
       this func shoudl only be called just before exit().*/
    close(fdi->fd);
  };
  
  for (fdi = gw_root; fdi != NULL; fdi = fdi->next) {
    pseudoconn.fd = fdi->fd;
    _mw_srbsendterm(&pseudoconn, 0);
    unmarkfd(fdi->fd);
    /* we shoudl delete, but I think we can allow a mem leak here, 
       this func shoudl only be called just before exit().*/
    close(fdi->fd);
  };
  
};

int opensockets(void)
{
  int rc, iOpt, optlen;

  /****************************** UNIX ******************************/

  unixsockaddr.sun_family = AF_UNIX;
  strncpy(unixsockaddr.sun_path, UNIXSOCKETPATH, 100);

  unix_socket = socket(PF_UNIX, SOCK_STREAM, 0);
  if (unix_socket == -1) {
    Error( "socket for unix failed errno=%d", errno);
    return -1;
  };
  rc = connect (unix_socket, (struct sockaddr*) &unixsockaddr, sizeof(struct sockaddr_un));
  if (rc == 0) {
    Error( "mwbd already running");
    return -1;
  };
  errno = 0;

  unlink(unixsockaddr.sun_path);
  rc = bind(unix_socket, (struct sockaddr*) &unixsockaddr, sizeof(struct sockaddr_un));
  if (rc == -1) {
    Error( "bind for unix socket on %s failed errno=%d", unixsockaddr.sun_path, errno);
    return -1;
  };

  rc = chmod (unixsockaddr.sun_path, 0777);
  if (rc == -1)  
    Error( "chmod for unix socket on %s failed errno=%d", unixsockaddr.sun_path, errno);


  /****************************** TCP ******************************/

  tcpsockaddr.sin_family = AF_INET;
  tcpsockaddr.sin_port = htons(SRB_BROKER_PORT);
  tcpsockaddr.sin_addr.s_addr = INADDR_ANY;

  tcp_socket = socket(PF_INET, SOCK_STREAM, 0);
  if (tcp_socket == -1) {
    Error( "socket for tcp failed errno=%d", errno);
    return -1;
  };

  iOpt= 1;
  rc = setsockopt(tcp_socket, SOL_SOCKET, SO_REUSEADDR, &iOpt, sizeof(int));
  DEBUG("setsockopt(tcp_socket, SOL_SOCKET, SO_REUSEADDR)=%d errno=%d", 
	rc, errno);


  rc = bind(tcp_socket, (struct sockaddr*) &tcpsockaddr, sizeof(struct sockaddr_in));
  if (rc == -1) {
    Error( "bind for tcp failed errno=%d", errno);
    return -1;
  };


  /****************************** UDP ******************************/

  udp_socket = socket(AF_INET, SOCK_DGRAM, 0);

  udpsockaddr.sin_family = AF_INET;
  udpsockaddr.sin_port = htons(SRB_BROKER_PORT);
  udpsockaddr.sin_addr.s_addr = htonl(INADDR_ANY);

  rc = bind(udp_socket, (struct sockaddr *)&udpsockaddr, sizeof(struct sockaddr_in));
  if (rc == -1) {
    Error( "bind for udp failed errno=%d", errno);
    return -1;
  };
  rc = setsockopt(udp_socket, SOL_SOCKET, SO_REUSEADDR, &iOpt, sizeof(int));
  DEBUG( "setsockopt(tcp_socket, SOL_SOCKET, SO_REUSEADDR)=%d errno=%d", 
	 rc, errno);


  optlen = sizeof(int);

  rc = listen(tcp_socket, 30);
  DEBUG( "listen for tcp returned %d errno=%d", rc, errno);

  rc = listen(unix_socket, 30);
  DEBUG( "listen for unix returned %d errno=%d", rc, errno);

  /* init the fd_set and mark the listen sockets */
  FD_ZERO(&open_fdset);
  markfd(tcp_socket);
  markfd(unix_socket);
  markfd(udp_socket);
  

  /* set nonblocking of these sockets */
 
  return 0;
};

 
int accept_tcp(void)
{
  int sd;

  sd = accept(tcp_socket, NULL, 0);
  DEBUG( "accept on tcp returned %d errno =%d", sd, errno);
  if (sd == -1) return sd;;

  markfd(sd);
  return sd;;
};

/* accepts connections from gateways. */

int accept_unix(void)
{
  int newfd;
  
  newfd = accept(unix_socket, NULL, 0);
  DEBUG( "accept on unix returned fd=%d", newfd);
  if (newfd != -1) markfd(newfd);
  add_gw(newfd);

  return newfd;
};

void initmcast(void)
{
  _mw_initmcast(udp_socket);
};

/* clean client is called regurarly by waitdata() to clean up
   connections that idle, Thus that connect but never send a SRB
   message. This is a DoS defence. */
void client_clean(void)
{
  struct fd_info * cfi = NULL;
  int now, fd;
  
  now = time(NULL);
 restart:
  for (cfi = client_root; cfi != NULL; cfi = cfi->next) {
    DEBUG("checking if fd=%d has exired %ld < %d", fd, cfi->lastused,  now - 10);
    if (cfi->lastused < now - 10) {
      fd = cfi->fd;
      Info("closing client withfd=%d due to timeout", fd);
      unmarkfd(fd);
      del_c(fd); 
      /* since we're going thru the list and have it changed thru
	 del_c() very bad things happen if we don't restart the for
	 loop cfi is invalid.*/
      close(fd); 
      goto restart; 
    }; 
  };
};

/* wrapper for select. mwbd should spend most of the time in
      blocking wait here. */

int waitdata(int * fd, int * operation)
{
  int i, n;
  struct timeval to;

  fd_set rfdset, efdset;

  /* if there are no files open */
  if (maxfd < 0)  {
    errno = EINVAL;
    return 0;
  };

 reselect:
  memcpy(&rfdset, &open_fdset, sizeof(fd_set));
  memcpy(&efdset, &open_fdset, sizeof(fd_set));

  to.tv_sec = 30;
  to.tv_usec = 0;
  errno = 0;
  DEBUG( "about to select(%d, , , , %ld.%ld) ", maxfd, to.tv_sec, to.tv_usec);
  {
    char line[256];
    int i, len = 0;
    line[0] = '\0';
    len = sprintf(line, "select on fd's: (setsize = %zu ", sizeof(fd_set)*8);
    for (i = 0; i < sizeof(fd_set)*8; i++) {
      if (FD_ISSET(i, &rfdset)) {
	len += sprintf (line+len, "%d, ", i);
	if (len > 250) {
	  DEBUG("  %s", line);
	  len = 0;
	  line[0] = '\0';
	};
      };
    };
    if (len) DEBUG("  %s", line);
  };
  n = select (maxfd+1, &rfdset, NULL, &efdset, &to);

  DEBUG( "select() returned %d errno=%d", n, errno);

  if (n == 0) {
    client_clean();
    goto reselect;
  };

  if (n < 0) {
    switch (errno) {
    case EINTR:
      return -1;
      
    case  EAGAIN:      
      goto reselect; 
    default:
      return n;
    };
  };

  /* if fd is NULL, nothing should be done to find the first ready fd. */
  if (fd == NULL) return n;

  /* maybe we should do all the reads first, but....  Maybe we should
     remember where we left off so that heavy traffic on a fd with low
     number don't block a fd with high number. But hey, this is a low
     traffic server, could it be a DoS attach scenario?? */

  for (i = 0; i <= maxfd; i ++) {
    if (FD_ISSET(i, &rfdset)) {
      *fd = i;
      if (operation != NULL) * operation = READREADY;
    };
  
    if (FD_ISSET(i, &efdset)) {
      *fd = i;
      if (operation != NULL) * operation = EXCEPTREADY;
    };
  };
  return n;
};


/* function that passes new_fd down gwfd, and closes new_fd */

int sendfd(int new_fd, int gwfd, char * message, int messagelen)
{
  int rc;
  struct msghdr msg;
  struct cmsghdr * cmsg;
  struct iovec iov[1];
  char buffer[CMSG_SPACE(sizeof(int))];

  errno = 0;

  iov[0].iov_base = message;
  iov[0].iov_len = messagelen;
  msg.msg_iov = iov;
  msg.msg_iovlen = 1;

  msg.msg_name = NULL;
  msg.msg_namelen = 0;
  msg.msg_control = buffer;
  msg.msg_controllen  = sizeof(buffer);

  msg.msg_flags = 0;

  cmsg = CMSG_FIRSTHDR(&msg);
  if (cmsg != NULL) {
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));
    msg.msg_controllen = cmsg->cmsg_len;
    * ((int*)CMSG_DATA(cmsg)) = new_fd;
  };
  int flags = 0;
#ifdef MSG_NOSIGNAL
  flags = MSG_NOSIGNAL;
#endif
  rc = sendmsg(gwfd, &msg, flags);
  return rc;
};

