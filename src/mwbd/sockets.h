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

#include <errno.h>
#include <assert.h>
#include <sys/socket.h>

#include "mwbd.h"



/* the three sockets that never die and their addresses */
static struct sockaddr_in tcpsockaddr;
static struct sockaddr_in udpsockaddr;  
static struct sockaddr_un unixsockaddr;  

static int tcp_socket = -1;
static int unix_socket = -1;
static int udp_socket = -1;

/* We can only handle FDSETSIZE - 3 (--) (~1000) gateways until we
   make a forkable (threads don't help) mwbd. (not anytime soon). A
   multi level daemon system is required.  the primary mwbd accepts
   the connectin and passes it on to the mwbd child that has the
   connection to the mwgwd.*/

fd_set open_fdset;

int maxfd = -1;
#define READREADY    1
#define WRITEREADY   2
#define EXCEPTREADY  3


/* functions that deal with fd sets for select() */

void markfd(int fd)
{
  FD_SET(fd, &open_fdset);
  if (fd > maxfd) maxfd = fd;
  debug("fd=%d is now set for select() maxfd = %d", fd, maxfd);
  return;
};

void unmarkfd(int fd)
{
  int i;
  FD_ISSET(fd, &open_fdset);
  debug( "fd=%d is now unset for select()", fd);

  if (fd != maxfd) return;
  debug( "fd==maxfd we must find a new maxfd");

  for (i = maxfd-1; i >= 0; i--) {
    if (FD_ISSET(fd, &open_fdset)) {
      maxfd = i; 
      debug( "maxfd is now %d %d", maxfd);
      return;
    };
  };
  debug( "fd=%d was the last open file", maxfd);
  maxfd = -1;
};

/* Functions that maintail a linked list of info for each fd we have 
   on unix sockets to gateways. ops are add(), del(), find().*/

struct fd_info * gw_root = NULL, * gw_tail = NULL;

void add_gw(int fd)
{
  struct fd_info * newgw;

  newgw = (struct fd_info *) malloc(sizeof(struct fd_info));
  if (!newgw) {
    error ("out of memory!");
    unmarkfd(fd);
    close(fd);
  };
  newgw->fd = fd;
  newgw->majorversion = 0;
  newgw->minorversion = 0;
  newgw->lastused = 0;
  memset(newgw->domain, '\0',  MWMAXNAMELEN);
  memset(newgw->instance, '\0', MWMAXNAMELEN);
  newgw->incomplete_mesg = NULL;
  newgw->next = NULL;

  if (gw_root == NULL) {
    gw_root = newgw;
  } else {
    gw_tail->next = newgw;
  };
  gw_tail = newgw;
};
    
void del_gw(int fd)
{
  struct fd_info ** gw;
  
  gw = & gw_root;
  
  while(*gw != NULL) {

    if ((*gw)->fd == fd) {
      if (gw_tail == (*gw)->next) gw_tail = *gw;
      (*gw) = (*gw)->next;
      return;
    };
    gw = &((*gw)->next);
  };
  error("attempting to delete a gw reference, not none found!");
  return;
};

struct fd_info * find_gw(char * domain, char *instance)
{
  struct fd_info * gw, * candidate = NULL;
  
  if ( (domain == NULL) && (instance == NULL) ) return NULL;
  
  
  for (gw = gw_root; gw != NULL; gw = gw->next) {

    if (gw->domain && (strncmp(gw->domain, domain, MWMAXNAMELEN) != 0))
      continue;

    if (gw->instance && (strncmp(gw->instance, instance, MWMAXNAMELEN) != 0))
      continue;

    if (!candidate || (candidate->lastused > gw->lastused)) 
      candidate = gw;      
    
  };

  if (candidate) candidate->lastused = time(NULL);
  return candidate;;
};


struct fd_info * find_gw_byfd(int fd)
{
  struct fd_info * gw;
  
  gw = gw_root;

  while(gw != NULL) {
    if (gw->fd == fd) {
      return gw;
    };
    gw = gw->next;
  };
  
  return NULL;
};


/* wrapper for select. mwbd should spend most of the time in blocking
   wait here. */

int waitdata(int * fd, int * operation)
{
  int i, n;

  fd_set rfdset, wfdset, efdset;

  /* if there are no files open */
  if (maxfd < 0)  {
    errno = EINVAL;
    return 0;
  };

  memcpy(&rfdset, &open_fdset, sizeof(fd_set));
  memcpy(&wfdset, &open_fdset, sizeof(fd_set));
  memcpy(&efdset, &open_fdset, sizeof(fd_set));

  debug( "about to select(%d, ...) ", maxfd);
  errno = 0;
  n = select (maxfd+1, &rfdset, NULL, &efdset, NULL);

  debug( "select() returned %d errno=%d", n, errno);

  if (n < 0) {
    switch (errno) {
    case EINTR:
      Exit(NULL, 0);
      return 0;
      
    default:
      return n;
    };
  };

  /* if fd is NULL, nothing should be done to find the first ready fd. */
  if (fd == NULL) return n;

  /* maybe we should do all the reads first, but....  Maybe we should
     remember where we left off so that heavy traffic on a fd with low
     number don't block a fd with high number */

  for (i = 0; i <= maxfd; i ++) {
    if (FD_ISSET(i, &rfdset)) {
      *fd = i;
      if (operation != NULL) * operation = READREADY;
    };
    /*    if (FD_ISSET(i, &wfdset)) {
     *fd = i;
     if (operation != NULL) * operation = WRITEREADY;
     };*/
    if (FD_ISSET(i, &efdset)) {
      *fd = i;
      if (operation != NULL) * operation = EXCEPTREADY;
    };
  };
  return n;
};
