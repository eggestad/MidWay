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
 * Revision 1.1  2001/09/15 23:40:09  eggestad
 * added the broker daemon
 *
 */

#ifndef _MWBD_C
extern int debugging; 
#endif 

#ifndef _MWBD_H
#define _MWBD_H

#include <stdio.h>
#include <syslog.h>

#ifdef DEBUG
#define error(m...) do { \
fprintf(stderr, "ERROR: " __FILE__ ":%d:" __FUNCTION__ ": ", __LINE__);\
fprintf(stderr, m); \
fprintf(stderr, "\n"); } while(0)
 
#define warn(m...) do { \
fprintf(stderr, "WARNING: " __FILE__ ":%d:" __FUNCTION__ ": ", __LINE__);\
fprintf(stderr, m); \
fprintf(stderr, "\n"); } while(0)

#define debug(m...) do { \
fprintf(stderr, "DEBUG: " __FILE__ ":%d:" __FUNCTION__ ": ", __LINE__);\
fprintf(stderr, m); \
fprintf(stderr, "\n"); } while(0)

#define info(m...) do { \
fprintf(stderr, "INFO: " __FILE__ ":%d:" __FUNCTION__ ": ", __LINE__);\
fprintf(stderr, m); \
fprintf(stderr, "\n"); } while(0)

#else 
#define error(m...)   syslog(LOG_ERR, m)
#define warn(m...)    syslog(LOG_WARNING, m)
#define debug(m...)   do { if (debugging) syslog(LOG_DEBUG, m); } while(0)
#define info(m...)    syslog(LOG_INFO, m)
#endif /* DEBUG */


#define MWMAXNAMELEN 64 + 1

#ifdef DEBUG
#define UNIXSOCKETPATH "/tmp/mwbd" 
#else 
#define UNIXSOCKETPATH "/var/run/mwbd.socket"
#endif

#define TRACEFILE1     "/var/log/mwbd.trace"
#define TRACEFILE2     "/tmp/mwbd.trace"

#endif

void Exit(char *, int);
