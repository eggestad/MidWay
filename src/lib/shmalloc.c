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


static char * RCSId = "$Id$";
static char * RCSName = "$Name$"; /* CVS TAG */

#include <sys/sem.h>
#include <sys/shm.h>
#include <errno.h>
#include <stdio.h> 
#include <math.h>
#include <stdlib.h>

#include <MidWay.h>
#include <shmalloc.h>
#include <ipctables.h>

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

struct segmenthdr * _mwHeapInfo = NULL;

/* this operate on absolute adresses */
chunkfoot * _mwfooter(chunkhead * head)
{
  int fadr;

  fadr = (int)head + sizeof(chunkhead) + 
    head->size * _mwHeapInfo->basechunksize;
  mwlog(MWLOG_DEBUG3, "footer for %#x is at %#x + %#x + %#x * %#x = %#x",
	head, head, sizeof(chunkhead),  head->size,  _mwHeapInfo->basechunksize,
	fadr) ;
  return (chunkfoot *) fadr;
};

/* conversion between offset and adresses */
int _mwadr2offset(void * adr)
{
  if (_mwHeapInfo == NULL) return -1;
  return (int) adr - (int) _mwHeapInfo;
};

void * _mwoffset2adr(int offset)
{
  if (_mwHeapInfo == NULL) return NULL;
  return (void *)_mwHeapInfo + offset;
};
/* a check for corruption
   return -1 if adr is not in the shm buffer segment.
   return 0 of chunk is OK
   return chunksize if corrupt (in basechunksizes)
*/
static int getchunksizebyadr(chunkhead * pCHead)
{
  int chksize, chkindex, i, rc = 0;
  chunkfoot *pCFoot;
  chunkhead *pCHabove;
  int iCHead;

  /* if order to do this fast when OK, we want to know if
     the above pointer in the footer points to the head.
     If so we're OK.*/

  /* needed for core dump (segfault) prevention */
  if ( ((void*)pCHead < (void*)_mwHeapInfo) || 
       ((void*)pCHead > (void*)(_mwHeapInfo + _mwHeapInfo->segmentsize)) )
    return -1;
  
  chkindex = log(pCHead->size) / log(2);
  
  /* if corrupt pCHead->size */ 
  if ( (chkindex < 0) || (chkindex >= _mwHeapInfo->numbins) ) {
    pCHead->size = 0;
    iCHead = _mwadr2offset(pCHead);
    chkindex = 0;
    for (i = 0; i < _mwHeapInfo->numbins-1; i++) {
      if ( (iCHead > _mwHeapInfo->chunk[i]) && 
	   (iCHead < _mwHeapInfo->chunk[i+1]) ) {
	chkindex = i;
	pCHead->size = 1<<i;
	rc = pCHead->size;
      };
    };
    if (iCHead > _mwHeapInfo->chunk[_mwHeapInfo->numbins]) {
      chkindex = _mwHeapInfo->numbins;
      pCHead->size = 1 << _mwHeapInfo->numbins;
      rc = pCHead->size;
    }
  };
  
  /* check for footer corruption and correct if neccessary */
  pCFoot = _mwfooter(pCHead);
  pCHabove = _mwoffset2adr(pCFoot->above);
  if (pCHabove != pCHead) {
    pCFoot->above = _mwadr2offset(pCHead);
    rc = pCHead->size;
  };
  return rc;
};

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
  int offset;
  int size;

  /* if SRBP then we have no heap... */
  if (_mwHeapInfo == NULL) return -1;

  /* first we make sure that adr is within the heap*/
  if (adr < (void *) ((int)_mwHeapInfo + sizeof(struct segmenthdr))) return -1;
  if (adr > (void *) ((int)_mwHeapInfo + _mwHeapInfo->segmentsize)) return -1;
      
  /* now we do sanity check on the chunk */
  size = getchunksizebyadr(adr - sizeof(chunkhead));

  if (size < 0)   return -1;
  if (size >= _mwHeapInfo->numbins) return -1;
  return _mwadr2offset(adr);
}


/* Shm buffer are stored in looped double linked lists, rings
   actually. I here make us two operators that push and pop chunks on
   these rings. Thier arguments are the root pointers to the ring.  

   When a chunk is inuse it is not a member of a ring.

   These functions requier that the neccessary sem locks have been
   obtained.*/
