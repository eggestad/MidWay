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
  License along with the MidWay distribution; see the file COPYING.  If not,
  write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA. 
*/

/*
 * $Id$
 * $Name$
 * 
 * $Log$
 * Revision 1.20  2004/12/14 19:06:48  eggestad
 * mwalloc() returned wrong size buffer in some cases
 *
 * Revision 1.19  2004/11/17 20:58:08  eggestad
 * Large data buffers for IPC
 *
 * Revision 1.18  2004/08/11 19:01:36  eggestad
 * - shm heap is now 32/64 bit interoperable
 * - added large buffer alloc
 *
 * Revision 1.17  2004/04/12 22:56:50  eggestad
 * *** empty log message ***
 *
 * Revision 1.16  2004/04/12 12:53:41  eggestad
 * _mw_putbuffer_to_call() failed if data is "", caused mwalloc(0), which returned -ENOMEM
 *
 * Revision 1.15  2004/02/19 23:42:11  eggestad
 * setting of correct owner id in _mwalloc()
 *
 * Revision 1.14  2003/07/20 23:16:19  eggestad
 * - major corruption fix:
 *  * added bounds check functions
 *  * push got into wrong bin, probably double to int conv problem (weird)
 *  * imporved perf of pushchunk
 *  * made debugging a little more intelligent
 *
 * Revision 1.13  2003/07/06 22:06:14  eggestad
 * -debugging now in debug3
 * - added funcs for seting and geting ownerid
 *
 * Revision 1.12  2003/06/12 07:22:19  eggestad
 * fix for negative size
 *
 * Revision 1.11  2002/11/19 12:43:54  eggestad
 * added attribute printf to mwlog, and fixed all wrong args to mwlog and *printf
 *
 * Revision 1.10  2002/10/20 18:13:52  eggestad
 * added sanity check
 *
 * Revision 1.9  2002/10/06 23:51:10  eggestad
 * bug in getchunksize, rather large, so a fixup in handling of size and verification
 *
 * Revision 1.8  2002/08/09 20:50:15  eggestad
 * A Major update for implemetation of events and Task API
 *
 * Revision 1.7  2002/07/07 22:35:20  eggestad
 * *** empty log message ***
 *
 * Revision 1.6  2002/02/17 14:23:31  eggestad
 * - added missing includes
 * - added _mw_getbuffer_from_call() and _mw_putbuffer_to_call()
 * 	These now hide fastpath. If fast path pointers point to shm buffers.
 *
 * Revision 1.5  2001/10/16 16:18:09  eggestad
 * Fixed for ia64, and 64 bit in general
 *
 * Revision 1.4  2000/09/21 18:45:10  eggestad
 * bug fix: core dump if SRB since _mwHeap was not tested for NULL
 *
 * Revision 1.3  2000/08/31 21:56:00  eggestad
 * DEBUG level set propper. moved out test for malloc()
 *
 * Revision 1.2  2000/07/20 19:36:21  eggestad
 * core dump on mwfree() on malloc()'d buffer fix.
 *
 * Revision 1.1.1.1  2000/03/21 21:04:16  eggestad
 * Initial Release
 *
 * Revision 1.1.1.1  2000/01/16 23:20:12  terje
 * MidWay
 *
 */


#include <sys/sem.h>
#include <sys/shm.h>
#include <errno.h>
#include <stdio.h> 
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <MidWay.h>
#include <shmalloc.h>
#include <ipctables.h>
#include <ipcmessages.h>

static char * RCSId UNUSED = "$Id$";
static char * RCSName UNUSED = "$Name$"; /* CVS TAG */

/* a note on threads. Since shmalloc, shmrealloc, and shmfree
   deal in shared memory they must be multi process proof.
   Since a thread in another process this module must be thread safe. */

/* this need to change for more segments, note module entry functions 
   are responsible for checking for heapinfo == NULL*/

/* Here I do something I otherwise selvdom do, I prefix each variable with 
   data type, i for int, p for pointer.
   This is to keep oversigth over the chaos we have joggeling absolute
   addresses (pointers) and offsets into the segment which is int.
   remember all members of the struct chunkhead and chunkfoot are in int.
*/

