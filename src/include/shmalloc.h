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

#include <ipcmessages.h>

struct _chunkhead {
  long ownerid;
  long size; /* in basechunksizes */
};
typedef struct _chunkhead chunkhead ;

struct _chunkfoot {
  int above;
  int next;
  int prev;
};
typedef struct _chunkfoot chunkfoot ;

/* conversion between offset and adresses */
int _mwadr2offset(void *);
void * _mwoffset2adr(int);

chunkfoot * _mwfooter(chunkhead *);

int _mwshmcheck(void * adr);

#define CHUNKOVERHEAD sizeof(chunkhead) + sizeof(chunkfoot)
#define BINS 6

struct segmenthdr {
  short magic; 
  short chunkspersize;
  long basechunksize;
  long segmentsize;
  long semid;

  int inusecount;
  int inusehighwater;
  int inuseaverage;
  int inuseavgcount;
  int numbins;
  int chunk[BINS];
  int freecount[BINS];
};


void * _mwalloc(int size);
void * _mwrealloc(void * adr, int newsize);
int _mwfree(void * adr);

int _mw_getbuffer_from_call (mwsvcinfo * svcreqinfo, Call * callmesg);
int _mw_putbuffer_to_call (Call * callmesg, char * data, int len);

#endif /* _SHMALLOC_H */