static chunkhead * popchunk(int *iRoot, int * freecount)
{ 
  chunkhead * pCHead, *pPrev, *pNext, *pEnd;
  chunkfoot * pCFoot;

  if ( (iRoot == NULL) || (freecount == NULL) ) 
    return NULL;
 
  if (*freecount == 0) return NULL;
  if (*iRoot == 0) return NULL;

  pCHead = _mwoffset2adr(*iRoot);
  pCFoot = _mwfooter(pCHead); 
  
  if (*freecount == 1)  { /*last free*/
    pCFoot->next = 0;
    pCFoot->prev = 0;
    *iRoot = 0;
    (*freecount) --;
    return pCHead;
  };

  /* close the remaing ring. */
  pPrev = _mwoffset2adr(pCFoot->prev);
  pNext = _mwoffset2adr(pCFoot->next);
  _mwfooter(pPrev)->next = _mwadr2offset(pNext);
  _mwfooter(pNext)->prev = _mwadr2offset(pPrev);

  *iRoot = pCFoot->next;
  (*freecount) --;

  /* disconnect the chunk */
  pCFoot->next == 0;
  pCFoot->prev == 0;
  return pCHead;
};

static int pushchunk(chunkhead * pInsert, int * iRoot, int * freecount)
{
  chunkhead * pCHead, *pPrev, *pNext;
  chunkfoot * pCFoot;

  if ( (pInsert == NULL) || (iRoot == NULL) || (freecount == NULL) ) 
    return -EINVAL;

  pCFoot = _mwfooter(pInsert);

  if (_mwoffset2adr(pCFoot->above) != pInsert) {
    mwlog(MWLOG_WARNING, 
	  "possible shm buffer corruption, error in chunk at %#x", pInsert);
    pCFoot->above = _mwadr2offset(pInsert);
  };

  /* if ring is empty */
  if (*iRoot == 0) {
    *iRoot = _mwadr2offset(pInsert);
    /* a ring with one element is still a ring */
    pCFoot->next = _mwadr2offset(pInsert);
    pCFoot->prev = _mwadr2offset(pInsert);
    (*freecount)++;
    return 0;
  };
  
  /* We're getting the element above and below the new element */
  pNext = _mwoffset2adr(_mwfooter(_mwoffset2adr(*iRoot))->next);
  pPrev = _mwoffset2adr(*iRoot);
  _mwfooter(pInsert)->next = _mwadr2offset(pNext);
  _mwfooter(pInsert)->prev = _mwadr2offset(pPrev);
  _mwfooter(pNext)->prev = _mwadr2offset(pInsert);
  _mwfooter(pPrev)->next = _mwadr2offset(pInsert);
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

void * _mwalloc(int size)
{
  struct ipcmaininfo * ipcmain;
  chunkhead *pCHead;
  int chksize, i, rc;

  if (_mwHeapInfo == NULL ) {
    ipcmain = _mw_ipcmaininfo(); 
    if (ipcmain == NULL) {
      mwlog(MWLOG_ERROR, "_mwalloc: It seems there are no mwd running, no main shm info attached.");
      return NULL;
    };
    _mwHeapInfo = shmat (ipcmain->heap_ipcid, NULL, 0);
    /* the manual says that shmat return -1 on failure, but
       that seem not natural to me since it return a pointer. */
    if ((_mwHeapInfo == (void *) -1) || (_mwHeapInfo == NULL)) {
      mwlog(MWLOG_ERROR, "_mwalloc: failed to attach the shm heap, erno = %d.", errno);
      return NULL;
    };
    /* the magic numer is "MW" thus 0x4D57 */
    if (_mwHeapInfo->magic != 0x4D57) {
      mwlog(MWLOG_ERROR, "_mwalloc: The shm heap seem not to be formated, wrong magic header");
      shmdt(_mwHeapInfo);
      return NULL;
    };
  }
  
  /*
    now we need to know how many base chunks we need.
    Remember that I have for each group of chunks doubled 
    the chunk size. thats: 1*basesize 2*basesize, 4*basesize, etc.
    Then the references to these chunk groups are in an array.
    We then find the inde for the arrays by doing ln2(n) where 
    n is the number of basesizes needed to hold the requested size.
 
  */
  chksize =  (size / _mwHeapInfo->basechunksize );
  
  for (i = chksize; i < _mwHeapInfo->numbins; i++) {
 
    /* there are no free chunk at the right size, we try the larges ones. */
    if (_mwHeapInfo->freecount[i] > 0) {
 
      rc = lock(i);
      if (rc < 0) {
	mwlog(MWLOG_ERROR,"_mwalloc: lock of bin %d failed erro=%d", i, errno);
	return NULL;
      }

      /* We now have a lock for the chunks of 1 * basesize */
      pCHead = popchunk(&_mwHeapInfo->chunk[i], 
			&_mwHeapInfo->freecount[i]);
      rc = unlock(i);
      
      if (pCHead == NULL) continue;

      
      _mwHeapInfo->inusecount ++;
      if (_mwHeapInfo->inusecount > _mwHeapInfo->inusehighwater) 
	_mwHeapInfo->inusehighwater = _mwHeapInfo->inusecount;
      
      /* really should be CLIENTID or SERVERID or GATEWAYID ... */
      pCHead->ownerid = getpid(); 
      if (pCHead->size != 1<<i) {
	mwlog(MWLOG_ERROR, "mwalloc: retrived chunk is of size %d*%d != %d*%d",
	      pCHead->size, _mwHeapInfo->basechunksize , 
	      1<<i, _mwHeapInfo->basechunksize);       
      };
      /* NB: wr return the addresss to the data area NOT to the head of chunk.
       */
      return (void*) pCHead + sizeof(chunkhead);
    };
  };
  mwlog(MWLOG_ERROR, "_mwalloc: out of memory");
  errno = -ENOMEM;
  return NULL;
};

void * _mwrealloc(void * adr, int newsize) 
{
  int size, copylength;
  void * newbuffer;
  if (adr == NULL ) return NULL; 

 
  size = getchunksizebyadr(adr - sizeof(chunkhead));
  if (size > 0) {
    /* check to see if newsize nedd teh same size of buffer. */
    if ( (newsize < size) && (newsize > size/2) ) 
      return adr;
    newbuffer = _mwalloc(newsize);
    if (newbuffer == NULL) return NULL;
    
    if (size < newsize) copylength = size;
    else copylength = newsize;
    memcpy(newbuffer, adr, copylength);
    _mwfree(adr);
    return newbuffer;
  }

  return NULL;
};

int _mwfree(void * adr)
{
  struct ipcmaininfo * ipcmain;
  chunkhead *pCHead;

  struct sembuf sops[2];
  int chksize, chkindex, headeroffset, rc;

  if (_mwHeapInfo == NULL ) {
    ipcmain = _mw_ipcmaininfo(); 
    if (ipcmain == NULL) return -1;
    _mwHeapInfo = shmat (ipcmain->heap_ipcid, NULL, 0);
    /* the manual says that shmat return -1 on failure, but
       that seem not natural to me since it return a pointer. */
    if ((_mwHeapInfo == (void *) -1) || (_mwHeapInfo == NULL))
      return -1;
    /* the magic numer is "MW" thus 0x4D57 */
    if (_mwHeapInfo->magic != 0x4D57) {
      shmdt(_mwHeapInfo);
      return -1;
    };
  }
  
  /* adr point to the data area of a chunk, which lies between the
     chunkhead and chunkfoot*/
  pCHead = adr - sizeof(chunkhead);;
  
  rc = getchunksizebyadr(pCHead);
  if (rc < 0) return -ENOENT;
  chkindex = log(pCHead->size) / log(2);
  pCHead->ownerid = UNASSIGNED;

  rc = lock(chkindex); /* lock sem for chunk */
  rc = pushchunk(pCHead, &_mwHeapInfo->chunk[chkindex], 
		 &_mwHeapInfo->freecount[chkindex]);
  if (rc != 0) {
    mwlog(MWLOG_ERROR, "mwfree: shm chunk of size %d lost, reason %d", 
	  pCHead->size, rc);
  };
  _mwHeapInfo->inusecount --;
  unlock(chkindex);
  return 0;
};

  
#ifdef TEST
main()
{
  int use=0, max=0, avg=0, hwt=0, rc=0;
  mwsetloglevel(MWLOG_DEBUG);
  printf ("shmb id %d\n",shmb_format(1024,8,123123));
  rc = shmb_info(0, &use, &avg, &hwt, &max);
  printf ("shmb_info rc=%d gave => use=%d average=%d highwater=%d max=%d\n",
	  rc, use, avg, hwt, max);
};
#endif