/*
  fastpath flag. This spesify weither programs are passed a copy
  of the data passed thru shared memoery or a pointer to shared
  memory directly.
  (Does it need to be global??)
*/
static int _mw_fastpath = 0;

int _mw_fastpath_enabled(void) 
{
  return _mw_fastpath;
};

struct segmenthdr * _mwHeapInfo = NULL;

/* this operate on absolute adresses */
chunkfoot * _mwfooter(chunkhead * head)
{
  void * fadr;

  fadr = (void *)head + sizeof(chunkhead) + 
    head->size * _mwHeapInfo->basechunksize;

  DEBUG3("footer for %p is at %p + %u + %lld * %d = %p",
	head, head, sizeof(chunkhead),  head->size,  _mwHeapInfo->basechunksize,
	fadr) ;
  DEBUG3("head adr %p foot adr %p", head,  fadr);
  return (chunkfoot *) fadr;
};


/************************************************************************/


static seginfo_t * segments = NULL;
static int nsegments = 0;

static seginfo_t *  addsegment(int id, int fd, void * start, void * end)
{
   int idx;

   idx = nsegments++;

   DEBUG1("adding %d on fd %d start=%p end=%p", id, fd, start, end);
   segments = realloc(segments, sizeof( struct shm_segment)* nsegments);
   segments[idx].segmentid = id;
   segments[idx].fd = fd;
   segments[idx].start = start;
   segments[idx].end = end;
   return & segments[idx];
};

// see also detach_mmap()
static int delsegment(int id, int fd)
{
   int idx;   

   if (fd > -1) {
      for (idx = 0; idx < nsegments; idx++) {
	 if (segments[idx].fd == fd) {
	    goto found;
	 }
      }
   } else {
      for (idx = 0; idx < nsegments; idx++) {
	 if (segments[idx].segmentid == id) {
	    goto found;
	 }
      }
	    
   };
   return 0;
   
 found:
   DEBUG1("deleting %d on fd %d start=%p end=%p", 
	  segments[idx].segmentid, segments[idx].fd, 
	  segments[idx].start, segments[idx].end);
   nsegments--;   
   segments[idx] = segments[nsegments];
   return 1;
};


static seginfo_t * findsegment_byaddr(void * ptr)
{
   int idx;
   
   for (idx = 0; idx < nsegments; idx++) {
      if ( (segments[idx].start <= ptr) && (segments[idx].end >= ptr) ) {
	 return & segments[idx];
      }
   }
   return NULL;
};

static seginfo_t * findsegment_byid(int id)
{
   int idx;
   
   for (idx = 0; idx < nsegments; idx++) {
      if (segments[idx].segmentid == id) 
	 return & segments[idx];
   }
   return NULL;
};

#if 0
static int check_buffer_bounds(void * adr)
{
   seginfo_t * seginfo;

   seginfo = findsegment_byadr(adr);
   
   if (seginfo == NULL) return -1;
   return 0;
};
#endif 
static seginfo_t * attach_mmap(int id) 
{
   char path[256];
   ipcmaininfo * ipcmain; 
   int fd, rc;
   struct stat statdat;
   ipcmain = _mw_ipcmaininfo();
   void * start, * end;
   seginfo_t * si ;

   if (id <= LOW_LARGE_BUFFER_NUMBER) return NULL;


   snprintf(path, 256, "%s/%d", ipcmain->mw_bufferdir, id);

   fd = open (path, O_RDWR);
   DEBUG1("open of %s => %d", path, fd);

   if (fd <= 0) {
      Error("failed to open mmap'ed buffer %d", errno);
      return NULL;
   };

   rc = fstat(fd, &statdat);
   DEBUG1("stat of %s size=%#lx", path, statdat.st_size);

   start = mmap(NULL, statdat.st_size, PROT_WRITE|PROT_READ, MAP_SHARED, fd, 0);
   DEBUG1("buffer mapped at %p", start);
   if (start == MAP_FAILED) {
      return NULL;
   };
   end = start + statdat.st_size;
   
   si = addsegment(id, fd, start, end);
   DEBUG1("addsegement returned %p", si);
   return si;
};

