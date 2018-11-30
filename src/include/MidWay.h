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

/** @file
    This is the MidWay.h for internal use in the library, we really should rename to libMidWay.h or MidWayImpl.h or something...


    We add on config.h and a lot of macros, debugging macros, etc
 */


#ifndef _MIDWAY_INTERNAL_H
#define _MIDWAY_INTERNAL_H

#include <assert.h>


#include "../../include/MidWay.h"

#include "../../config.h"
#include "acconfig.h"


/** @defgroup mwid MWID - The MidWay ID
    One thing that is used almost everywhere is the \a MWID.
  
    The different type #CLIENTID, #SERVERID, #SERVICEID,
    #GATEWAYID, defined in the include/MidWay.h are unioned by
    #MWID. If we'd been object oriented MWID would be the super class of all other IDs. 
 

    Different MidWay entities are uniquely identfied by their id. The
    \a MWID is a 32 bit number where the top 8 bit identify the type
    of entity and the lower 24 bit is the index.

    The different entities are normally in a IPC table maintained by
    ipctable.c, where the index give the row. There are access
    functions to get the entries thru ipctables.c.
  
    The MW*MASK and #MWINDEXMASK are no longer intended for use
    outside src/include/MidWay.h

//@{

*/


//! The bit indicating that the \a MWID is a server
#define MWSERVERMASK     0x01000000
//! The bit indicating that the \a MWID is a client
#define MWCLIENTMASK     0x02000000
//! The bit indicating that the \a MWID is a gateway
#define MWGATEWAYMASK    0x04000000
//! The bit indicating that the \a MWID is a service
#define MWSERVICEMASK    0x08000000
//! This mask filter out the index part of \a MWID
#define MWINDEXMASK      0x00FFFFFF

//! The MWID which is the generic type for holding all type of ID's. 
typedef int MWID;

/*
 * I tried for a while to keep the SERVICE/SERVER/CLIENT/GATEWAYID to
 * be the index, without the flags set, and introduce a MWID where we
 * checked the mask to see what it was.  (typically to distinguish
 * between server and gateway.  I've however abandoned that though,
 * and for all *ID the type mask shall ALWAYS be set. We introducer
 * the index "type" (int) that are the unmasked number.  The safety of
 * first of all mostly separate variables for the ID, and then check
 * to verify that it's the correct type just appeal to me. Thus these
 * macros are now obsolete.
 */

/** @defgroup mwid2idx Getting the index from an ID 
    These macros is the recommend way of getting the index part of a \a MWID. 
    If the \a MWID is of a 

    @ingroup mwid
//@{
*/

//! Get the index part of clientid, #UNASSIGNED if not a clientid
#define CLTID2IDX(id)  ((id != UNASSIGNED) ? ((id & MWCLIENTMASK)  > 0 ? (id & MWINDEXMASK) : UNASSIGNED) : UNASSIGNED)
//! Get the index part of serverid, #UNASSIGNED if not a serverid
#define SRVID2IDX(id)  ((id != UNASSIGNED) ? ((id & MWSERVERMASK)  > 0 ? (id & MWINDEXMASK) : UNASSIGNED) : UNASSIGNED)
//! Get the index part of serviceid, #UNASSIGNED if not a serviceid
#define SVCID2IDX(id)  ((id != UNASSIGNED) ? ((id & MWSERVICEMASK) > 0 ? (id & MWINDEXMASK) : UNASSIGNED) : UNASSIGNED)
//! Get the index part of gatewayid, #UNASSIGNED if not a gatewayid
#define  GWID2IDX(id)  ((id != UNASSIGNED) ? ((id & MWGATEWAYMASK) > 0 ? (id & MWINDEXMASK) : UNASSIGNED) : UNASSIGNED)
//@}

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

char * _mwid2str(MWID id, char * buffer);

//@}

/************************************************************************
 * other utilities
 */

