/*
  The MidWay API
  Copyright (C) 2000 Terje Eggestad

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

/*
 * $Id$
 * $Name$
 * 
 * $Log$
 * Revision 1.9  2004/11/17 20:58:08  eggestad
 * Large data buffers for IPC
 *
 * Revision 1.8  2004/08/10 19:39:09  eggestad
 * - shm heap is now 32/64 bit interoperable
 * - added large buffer alloc
 *
 * Revision 1.7  2003/07/06 22:06:14  eggestad
 * -debugging now in debug3
 * - added funcs for seting and geting ownerid
 *
 * Revision 1.6  2002/10/20 18:07:27  eggestad
 * added top and bottom offsets for legal buffer addresses, for _mwshmcheck
 *
 * Revision 1.5  2002/10/06 23:51:10  eggestad
 * bug in getchunksize, rather large, so a fixup in handling of size and verification
 *
 * Revision 1.4  2002/08/09 20:50:15  eggestad
 * A Major update for implemetation of events and Task API
 *
 * Revision 1.3  2002/07/07 22:45:48  eggestad
 * *** empty log message ***
 *
 * Revision 1.2  2002/02/17 13:53:14  eggestad
 * added prototypes for _mw_putbuffer_to_call(), _mw_getbuffer_from_call()
 *
 * Revision 1.1.1.1  2000/03/21 21:04:04  eggestad
 * Initial Release
 *
 * Revision 1.1.1.1  2000/01/16 23:20:12  terje
 * MidWay
 *
 */

/*

  (each) Shared memory heaps are formatet as follows
  (each line is a 32 bit word):

  The start of the segment has a header structure.
  +       +       +       +       
  +------------------------------+
  |    magic      | chunkpersize |
  |    basechunksize (octets)    |  <- refered to below as base.
  |    segment size (octets)     |
  |    semaphore id              |
  |    inuse root pointer        |
  |    inuse count;              |
  |    inuse highwater;          |
  |    inuse average;            |
  |    inuse averagecount;       |
  |    number of bins;           |
  |    chunk size = 1*base       | <--- these are arrays of size 
  /    ----- foreach bin -----   / <-|  "number of bins". Each bin
  |    freecounter size = 1      | <-|  holds chunks of the same size.
  /    ----- foreach bin -----   /  <-|  Sizes double for each bin
  |    startadr of bin size = 1  |  <-|  1*base, 2*base , 4*base etc.
  /    ----- foreach bin -----   /  <-|  
  |    endadr of bin size = 1    |  <-|  start and end adr is used to   
  /    ----- foreach bin -----   /  <-/  check if a chunk is corrupt.
  +------------------------------+
  
  The remainder of the heap shm segemts is filled with chuck formated as:

  +------------------------------+ <- start of chunk
  |     owner id (MWID, not pid) | (correction, pid as of this time)
  |     size (basechunksizes)    |
  /            DATA              / <- start of data
  /                              /
  |       above pointer          | <- shall point to start of chunk
  |       next pointer           | <-- used to make the double linked rings.
  |       prev pointer           | <-/
  +------------------------------+
  /                              /  (next chunk)
  /                              /
  +------------------------------+

  The list are ring lists, and not as in glibc where pre poimter
  points to the chuck above the current chunk. The latter is needed
  for mergeing with the chuck above. We define the abovepointer here
  but do not use it. Merging chunks are future use.
 
  Chuck size are set at runtime, this is for optimalization.  This
  scheme is effectivly the same as GLIBC's malloc.  However, since
  shared memory segments can't be resized, entire segments has to be
  added at run time.

  GLIBC's malloc also has the feature that it can split and join
  segment.  Since we have a mwd that (should) be mostly idle, this is
  something it should do. It must however free up buffers owned by
  dead members.

  Extremly large segments must be handled with a seperate scheme.
  Either by a pipe/socket or storing to disk (mmap()). glibc malloc
  does the later. Storing to disk is preferable to creating large
  shared memory segements, the latter since it will provoke
  pageing/swapping.

  Since many processes shall be clobbering around here we need a
  number of semaphores to prevent chaos. We only have one for each
  chunk list pointer in the header. We should really have more
  pointers to prevent blocking during mwd's garbage collect.

  Note: traditionally a transaction is done by passing messages with
  size < 10k. People tend to think larger goes againts the very idea
  of a transaction monitor, since speed is severly hampered by such
  large pieces of data. They tend to think that such data sizes should
  be sendt by ftp. I however recognice the need for sending images
  which almost never are < 10k. However, per default I think we should
  be resticted to 1M. 

  For version 1.0 we don't allow chuck partitioning or mergeing. what
  you booted with is what you have. We also postpone runtime adding of
  segments, as well as storing large segemtens to disk. We also
  postpone having several segments

*/