int _mw_detach_mmap(seginfo_t * si)
{
   Assert(si != NULL);
   DEBUG1("deleting %d on fd %d start=%p end=%p", 
	  si->segmentid, si->fd, 
	  si->start, si->end);
   
   munmap(si->start, si->end - si->start);
   delsegment(si->segmentid, si->fd);
   return 0;
}

seginfo_t *  _mw_addsegment(int id, int fd, void * start, void * end)
{
   return  addsegment( id, fd, start, end);
};

seginfo_t * _mw_getsegment_byid(int segid)
{
   seginfo_t * si;

   si = findsegment_byid(segid);
   if (si == NULL) {
      si = attach_mmap(segid);
   };

   DEBUG1("lookup of segment id %d => %p", segid, si); 
   return si;
};

seginfo_t * _mw_getsegment_byaddr(void * addr)
{
   seginfo_t * si;

   si = findsegment_byaddr(addr);

   DEBUG1("lookup of segment on addr %p => %p", addr, si); 
   return si;
};

/************************************************************************/


/* conversion between offset and adresses */
long _mwadr2offset(void * adr, seginfo_t * si)
{
   if (si == NULL) si = findsegment_byaddr(adr);

   if (si == NULL) return -1;   
   return (long) adr - (long) si->start;;
};

void * _mwoffset2adr(long offset, seginfo_t * si)
{
   if (si == NULL) return NULL;   
   if (offset < 0) return NULL; 

   return (void *) si->start + offset;
};

/* a check for corruption
   return -1 if adr is not in the shm buffer segment.
   return 0 of chunk is OK
   return chunksize if corrupt (in basechunksizes)
*/


int _mw_getbuffer_from_call (mwsvcinfo * svcreqinfo, Call * callmesg)
{
  char * ptr;
  size_t size;
  seginfo_t * si;

  if ( (svcreqinfo == NULL) || (callmesg == NULL) ) {
    errno = EINVAL;
    return -1;
  };

  /* is there no data (datalen == 0), shourtcut */
  svcreqinfo->datalen = callmesg->datalen;    
  if (callmesg->datalen == 0) {
    svcreqinfo->data = NULL;
    return 0;
  };


  si = _mw_getsegment_byid(callmesg->datasegmentid);
  if (si == NULL) {
     errno = EBADR;
     Error("failed to open the associated mmap'ed buffer to call");
     return -1;
  }; 

  /* check that the ptr is in the heap and is of sane length (eg larger) */
  ptr = _mwoffset2adr(callmesg->data, si);
  size = _mwshmgetsizeofchunk(ptr);
  if ( (size <= 0) || (size <=  callmesg->datalen)) {
     Error("got a call with illegal data pointer buffer size = %ld datalen = %lld", 
	   (long) size, callmesg->datalen);
    errno = -EBADMSG;
    return -1;
  };
		    
  /* transfer of the data buffer, unless in fastpath where we recalc the pointer. */
  if (_mw_fastpath_enabled()) {
    svcreqinfo->data = ptr;
  } else {
    svcreqinfo->data = malloc(callmesg->datalen+1);
    memcpy(svcreqinfo->data, ptr, callmesg->datalen);
    /* we adda trainling NUL just to be safe */
    svcreqinfo->data[callmesg->datalen] = '\0';
    _mwfree(ptr);
  };    
  return 0;
};

int _mw_putbuffer_to_call (Call * callmesg, char * data, size_t len)
{
   long dataoffset;
  void * dbuf;

  /* First we handle return buffer. If data is not NULL, and len is 0,
     datat is NULL terminated. if buffer is not a shared memory
     buffer, get one and copy over. */
  
  // we must check for a special case: we could still have a "" as
  // data => len == 0, which we treat as NULL
  if (data != NULL) {
     if (len == 0) len = strlen(data);
     if (len == 0) data = NULL;
  };

  if (data != NULL) {
    dataoffset = _mwshmcheck(data);
    if (dataoffset == -1) {
       dbuf = _mwalloc(len);
       if (dbuf == NULL) {
	  Error("mwalloc(%d) failed reason %d", len, (int) errno);
	  return -errno;
       };
       memcpy(dbuf, data, len);
       dataoffset = _mwshmcheck(dbuf);
    }

    callmesg->data = dataoffset;
    callmesg->datalen = len;
  } else {
    callmesg->data = 0;
    callmesg->datalen = 0;
  }
  return 0;
};

  
/* return in bytes */