//! min for any numeric args 
#define min(a,b) (a < b ? a : b)
//! max for any numeric args 
#define max(a,b) (a > b ? a : b)


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
   just plain abort if that happen */
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

#define malloc(s) debug_malloc(__FILE__, __LINE__, s)
#define realloc(p,s) debug_realloc(__FILE__, __LINE__, p, s)
#define free(p) debug_free(__FILE__, __LINE__, p)
#define strdup(p) debug_strdup(__FILE__, __LINE__, p)

#else 

#define malloc(s) x_malloc(s)
#define realloc(p,s) x_realloc(p, s)

#endif

// timepegs which are defined in lib/utils.c

/**
@defgroup timing Timing 

These macros are for use during development cycle to optimize code for
speed. Define TIMEPEGS to enable. When timing a piece of code first to
a timepeg_clear() to clear the trace table, the place #TIMEPEG or
#TIMEPEGNOTE thru the code you want to time, and finally to a
timepeg_sprint() or a timepeg_log() to get the table printed.

//@{
*/

#ifdef TIMEPEGS

/** Record a timepeg with a note. Use if you want a the \a note to
    appear in he table when you print the trace.
 */
#define TIMEPEGNOTE(note) __timepeg(__FUNCTION__, __FILE__, __LINE__, note)
/** Record a timepeg. */
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

//@}

// debugging  macros

/**
   @defgroup debug Debugging 

   We got a set of debugging macros defined here, first and formost we
   got a set on aliases for mwlog(MWLOG_*, ...);

   The DEBUGs() print DEBUG<em>E<\em>: \e function : \e linenumber, the Info, Error, etc do not. 

   When you are developing you tend to use DEBUG and DEBUG1, then
   after code is becomming stable migrate messages need for debugging
   this part to DEBUG2 and DEBUG3, while leaving messages interesting
   to the complete operation in DEBUG/DEBUG1


   To turn off all DEBUG*() define NDEBUG, it will speed up the code. 
//@{
*/


#include <stdarg.h>

void _mw_vlogf(int level, const char * format, va_list ap); // in mwlog.c

/* here we introduce some short forms for the mwlog() calls. These are
   here and not the main MidWay.h so that we don't clobber the
   namespace to user applications */
//#define NDEBUG


#ifndef NDEBUG

int * _mwgetloglevel(void);

static int * debuglevel = NULL;
int _mwstr2loglevel(const char *);

#define PRINTF_ATTR __attribute__ ((format (printf, 5, 6)))
#define DEPRECATED __attribute__ ((deprecated))

#ifndef PRINTF_ATTR
#define PRINTF_ATTR
#endif
static inline int _DEBUGN(int N, const char * func, const char * file, int line, char * m, ...) PRINTF_ATTR;

static inline int _DEBUGN(int N, const char * func, const char * file, int line, char * m, ...)
{
  va_list ap;
  char buffer[4096];
  
  if (debuglevel == NULL) debuglevel = _mwgetloglevel();
  if (*debuglevel < MWLOG_DEBUG + N) return 0;

  timepeg_pause(); // we're attempting to remove debugging overhead from timepegs bookkeeping
  
  va_start(ap, m);
  if(strlen(m) > 4000) {
     timepeg_resume(); 
     return 0;
  }
  sprintf(buffer, "%s(%d): %s", func, line, m);
  _mw_vlogf(MWLOG_DEBUG + N, buffer, ap);
  
  va_end(ap);
  timepeg_resume(); 
  return 0;
};

