/*
  The MidWay API
  Copyright (C) 2000, 2002 Terje Eggestad

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

/* this is the MidWay.h for internal MidWay use. We add on config.h ++ */

#ifndef _MIDWAY_INTERNAL_H
#define _MIDWAY_INTERNAL_H

#include <assert.h>

#include "../../include/MidWay.h"

#include "../../config.h"


/* The max number of each ID are limitet to 24 bits, we use the top 
 * 8 bit to disdingish among them, the lower 24 bits are the index into 
 * their respective tables. */
#define MWSERVERMASK     0x01000000
#define MWCLIENTMASK     0x02000000
#define MWGATEWAYMASK    0x04000000
#define MWSERVICEMASK    0x08000000
#define MWINDEXMASK      0x00FFFFFF

typedef int MWID;

/*
 * I tried for a  while to keep the SERVICE/SERVER/CLIENT/GATEWAYID to
 * be the index, without the  flags set, and introduce a MWID where we
 *  checked the mask  to see  what it  was.  (typically  to distiguius
 * between  server and gateway.   I've however abandoned  that though,
 * and for all *ID the type mask shall ALWAYS be set. We intruduce the
 * index  "type" (int)  that are the  unmasked number.  The  safety of
 * first of all mostly seperate  variables foir the ID, and then check
 * to verify  that it's the correct type just apeal  to me. THus these
 * macros are now obsiolete.

#define MWID2CLTID(id)  ((id & MWCLIENTMASK)  >= 0 ? (id&MWINDEXMASK):UNASSIGNED)
#define MWID2SRVID(id)  ((id & MWSERVERMASK)  >= 0 ? (id&MWINDEXMASK):UNASSIGNED)
#define MWID2SVCID(id)  ((id & MWSERVICEMASK) >= 0 ? (id&MWINDEXMASK):UNASSIGNED)
#define MWID2GWID(id)   ((id & MWGATEWAYMASK) >= 0 ? (id&MWINDEXMASK):UNASSIGNED)
 
#define CLTID2MWID(id)  (id | MWCLIENTMASK) 
#define SRVID2MWID(id)  (id | MWSERVERMASK)  
#define SVCID2MWID(id)  (id | MWSERVICEMASK) 
#define GWID2MWID(id) (id | MWGATEWAYMASK) */

/* get the index or unassigned if wrong mask set */
#define CLTID2IDX(id)  ((id != UNASSIGNED) ? ((id & MWCLIENTMASK)  >= 0 ? (id & MWINDEXMASK) : UNASSIGNED) : UNASSIGNED)
#define SRVID2IDX(id)  ((id != UNASSIGNED) ? ((id & MWSERVERMASK)  >= 0 ? (id & MWINDEXMASK) : UNASSIGNED) : UNASSIGNED)
#define SVCID2IDX(id)  ((id != UNASSIGNED) ? ((id & MWSERVICEMASK) >= 0 ? (id & MWINDEXMASK) : UNASSIGNED) : UNASSIGNED)
#define  GWID2IDX(id)  ((id != UNASSIGNED) ? ((id & MWGATEWAYMASK) >= 0 ? (id & MWINDEXMASK) : UNASSIGNED) : UNASSIGNED)

/* filters */
#define CLTID(id)  ((id & MWCLIENTMASK)  != 0 ? id : UNASSIGNED)
#define SRVID(id)  ((id & MWSERVERMASK)  != 0 ? id : UNASSIGNED)
#define SVCID(id)  ((id & MWSERVICEMASK) != 0 ? id : UNASSIGNED)
#define  GWID(id)  ((id & MWGATEWAYMASK) != 0 ? id : UNASSIGNED)

/* turn a given idex into a type. */
#define IDX2CLTID(id)  ((id & MWINDEXMASK) | MWCLIENTMASK)  
#define IDX2SRVID(id)  ((id & MWINDEXMASK) | MWSERVERMASK)  
#define IDX2SVCID(id)  ((id & MWINDEXMASK) | MWSERVICEMASK) 
#define  IDX2GWID(id)  ((id & MWINDEXMASK) | MWGATEWAYMASK) 

#ifdef __GNUC__
/* gcc hack in order to avoid unused warnings (-Wunused) on cvstags */
#define UNUSED __attribute__ ((unused))
/* gcc hack in order to get wrong arg type in mwlog() */
#define FORMAT_PRINTF __attribute__ ((format (printf, 2, 3)))
#else
#define UNUSED 
#define FORMAT_PRINTF 
#endif


#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* here we wrap malloc, realloc and catch out of memory error, and
   just plain abort if taht happen */
static inline void * x_malloc(size_t size)
{
  void * nptr;
  nptr =  malloc(size);

  if (nptr == NULL) {
    mwlog(MWLOG_FATAL, "Out of memory! aborting...");
    abort();
  };
  return nptr;
};

static inline void * x_realloc(void *ptr, size_t size)
{
  void * nptr;
  nptr = realloc(ptr, size);
  if (nptr == NULL) {
    mwlog(MWLOG_FATAL, "Out of memory! aborting...");
    abort();
  };
  return nptr;
};

/* malloc debugging, we just print out the addresses when *alloc()ing
   and free()ing */
#ifdef DEBUG_MALLOC

static inline void * debug_malloc(char * file, int line, size_t size)
{
  void * nptr;
  nptr =  x_malloc(size);
  mwlog(MWLOG_DEBUG2, "@@@@@@@@@ %s:%d @@@@@@@@@    malloc(%d) => %p\n", file, line, size, nptr);
  return nptr;
};