static size_t getchunksizebyadr(void * adr, seginfo_t * si)
{
   chunkhead * pCHead;
   pCHead = (chunkhead *) (adr - sizeof(chunkhead));
   if ( (!adr) || (!si)) {
      return -1;
   };
   DEBUG3("si->segmentid = %d size = %lld", si->segmentid, pCHead->size);
   if (si->segmentid == 0) {
      return  pCHead->size * _mwHeapInfo->basechunksize;
   };
   return  pCHead->size;
};

#if 0 // this only works for shmheap, and we mey revisit when we got
      // more multi chunk segments with different base sizes.
static int getchunksizebyadr(chunkhead * pCHead)
{
  int  bin, i;
  chunkfoot *pCFoot;
  chunkhead *pCHabove;
  int iCHead;
  double d;
  
  /* if order to do this fast when OK, we want to know if
     the above pointer in the footer points to the head.
     If so we're OK.*/

  /* needed for core dump (segfault) prevention */
  
  if (check_heap_bounds(pCHead) != 0) {     
     return -1;
  };

  d = log(pCHead->size) / log(2);
  bin = d;
  DEBUG3("size %d Bin = %d %f", pCHead->size, bin, d);
  /* if corrupt pCHead->size */ 
  if ( (bin < 0) || (bin >= _mwHeapInfo->numbins) ) {
    pCHead->size = 0;
    iCHead = _mwadr2offset(pCHead);
    bin = 0;
    for (i = 0; i < _mwHeapInfo->numbins-1; i++) {
      if ( (iCHead > _mwHeapInfo->chunk[i]) && 
	   (iCHead < _mwHeapInfo->chunk[i+1]) ) {
	bin = i;
	pCHead->size = 1<<i;
      };
    };
    if (iCHead > _mwHeapInfo->chunk[_mwHeapInfo->numbins]) {
      bin = _mwHeapInfo->numbins;
      pCHead->size = 1 << _mwHeapInfo->numbins;
    }
  };
  
  /* check for footer corruption and correct if neccessary */
  pCFoot = _mwfooter(pCHead);
  pCHabove = _mwoffset2adr(pCFoot->above);
  if (pCHabove != pCHead) {
    pCFoot->above = _mwadr2offset(pCHead);
  };
  return  pCHead->size;
};
#endif

/* Used in mw(a)call() and mwreply() to check if an address in a shmbuffer.
   Remember that in contrast to tuxedo our data areas must not have bo be alloced 
   by mwalloc() before mwacall and mwreply. if static or alloacted by malloc() 
   mwacall and mwreply will copy the data into a shmbuffer. 
   this function is used to find out if that is needed.
   see mwacall(3C), mwreply(3C), mwalloc(3C)
   This return the offset of the buffer.
*/
int _mwshmcheck(void * adr)
{
  size_t size;
  int offset; 
  seginfo_t * si;

  si = findsegment_byaddr(adr);
  if (si == NULL) return -1;

  size = getchunksizebyadr(adr, si);
  if (size < 0)   return -1; // can't happen

  DEBUG1("buffer at %p has size %d", adr, size);

  offset = _mwadr2offset(adr, si);
  return offset;
}

size_t _mwshmgetsizeofchunk(void * adr)
{
  seginfo_t * si;

  si = findsegment_byaddr(adr);
  if (si == NULL) return -1;

  return  getchunksizebyadr(adr, si);
}

/************************************************************************/


/* Shm buffer are stored in looped double linked lists, rings
   actually. I here make us two operators that push and pop chunks on
   these rings. Thier arguments are the root pointers to the ring.  

   When a chunk is inuse it is not a member of a ring.

   These functions requier that the neccessary sem locks have been
   obtained.*/