#ifndef _SHMALLOC_H
#define _SHMALLOC_H

#include <stdint.h>
#include <ipcmessages.h>

struct _chunkhead {
  MWID ownerid;
  int64_t size; /* in # of basechunksizes in IPC segements, bytes on mmap with one buffer */
};
typedef struct _chunkhead chunkhead ;

struct _chunkfoot {
  int64_t above;
  int64_t next;
  int64_t prev;
};
typedef struct _chunkfoot chunkfoot ;

/* the heap number that distinguish between many buffer heaps. 
   0 being shm
   1 - LOW_LARGE_BUFFER_NUMBER being heaps on files (currently unused)
   LOW_LARGE_BUFFER_NUMBER - INTMAX is single buffer files alloced by mwd
*/
#define LOW_LARGE_BUFFER_NUMBER 999

/* the struct used in shmalloc.c to keep track of buffer heaps. It's
   either ipc shm or a mmaped file. This struct is really only used in
   shmalloc.c, but _mwadr2offset() is used in event.c, but then we
   pass NULL for seginfo_t * */
struct shm_segment {
   int segmentid;
   int fd;
   void * start;
   void * end;
} ;

typedef struct shm_segment seginfo_t;


/* conversion between offset and adresses */
long _mwadr2offset(void *, seginfo_t *);
void * _mwoffset2adr(long, seginfo_t *);

chunkfoot * _mwfooter(chunkhead *);

int _mwshmcheck(void * adr);

#define CHUNKOVERHEAD sizeof(chunkhead) + sizeof(chunkfoot)
#define BINS 6

/* Magic number is "MW" thus thus 0x4D57 */
#define MWSEGMAGIC 0x4D57

struct segmenthdr {
  int16_t magic; 
  int16_t chunkspersize;
  int32_t basechunksize;
  int64_t segmentsize;
  int64_t semid;

  int64_t top, bottom; // the max and min offsets into the heap that may be buffers
  int32_t inusecount;
  int32_t inusehighwater;
  int32_t inuseaverage;
  int32_t inuseavgcount;
  int32_t numbins;
  int32_t chunk[BINS];
  int32_t freecount[BINS];
};

int _mw_gettopofbin(struct segmenthdr * seghdr, int bin);

int _mw_detach_mmap(seginfo_t * si);
seginfo_t * _mw_getsegment_byid(int segid);
seginfo_t * _mw_getsegment_byaddr(void * addr);
seginfo_t *  _mw_addsegment(int id, int fd, void * start, void * end);

void * _mwalloc(size_t size);
void * _mwrealloc(void * adr, size_t newsize);
int _mwfree(void * adr);

int _mw_getbuffer_from_call (mwsvcinfo * svcreqinfo, Call * callmesg);
int _mw_putbuffer_to_call (Call * callmesg, char * data, size_t len);

int _mw_fastpath_enabled(void) ;

int _mwshmcheck(void * adr);
size_t _mwshmgetsizeofchunk(void * adr);

int _mwshmgetowner(void * adr, MWID * id);
int _mwshmsetowner(void * adr, MWID id);

static inline size_t get_pagesize(void)
{
   static size_t pgsz = 0;
   
   if (pgsz == 0) {
      pgsz = sysconf(_SC_PAGESIZE);
   };
   return pgsz;
};



#endif /* _SHMALLOC_H */