static inline void * debug_realloc(char * file, int line, void *ptr, size_t size)
{
  void * nptr;
  nptr = x_realloc(ptr, size);
  mwlog(MWLOG_DEBUG2, "@@@@@@@@@ %s:%d @@@@@@@@@    realloc (%p, %d) => %p\n", file, line, ptr, size, nptr);
  return nptr;
};

static inline void debug_free(char * file, int line, void *ptr)
{
  mwlog(MWLOG_DEBUG2, "@@@@@@@@@ %s:%d @@@@@@@@@    freeing %p\n", file, line, ptr);  
  free(ptr);
};



#define malloc(s) debug_malloc(__FILE__, __LINE__, s)
#define realloc(p,s) debug_realloc(__FILE__, __LINE__, p, s)
#define free(p) debug_free(__FILE__, __LINE__, p)

#else 

#define malloc(s) x_malloc(s)
#define realloc(p,s) x_realloc(p, s)

#endif

// timepegs which are defined in lib/utils.c

#ifndef TIMEPEGS
#ifdef DEBUGGING
#define TIMEPEGS
#endif
#endif

#ifdef TIMEPEGS

#define TIMEPEGNOTE(note) __timepeg(__FUNCTION__, __FILE__, __LINE__, note)
#define TIMEPEG() __timepeg(__FUNCTION__, __FILE__, __LINE__, NULL)
void  __timepeg(char * function, char * file, int line, char * note);
void timepeg_clear(void);
int timepeg_sprint(char * buffer, size_t size);
void timepeg_log(void);

void _perf_pause(void);
void _perf_resume(void);
#define timepeg_pause() _perf_pause()
#define timepeg_resume() _perf_resume()

#else

#define TIMEPEGNOTE(note)
#define TIMEPEG()

#define timepeg_pause()
#define timepeg_resume()
#define timepeg_clear()
#define timepeg_sprint(a,b)
#define timepeg_log()

#endif

// debugging  macros

#include <stdarg.h>

void _mw_vlogf(int level, char * format, va_list ap); // in mwlog.c

/* here we introduce some short forms for the mwlog() calls. These are
   here and not the main MidWay.h so that we don't clobber the
   namespace to user applications */
//#define NDEBUG

#ifndef NDEBUG

int * _mwgetloglevel(void);

static int * debuglevel = NULL;

static inline int _DEBUGN(int N, char * func, char * file, int line, char * m, ...)
{
  va_list ap;
  char buffer[4096];
  
  if (debuglevel == NULL) debuglevel = _mwgetloglevel();
  if (*debuglevel < MWLOG_DEBUG + N) return 0;
  
  timepeg_pause(); // we're attempting to remove debugging overhead from timepegs bookkeeping
  va_start(ap, m);
  if(strlen(m) > 4000) return 0;

  sprintf(buffer, "%s(%d): %s", func, line, m);
  _mw_vlogf(MWLOG_DEBUG + N, buffer, ap);
  
  va_end(ap);
  timepeg_resume(); 
  return 0;
};

#define DEBUG(m...)  _DEBUGN(0, __FUNCTION__, __FILE__, __LINE__, m)
#define DEBUG1(m...) _DEBUGN(1, __FUNCTION__ , __FILE__, __LINE__, m)
#define DEBUG2(m...) _DEBUGN(2, __FUNCTION__ , __FILE__, __LINE__, m)
#define DEBUG3(m...) _DEBUGN(3, __FUNCTION__ , __FILE__, __LINE__, m)
#define DEBUG4(m...) _DEBUGN(4, __FUNCTION__ , __FILE__, __LINE__, m)

#else 
#define DEBUG(m...)
#define DEBUG1(m...)
#define DEBUG2(m...)
#define DEBUG3(m...)
#define DEBUG4(m...)
#endif

#define Info(m...)    mwlog(MWLOG_INFO, m)
#define Warning(m...) mwlog(MWLOG_WARNING, m)
#define Error(m...)   mwlog(MWLOG_ERROR, m)

#define Fatal(m...)   do { mwlog(MWLOG_FATAL, m); abort(); } while (0)
#define Assert(test) do { if (!(test)) {mwlog(MWLOG_FATAL, "Internal error: %s fails in %s() %s:%d", \
#test, __func__, __FILE__, __LINE__); abort(); }} while (0)

/* Mutex  funtions */
#ifdef HAVE_LIBPTHREAD
#include <pthread.h>
#define USETHREADS
#endif

#ifdef USETHREADS

#define DECLAREMUTEX(name) static pthread_mutex_t name = PTHREAD_MUTEX_INITIALIZER
#define DECLAREGLOBALMUTEX(name)  pthread_mutex_t name = PTHREAD_MUTEX_INITIALIZER
#define _LOCKMUTEX(name)   do {pthread_mutex_lock(&name); }   while(0)
#define _UNLOCKMUTEX(name) do {pthread_mutex_unlock(&name); } while(0)
#define LOCKMUTEX(name)    do { DEBUG1("locking mutex " #name " ..." );  \
pthread_mutex_lock(&name); DEBUG1("locked mutex " #name);  } while(0)
#define UNLOCKMUTEX(name)  do { DEBUG1("unlocking mutex " #name); pthread_mutex_unlock(&name); } while(0)
#else 
#error "PTHREADS are currently required"
#endif


#endif