static chunkhead * popchunk(int *iRoot, int * freecount, seginfo_t * si)
{ 
  chunkhead * pCHead, *pPrev, *pNext;
  chunkfoot * pCFoot, * ptmpfoot;;

  if ( (iRoot == NULL) || (freecount == NULL) ) 
    return NULL;
 
  if (*freecount == 0) return NULL;
  if (*iRoot == 0) return NULL;

  DEBUG3("root offset = %d", *iRoot);

  pCHead = _mwoffset2adr(*iRoot, si);
  pCFoot = _mwfooter(pCHead); 

  DEBUG3("head at %p footer at %p", pCHead, pCFoot);
  DEBUG3("footer: above = %lld prev %lld next %lld", pCFoot->above, pCFoot->next, pCFoot->prev);

  if (*freecount == 1)  { /*last free*/
    pCFoot->next = 0;
    pCFoot->prev = 0;
    *iRoot = 0;
    (*freecount) --;
    return pCHead;
  };

  /* close the remaing ring. */
  pPrev = _mwoffset2adr(pCFoot->prev, si);
  pNext = _mwoffset2adr(pCFoot->next, si);
  ptmpfoot = _mwfooter(pPrev);
  ptmpfoot->next = _mwadr2offset(pNext, si);
  ptmpfoot = _mwfooter(pNext);
  ptmpfoot->prev = _mwadr2offset(pPrev, si);

  *iRoot = pCFoot->next;
  (*freecount) --;
  DEBUG3("root offset now = %d", *iRoot);

  /* disconnect the chunk */
  pCFoot->next = 0;
  pCFoot->prev = 0;
  DEBUG3("returning head at %p ", pCHead);
  return pCHead;
};

static int pushchunk(chunkhead * pInsert, int * iRoot, int * freecount, seginfo_t * si)
 {
   chunkhead *pPrev, *pNext;
   chunkfoot * pCFoot;
   int iInsert, iNext, iPrev;

   if ( (pInsert == NULL) || (iRoot == NULL) || (freecount == NULL) ) 
     return -EINVAL;

   iInsert = _mwadr2offset(pInsert, si);

   DEBUG3("inserting buffer at offset %d", iInsert);

   pCFoot = _mwfooter(pInsert);

   if (_mwoffset2adr(pCFoot->above, si) != pInsert) {
     Warning("possible shm buffer corruption, error in chunk at %p", pInsert);
     pCFoot->above = iInsert;
   };

   /* if ring is empty */
   if (*iRoot == 0) {
      DEBUG3("Empty buffer ring");
      *iRoot = iInsert;
      /* a ring with one element is still a ring */
      pCFoot->next = iInsert;
      pCFoot->prev = iInsert;
      (*freecount)++;
      return 0;
   };

   /* We're getting the element above and below the new element */
   iNext = _mwfooter(_mwoffset2adr(*iRoot, si))->next;
   iPrev = *iRoot;

   DEBUG3("prev buffer head at offset %d next at offset %d", iPrev, iNext);

   pPrev = _mwoffset2adr(iPrev, si);
   pNext = _mwoffset2adr(iNext, si);

   pCFoot->next = iNext;
   pCFoot->prev = iPrev;
   _mwfooter(pNext)->prev = iInsert;
   _mwfooter(pPrev)->next = iInsert;
   (*freecount)++;
   return 0;
 };


 /* lock and unlock of heap info, and chunk rings, 
    arg 0 means the common data, including inuse ring.
    arg 1-n mean lock of the free chunk lists.
 */

 static int lock(int id)
 {
   struct sembuf sops[2];

   if (_mwHeapInfo == NULL) return -EILSEQ;
   sops[0].sem_num = id; /* lock sem for this chunk */
   sops[0].sem_op = -1;
   sops[0].sem_flg = 0;
   return semop(_mwHeapInfo->semid, sops, 1);
 };
 static int unlock(int id)
 {
   struct sembuf sops[2];

   if (_mwHeapInfo == NULL) return -EILSEQ;
   sops[0].sem_num = id; /* lock sem for this chunk */
   sops[0].sem_op = 1;
   sops[0].sem_flg = 0;
   return semop(_mwHeapInfo->semid, sops, 1);
 };

 static inline int attachheap(void)
 {
   struct ipcmaininfo * ipcmain;

   if (_mwHeapInfo != NULL ) return 0;

   ipcmain = _mw_ipcmaininfo(); 
   if (ipcmain == NULL) {
     Error("_mwalloc: It seems there are no mwd running, no main shm info attached.");
     return -ENOENT;
   };
   _mwHeapInfo = shmat (ipcmain->heap_ipcid, NULL, 0);
   /* the manual says that shmat return -1 on failure, but
      that seem not natural to me since it return a pointer. */
   if ((_mwHeapInfo == (void *) -1) || (_mwHeapInfo == NULL)) {
     Error("_mwalloc: failed to attach the shm heap, erno = %d.", errno);
     return -errno;
   };
   /* the magic numer is "MW" thus 0x4D57 */
   if (_mwHeapInfo->magic != 0x4D57) {
     Error("_mwalloc: The shm heap seem not to be formated, wrong magic header");
     shmdt(_mwHeapInfo);
     return -EUCLEAN;;
   };

   addsegment(0, -1, _mwHeapInfo, _mwHeapInfo + _mwHeapInfo->segmentsize);
   return 0;
 };


 int _mwshmgetowner(void * adr, MWID * id)
 {
    chunkhead * pCHead;

    pCHead = adr - sizeof(chunkhead);

    *id = pCHead->ownerid;
    return 0;
 };


 int _mwshmsetowner(void * adr, MWID id)
 {
    chunkhead * pCHead;

    pCHead = adr - sizeof(chunkhead);

    pCHead->ownerid = id;

    return 0;
 };