static inline int _CALLTRACE(int up_down, const char * func)
{
  char buffer[128];
  static  __thread int calllevel = 0;

  static char * enterstr = ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>";
  static char * leavestr = "<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<";

  if (debuglevel == NULL) debuglevel = _mwgetloglevel();
  if (*debuglevel < MWLOG_DEBUG1) return 0;

  timepeg_pause(); // we're attempting to remove debugging overhead from timepegs bookkeeping
  
  if (up_down) calllevel++;
  else calllevel--;
  
  sprintf(buffer, "%s%*.*s[%d] %s", up_down?">Enter":">Leave", calllevel, calllevel, up_down?enterstr:leavestr, calllevel, func);

  mwlog(MWLOG_DEBUG1, "%s", buffer);
  
  timepeg_resume(); 
  return 0;
};
/** Debug level 0, to be used in non verbose debugging from utiliies */
#define DEBUG(m...)  _DEBUGN(0, __FUNCTION__, __FILE__, __LINE__, m)
/** Debug level 1, to be used in non verbose debugging from libraries */
#define DEBUG1(m...) _DEBUGN(1, __FUNCTION__ , __FILE__, __LINE__, m)
/** Debug level 2, to be used in verbose debugging from utiliies */
#define DEBUG2(m...) _DEBUGN(2, __FUNCTION__ , __FILE__, __LINE__, m)
/** Debug level 3, to be used in verbose debugging from library */
#define DEBUG3(m...) _DEBUGN(3, __FUNCTION__ , __FILE__, __LINE__, m)
/** Debug level 4, to be used in really really verbose debugging */
#define DEBUG4(m...) _DEBUGN(4, __FUNCTION__ , __FILE__, __LINE__, m)

#define ENTER(m...) _CALLTRACE(1, __FUNCTION__ , ## m)
#define LEAVE(m...) _CALLTRACE(0, __FUNCTION__ , ## m)
#else  // ifndef NDEBUG
#define DEBUG(m...)
#define DEBUG1(m...)
#define DEBUG2(m...)
#define DEBUG3(m...)
#define DEBUG4(m...)
#define ENTER(m...) 
#define LEAVE(m...)
#endif // ifndef NDEBUG

#ifndef DEPRECATED
#define DEPRECATED
#endif

/** Info level log. */
#define Info(m...)    mwlog(MWLOG_INFO, m)
/** Warning level log */
#define Warning(m...) mwlog(MWLOG_WARNING, m)
/** Error level log */
#define Error(m...)   mwlog(MWLOG_ERROR, m)

/** Fatal leve log and call abort() */
#define Fatal(m...)   do { mwlog(MWLOG_FATAL, m); abort(); } while (0)

/** do an assert with Fatal level log and abort */
#define Assert(test) do { if (!(test)) {mwlog(MWLOG_FATAL, "Internal error: (%s) fails in %s() %s:%d", \
#test, __func__, __FILE__, __LINE__); abort(); }} while (0)
//@}

/* Mutex  funtions */
#ifdef HAVE_LIBPTHREAD
#include <pthread.h>
#define USETHREADS
#endif

#ifdef USETHREADS

/**
   Wrapper for pthread mutex declare. Module local.
   We have a set of wrappers for thread libraries, this is for pthread. 
*/
#define DECLAREMUTEX(name) static pthread_mutex_t name = PTHREAD_MUTEX_INITIALIZER
//! Wrapper for pthread mutex declare within a struct
#define DECLARESTRUCTMUTEX(name) pthread_mutex_t name
//! Wrapper for global pthread mutex declare
#define DECLAREGLOBALMUTEX(name) pthread_mutex_t name = PTHREAD_MUTEX_INITIALIZER
//! Wrapper for extern pthread mutex declare
#define DECLAREEXTERNMUTEX(name) extern pthread_mutex_t name

#define _LOCKMUTEX(name)   do {pthread_mutex_lock(&name); }   while(0)
#define _UNLOCKMUTEX(name) do {pthread_mutex_unlock(&name); } while(0)
//! Wrapper for pthread mutex lock
#define LOCKMUTEX(name)    do { DEBUG3("locking mutex " #name " ..." );  \
pthread_mutex_lock(&name); DEBUG1("locked mutex " #name);  } while(0)
//! Wrapper for pthread mutex unlock
#define UNLOCKMUTEX(name)  do { DEBUG1("unlocking mutex " #name); pthread_mutex_unlock(&name); } while(0)
#else 
#error "PTHREADS are currently required"
#endif


#endif

