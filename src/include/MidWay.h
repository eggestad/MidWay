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

/* gcc hack in order to avoid unused warnings (-Wunused) on cvstags */
#ifdef __GNUC__
#define UNUSED __attribute__ ((unused))
#else
#define UNUSED 
#endif


#include <stdlib.h>
#include <stdio.h>

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

/* here we introduce some short forms for the mwlog() calls. These are
   here and not the main MidWay.h so that we don't clobber the
   namespace to user applications */
#ifndef NDEBUG
#define DEBUG(m...)  mwlog(MWLOG_DEBUG,  __FUNCTION__ "(): " m)
#define DEBUG1(m...) mwlog(MWLOG_DEBUG1, __FUNCTION__ "(): " m)
#define DEBUG2(m...) mwlog(MWLOG_DEBUG2, __FUNCTION__ "(): " m)
#define DEBUG3(m...) mwlog(MWLOG_DEBUG3, __FUNCTION__ "(): " m)
#define DEBUG4(m...) mwlog(MWLOG_DEBUG4, __FUNCTION__ "(): " m)

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


#ifdef TIMEPEGS

#define TIMEPEGNOTE(note) __timepeg(__FUNCTION__, __FILE__, __LINE__, note)
#define TIMEPEG __timepeg(__FUNCTION__, __FILE__, __LINE__, NULL)
void  __timepeg(char * function, char * file, int line, char * note);
void timepeg_clear(void);
int timepeg_sprint(char * buffer, size_t size);
void timepeg_log(void);

#else

#define TIMEPEGNOTE(note)
#define TIMEPEG

#define timepeg_clear()
#define timepeg_sprint(a,b)
#define timepeg_log()

#endif


#endif