static int find_bin(size_t size, seginfo_t * si)
{
   int bin;
   float f;
   int chksize;
   if (si->segmentid > LOW_LARGE_BUFFER_NUMBER) {
      return 0;
   };
   
   f = size;
   f /= _mwHeapInfo->basechunksize;
   chksize = ceilf(f);
   
   f = log(chksize) / log(2);
   bin = ceilf(f);
   
   DEBUG1("alloc size %d, size in chunks = %d, bin = %d", size, chksize, bin);
   return bin;
};

void * _mwalloc(size_t size)
{
   chunkhead *pCHead;
   int bin, i, rc;
   seginfo_t * si;
   
   if (size <= 0) {
      errno = ENOMEM;
      return NULL;
   };


  /*
    now we need to know how many base chunks we need.
    Remember that I have for each group of chunks doubled 
    the chunk size. thats: 1*basesize 2*basesize, 4*basesize, etc.
    Then the references to these chunk groups are in an array.
    We then find the inde for the arrays by doing ln2(n) where 
    n is the number of basesizes needed to hold the requested size.
 
  */
  si = findsegment_byid(0); // 0 is the ipcshm heap. 
  bin = find_bin(size, si);
  for (i = bin; i < _mwHeapInfo->numbins; i++) {

    /* there are no free chunk at the right size, we try the larges ones. */
     if (_mwHeapInfo->freecount[i] > 0) {
 
      rc = lock(i);
      if (rc < 0) {
	Error("_mwalloc: lock of bin %d failed erro=%d", i, errno);
	return NULL;
      }

      /* We now have a lock for the chunks of 1 * basesize */
      DEBUG3("first chunk %d freecount %d", 
	     _mwHeapInfo->chunk[i], _mwHeapInfo->freecount[i]);
      pCHead = popchunk(&_mwHeapInfo->chunk[i], 
			&_mwHeapInfo->freecount[i], si);
      DEBUG3("first chunk %d freecount %d", 
	     _mwHeapInfo->chunk[i], _mwHeapInfo->freecount[i]);

      rc = unlock(i);
      
      if (pCHead == NULL) continue;
      
      DEBUG3("chunk has size in chunks %lld ownerid = %x", pCHead->size, pCHead->ownerid);
      
      _mwHeapInfo->inusecount ++;
      if (_mwHeapInfo->inusecount > _mwHeapInfo->inusehighwater) 
	_mwHeapInfo->inusehighwater = _mwHeapInfo->inusecount;
      
      /* really should be CLIENTID or SERVERID or GATEWAYID ... */
      pCHead->ownerid = _mw_get_my_mwid(); 
      if (pCHead->size != 1<<i) {
	Error("mwalloc: retrived chunk is of size %lld*%d != %d*%d chunk at offset %ld",
	      pCHead->size, 
	      _mwHeapInfo->basechunksize , 
	      1<<i, 
	      _mwHeapInfo->basechunksize, 
	      _mwadr2offset(pCHead, si));
	Error ("get cgetchunksizebyadr = %d", getchunksizebyadr(pCHead, si));
      };

      DEBUG1("_mwalloc(%ld) return a chunk with size %lld at %p ", 
	     (long) size, pCHead->size*_mwHeapInfo->basechunksize, (void*) pCHead + sizeof(chunkhead));

      /* NB: wr return the addresss to the data area NOT to the head of chunk.
       */
      return (void*) pCHead + sizeof(chunkhead);
    };
  };
  
  // request large buffer from mwd
  {
     Alloc allocmesg;
     int len;
     void * ptr;
     seginfo_t * si;
     allocmesg.mtype = ALLOCREQ;
     allocmesg.mwid = _mw_get_my_mwid();
     allocmesg.size = size;
     allocmesg.bufferid = UNASSIGNED;

     rc = _mw_ipc_putmessage(0, (void *) &allocmesg, sizeof(Alloc), 0);
     DEBUG1("sendt alloc request to mwd rc=%d", rc);
     Assert(rc == 0);

     len = sizeof(Alloc);
     rc = _mw_ipc_getmessage((void*) &allocmesg, &len, ALLOCRPL, 0);
     DEBUG1("Got alloc reply, rc=%d", rc);
     Assert(rc == 0);
     Assert (len  == sizeof(Alloc));
     DEBUG("size=%lld pages=%lld id=%d", allocmesg.size, allocmesg.pages, allocmesg.bufferid);

     if (allocmesg.bufferid < LOW_LARGE_BUFFER_NUMBER) {
	Error("Out of memory (from mwd)");
	errno = -ENOMEM;
	return NULL;
     } else {
	si = attach_mmap(allocmesg.bufferid);
	if (si == NULL) {
	   Error("failed to allocate a large shared buffer, reason %s", strerror(errno));
	   return NULL;
	};
	ptr = si->start + sizeof(chunkhead);
	return ptr;
     };
  };
  Error("out of memory");
  errno = -ENOMEM;
  return NULL;
};

