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
 * Revision 1.11  2005/06/25 12:06:53  eggestad
 * added doxygen doc
 *
 * Revision 1.10  2005/06/13 23:21:04  eggestad
 * Added doxygen comments
 *
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

/**
   @file
   The layout of the IPC shmsegment are described here. 
   
  (each) Shared memory heaps are formatet as follows
  (each line is a 32 bit word):

  The start of the segment has a header structure.
  @verbatim
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
  @endverbatim

  The remainder of the heap shm segemts is filled with chuck formated as:
  
  @verbatim
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
  @endverbatim

  The list are ring lists, and not as in glibc where pre pointer
  points to the chuck above the current chunk. The latter is needed
  for merging with the chuck above. We define the above pointer here
  but do not use it. Merging chunks are future use.
 
  Chuck size are set at runtime, this is for optimization.  This
  scheme is effectively the same as GLIBC's malloc.  However, since
  shared memory segments can't be resized, entire segments has to be
  added at run time.

  GLIBC's malloc also has the feature that it can split and join
  segment.  Since we have a mwd that (should) be mostly idle, this is
  something it should do. It must however free up buffers owned by
  dead members.

  Extremely large segments must be handled with a separate scheme.
  Either by a pipe/socket or storing to disk (mmap()). glibc malloc
  does the later. Storing to disk is preferable to creating large
  shared memory segments, the latter since it will provoke
  paging/swapping.

  Since many processes shall be clobbering around here we need a
  number of semaphores to prevent chaos. We only have one for each
  chunk list pointer in the header. We should really have more
  pointers to prevent blocking during mwd's garbage collect.

  Note: traditionally a transaction is done by passing messages with
  size < 10k. People tend to think larger goes against the very idea
  of a transaction monitor, since speed is severely hampered by such
  large pieces of data. They tend to think that such data sizes should
  be sent by ftp. I however recognize the need for sending images
  which almost never are < 10k. However, per default I think we should
  be restricted to 1M.

  For version 1.0 we don't allow chuck partitioning or merging. what
  you booted with is what you have. We also postpone runtime adding of
  segments, as well as storing large segments to disk. We also
  postpone having several segments

*/

#ifndef _SHMALLOC_H
#define _SHMALLOC_H

#include <stdint.h>
#include <ipcmessages.h>

/**
   The struct at the top of a chunk. 

   The chunk pointer/offset is immediately after this header. 
*/
struct _chunkhead {
   MWID ownerid; //!< The MWID thaht owns the chunk, #UNASSIGNED if free
   int64_t size; //!< in # of basechunksizes in IPC segements, bytes on mmap with one buffer 
};
/** A typedef for the struct _chunkhead */
typedef struct _chunkhead chunkhead ;

/**
   The structure at the end of every chunk.
   
   If keeps an offset to the start of chunk above (for sanity
   checking) and the next prov for a double linked link.
*/
struct _chunkfoot {
   int64_t above; //!< the offset of the start of chunk on the chunk above in the segment. 
   int64_t next;  //!< next in a DDL
   int64_t prev;  //!< prev in a DDL
};
/** A typedef for the struct _chunkfoot */
typedef struct _chunkfoot chunkfoot ;

/**
   The heap number that distinguish between many buffer heaps. 
   0 being shm
   1 - LOW_LARGE_BUFFER_NUMBER being heaps on files (currently unused)
   LOW_LARGE_BUFFER_NUMBER - INTMAX is single buffer files alloced by mwd
*/
#define LOW_LARGE_BUFFER_NUMBER 999

/**
   The struct used in shmalloc.c to keep track of buffer heaps. It's
   either ipc shm or a mmaped file. This struct is really only used in
   shmalloc.c, but _mwadr2offset() is used in event.c, but then we
   pass NULL for seginfo_t * */
struct shm_segment {
   int segmentid; //!< The segment of this shm or mmap
   int fd;        //!< the fd of the mmap'ed file, #UNASSIGNED if shm
   void * start;  //!< The top (lowest) address that mey be a buffer
   void * end;    //!< The bottom (highest) address that mey be a buffer
} ;

/** A typedef for the struct shm_segment */
typedef struct shm_segment seginfo_t;


/* conversion between offset and adresses */
long _mwadr2offset(void *, seginfo_t *);
void * _mwoffset2adr(long, seginfo_t *);

chunkfoot * _mwfooter(chunkhead *);

int _mwshmcheck(void * adr);

/** The # of bytes wasted to header and footer */
#define CHUNKOVERHEAD sizeof(chunkhead) + sizeof(chunkfoot)
/** The number of bins, really should be settable thru mwd(). */
#define BINS 6

/** Magic number used in segement header.  This is for sanity checking
    to find out if a mmap or shm is used in MidWay. The Magic is "MW"
    thus thus 0x4D57 */
#define MWSEGMAGIC 0x4D57

/** 
    The header struct for shm and mmap heaps. Single buffer mmap'ed files do not have this struct. 
*/
struct segmenthdr {
   int16_t magic;         //!< #MWSEGMAGIC
   int16_t chunkspersize; //!< The number of chunks per size/BIN, set be mwd(). 
   int32_t basechunksize; //!< The size in bytes of the base chunk for BIN \a n size is \a *2^n
   int64_t segmentsize;   //!< The total segement size in bytes
   int64_t semid;         //!< The SYSVIPC id for the semaphore used to lock the BINS.  

   int64_t top;           //!< the min offset into the heap/segement that may be buffers
   int64_t bottom;        //!< the max offset into the heap/segement that may be buffers
   int32_t inusecount;    //!< The number of buffers currently in use
   int32_t inusehighwater;//!< The highest number of chunks thath ever has been in use at the same time
   int32_t inuseaverage;  //!< The average inuse count
   int32_t inuseavgcount; 
   int32_t numbins;       //!< The number of bins currently must be #BINS
   int32_t chunk[BINS];   //!< The root offset(pointer) to the in use chucks for the bins
   int32_t freecount[BINS]; //!< The root offset(pointer) to the free lists for the bins
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




/*  LocalWords:  malloc
 */