void * _mwrealloc(void * adr, size_t newsize) 
{
  size_t size, copylength;
  void * newbuffer;

  if (adr == NULL ) return _mwalloc(newsize); 

  size =  _mwshmgetsizeofchunk(adr);
  if (size > 0) {
    DEBUG1("newsize = %d oldsize = %d", newsize, size);

    /* check to see if newsize need the same size of buffer. */
    if (newsize < size) 
       return adr;
    
    newbuffer = _mwalloc(newsize);
    if (newbuffer == NULL) return NULL;
    
    if (size < newsize) copylength = size;
    else copylength = newsize;
    memcpy(newbuffer, adr, copylength);
    _mwfree(adr);
    return newbuffer;
  }
  
  if (adr) _mwfree(adr);
  return NULL;
};

static int _mwfree0(seginfo_t * si, void * adr)
{
   long offset, off, l, s, n, t;
   chunkhead *pCHead;
   int bin, rc;
   double d;

   // this is a protection of the segment header
   DEBUG1("checking to see if the address is in the chuck area of the heap");
   offset = (unsigned long) adr - (unsigned long) _mwHeapInfo;
   DEBUG1("top = %lld < adr = %ld < %lld, bottom", _mwHeapInfo->top, offset, _mwHeapInfo->bottom);
   if ( (offset < _mwHeapInfo->top) || (offset > _mwHeapInfo->bottom)) {
      DEBUG1("EINVAL");
      return -EINVAL;
   };
   
   /* adr point to the data area of a chunk, which lies between the
      chunkhead and chunkfoot*/
   pCHead = adr - sizeof(chunkhead);;
   offset -= sizeof(chunkhead); //offset now it at the beginning of a chunk;
   rc = getchunksizebyadr(adr, si);
   if (rc < 0) {
      DEBUG1("ENOENT");
      return -ENOENT;
   };

   // blacvk out buffer
   s = _mwHeapInfo->basechunksize * pCHead->size;
   DEBUG1("Clearinbg buffer %ld bytes", s);
   memset(adr, 0, s);
   
   d = log(pCHead->size) / log(2);
   bin = d;
   DEBUG1("d = %f, bin = %d", d, bin);

   /* check that the pointer is at a start of a chunk
      first get the byte offset into the chunk area, then we do two checks:
      1. the mod of the offset into the chunk area and size_of_chunk + CHUNKOVERHEAD must be 0
      2. the offset into chunk area / by size_of_chunk + CHUNKOVERHEAD must be within chunks per size
   */
   t = _mw_gettopofbin(_mwHeapInfo, bin);
   off = offset - t;
   s =  (_mwHeapInfo->basechunksize *pCHead->size)+ CHUNKOVERHEAD;

   DEBUG1("bin %d start %ld, %ld bytes per chunk, %ld bytes offset into chunk area", 
	  bin, t, s, off);
   l = off / s;
   n =  bin  ? _mwHeapInfo->chunkspersize :  _mwHeapInfo->chunkspersize *2;
   DEBUG1("bin %d chunk index %ld of %ld", bin, l, n);
   if ( (l < 0) || (l > n)) {
      Error ("Possible shm buffer corruption 0 < %ld < %ld fails for adr %p offset %ld", l,  n, adr, off);
      errno = EINVAL;
      return -1;
   };
   
   l = off % s;
   DEBUG1 ("%ld %% %ld = %ld shall be 0", off, s, l);
   if (l != 0) {
      Error("free on an address not at beginning of a buffer %p", adr);
      errno = EINVAL;
      return -1;
   };


   DEBUG1("size %lld ownerid=%s Bin = %d %d %f", pCHead->size, _mwid2str(pCHead->ownerid, NULL), rc, bin, d);
   pCHead->ownerid = UNASSIGNED;
   
   rc = lock(bin); /* lock sem for bin */
   rc = pushchunk(pCHead, &_mwHeapInfo->chunk[bin], 
		  &_mwHeapInfo->freecount[bin], si);
   if (rc != 0) {
      Error("mwfree: shm chunk of size %lld lost, reason %d", 
	    pCHead->size, rc);
   };
   _mwHeapInfo->inusecount --;
   unlock(bin);
   
   return 0;
};

int _mwfree_mmap(seginfo_t * si, void * adr)
{
   Alloc allocmesg;
   int rc;

   allocmesg.mtype = FREEREQ;
   allocmesg.mwid = _mw_get_my_mwid();
   allocmesg.size = -1;
   allocmesg.bufferid = si->segmentid;
   allocmesg.pages = -1;
   
   _mw_detach_mmap(si);
   
   rc = _mw_ipc_putmessage(0, (void *) &allocmesg, sizeof(Alloc), 0);
   DEBUG1("sendt free request to mwd rc=%d", rc);
   Assert(rc == 0);
   
   return 0;
};
   
int _mwfree(void * adr)
{
   seginfo_t * si;
   
   si = findsegment_byaddr(adr);
   
   if (si == NULL) return -ENOENT;
   
   if (si->segmentid == 0) {
      DEBUG3("shm buffer");
      return _mwfree0(si, adr);
   } else {
      DEBUG3("mmap'ed buffer");
      return _mwfree_mmap(si, adr);
   };
};

#ifdef SHMDEBUG

int __mw_verify_segmenthdr(struct segmenthdr * seghdr)
{
   struct shmid_ds stat;

   if (seghdr->magic != MWSEGMAGIC) {
      Error("segment header %hx != %hx", seghdr->magic != MWSEGMAGIC);
   };
   
   
};
#endif

int _mw_gettopofbin(struct segmenthdr * seghdr, int bin)
{
   int offset, l;
   int b, s;

   offset = sizeof(struct segmenthdr);

   for (b = 0; b < bin; b++) {
      s = (1 << b) *  seghdr->basechunksize;
      l = seghdr->chunkspersize * (s + CHUNKOVERHEAD);
      if ( b == 0) l *= 2;
      DEBUG1("chunks in bin %d is %d bytes total size %d bytes ", b, s, l);
      offset += l;
   }; 
   DEBUG1("top of bin %d is %d ", bin, offset);
   return offset;
};

/* Emacs C indention
Local variables:
c-basic-offset: 3
End:
*/

   

