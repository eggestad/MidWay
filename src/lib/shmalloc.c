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
  DEBUG3("footer for %p is at %p + %p + %p * %p = %p",
	head, head, sizeof(chunkhead),  head->size,  _mwHeapInfo->basechunksize,
	fadr) ;
  return (chunkfoot *) fadr;
};

/* conversion between offset and adresses */
int _mwadr2offset(void * adr)
{
  if (_mwHeapInfo == NULL) return -1;
  return (long) adr - (long) _mwHeapInfo;
};

void * _mwoffset2adr(int offset)
{
  if (offset < 0) return NULL; 
  if (_mwHeapInfo == NULL) return NULL;
  return (void *)_mwHeapInfo + offset;
};

/* a check for corruption
   return -1 if adr is not in the shm buffer segment.
   return 0 of chunk is OK
   return chunksize if corrupt (in basechunksizes)
*/


int _mw_getbuffer_from_call (mwsvcinfo * svcreqinfo, Call * callmesg)
{
  char * ptr;
  int size;
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

  /* check that the ptr is in the heap and is of sane length (eg larger) */
  ptr = _mwoffset2adr(callmesg->data);
  size = _mwshmgetsizeofchunk(ptr);
  if ( (size <= 0) || (size <=  callmesg->datalen)) {
    Error("got a call with illegal data pointer buffer size = %d datalen = %d", size, callmesg->datalen);
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

int _mw_putbuffer_to_call (Call * callmesg, char * data, int len)
{
  int dataoffset;
  void * dbuf;

  /* First we handle return buffer. If data is not NULL, and len is 0,
     datat is NULL terminated. if buffer is not a shared memory
     buffer, get one and copy over. */
    
  if (data != NULL) {
    if (len == 0) len = strlen(data);

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
static int getchunksizebyadr(chunkhead * pCHead)
{
  int  bin, i;
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
  
  bin = log(pCHead->size) / log(2);
  
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
  int size, bin;
  
  /* if SRBP then we have no heap... */
  if (_mwHeapInfo == NULL) return -1;

  DEBUG3(" testing to see if byffer is %x < %x < %x", 
	 (void *)_mwHeapInfo + sizeof(struct segmenthdr), 
	 adr, 
	 (void *)_mwHeapInfo + _mwHeapInfo->segmentsize);

  /* first we make sure that adr is within the heap*/
  if (adr < ((void *)_mwHeapInfo + sizeof(struct segmenthdr))) return -1;
  if (adr > ((void *)_mwHeapInfo + _mwHeapInfo->segmentsize)) return -1;
      
  /* now we do sanity check on the chunk */
  size = getchunksizebyadr(adr - sizeof(chunkhead));

  if (size < 0)   return -1;
  bin = log(size)/log(2);
  DEBUG3("buffer at %p has size %d * %d, bin = %d of max %d", 
	 size, _mwHeapInfo->basechunksize, bin, _mwHeapInfo->numbins);
  if (bin >= _mwHeapInfo->numbins) return -1;

  return _mwadr2offset(adr);
}

int _mwshmgetsizeofchunk(void * adr)
{
  int size;

  /* if SRBP then we have no heap... */
  if (_mwHeapInfo == NULL) return -1;

  /* first we make sure that adr is within the heap*/
  if (adr < ((void *)_mwHeapInfo + sizeof(struct segmenthdr))) return -1;
  if (adr > ((void *)_mwHeapInfo + _mwHeapInfo->segmentsize)) return -1;
      
  /* now we do sanity check on the chunk */
  size = getchunksizebyadr(adr - sizeof(chunkhead));

  if (size < 0)   return -1;
  return size * _mwHeapInfo->basechunksize;
}


/* Shm buffer are stored in looped double linked lists, rings
   actually. I here make us two operators that push and pop chunks on
   these rings. Thier arguments are the root pointers to the ring.  

   When a chunk is inuse it is not a member of a ring.

   These functions requier that the neccessary sem locks have been
   obtained.*/
static chunkhead * popchunk(int *iRoot, int * freecount)
{ 
  chunkhead * pCHead, *pPrev, *pNext;
  chunkfoot * pCFoot;

  if ( (iRoot == NULL) || (freecount == NULL) ) 
    return NULL;
 
  if (*freecount == 0) return NULL;
  if (*iRoot == 0) return NULL;

  DEBUG3("root offset = %d", *iRoot);

  pCHead = _mwoffset2adr(*iRoot);
  pCFoot = _mwfooter(pCHead); 

  DEBUG3("head at %p footer at %p", pCHead, pCFoot);
  DEBUG3("footer: above = %d prev %d next %d", pCFoot->above, pCFoot->next, pCFoot->prev);

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
  DEBUG3("root offset now = %d", *iRoot);

  /* disconnect the chunk */
  pCFoot->next = 0;
  pCFoot->prev = 0;
  DEBUG3("returning head at %p ", pCHead);
  return pCHead;
};

static int pushchunk(chunkhead * pInsert, int * iRoot, int * freecount)
{
  chunkhead *pPrev, *pNext;
  chunkfoot * pCFoot;

  if ( (pInsert == NULL) || (iRoot == NULL) || (freecount == NULL) ) 
    return -EINVAL;

  pCFoot = _mwfooter(pInsert);

  if (_mwoffset2adr(pCFoot->above) != pInsert) {
    Warning(	  "possible shm buffer corruption, error in chunk at %p", pInsert);
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

  return 0;
};


int _mwshmgetowner(int offset, MWID * id)
{
   chunkhead * pCHead;
   
   if (id == NULL) return -EINVAL;
   
   if (offset < 0) return -ERANGE;
   if (offset > _mwHeapInfo->segmentsize) return -ERANGE;

   pCHead = _mwoffset2adr(offset - sizeof(chunkhead));
   *id = pCHead->ownerid;
   return 0;
};


int _mwshmsetowner(int offset, MWID id)
{
   chunkhead * pCHead;
   
   if (offset < 0) return -ERANGE;
   if (offset > _mwHeapInfo->segmentsize) return -ERANGE;

   pCHead = _mwoffset2adr(offset - sizeof(chunkhead));
   pCHead->ownerid = id;

   return 0;
};


void * _mwalloc(int size)
{
  chunkhead *pCHead;
  int chksize, bin, i, rc;

  rc = attachheap();
  if (rc != 0) {
    errno = -rc;
    return NULL;
  };

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
  chksize =  (size / _mwHeapInfo->basechunksize)+1;
  bin = log(chksize) / log(2);
  DEBUG1("alloc size %d, size in chunks = %d, bin = %d", size, chksize, bin);
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
			&_mwHeapInfo->freecount[i]);
      DEBUG3("first chunk %d freecount %d", 
	     _mwHeapInfo->chunk[i], _mwHeapInfo->freecount[i]);

      rc = unlock(i);
      
      if (pCHead == NULL) continue;
      
      DEBUG3("chunk has size in chunks %ld ownerid = %lx", pCHead->size, pCHead->ownerid);
      
      _mwHeapInfo->inusecount ++;
      if (_mwHeapInfo->inusecount > _mwHeapInfo->inusehighwater) 
	_mwHeapInfo->inusehighwater = _mwHeapInfo->inusecount;
      
      /* really should be CLIENTID or SERVERID or GATEWAYID ... */
      pCHead->ownerid = getpid(); 
      if (pCHead->size != 1<<i) {
	Error("mwalloc: retrived chunk is of size %ld*%ld != %d*%ld",
	      pCHead->size, _mwHeapInfo->basechunksize , 
	      1<<i, _mwHeapInfo->basechunksize);       
      };

      DEBUG1("_mwalloc(%d) return a chunk with size %d at %p ", 
	     size, pCHead->size*_mwHeapInfo->basechunksize, (void*) pCHead + sizeof(chunkhead));

      /* NB: wr return the addresss to the data area NOT to the head of chunk.
       */
      return (void*) pCHead + sizeof(chunkhead);
    };
  };
  Error("_mwalloc: out of memory");
  errno = -ENOMEM;
  return NULL;
};

void * _mwrealloc(void * adr, int newsize) 
{
  int size, copylength, rc;
  void * newbuffer;
  if (adr == NULL ) return NULL; 

  rc = attachheap();
  if (rc != 0) {
    errno = -rc;
    return NULL;
  };
 
  size = getchunksizebyadr(adr - sizeof(chunkhead));
  if (size > 0) {
    size *= _mwHeapInfo->basechunksize;
    DEBUG1("newsize = %d oldsize = %d", newsize, size);

    /* check to see if newsize need the same size of buffer. */
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
  chunkhead *pCHead;
  int bin, rc;

  rc = attachheap();
  if (rc != 0) {
    return rc;
  };

  /* adr point to the data area of a chunk, which lies between the
     chunkhead and chunkfoot*/
  pCHead = adr - sizeof(chunkhead);;
  
  rc = getchunksizebyadr(pCHead);
  if (rc < 0) return -ENOENT;
  bin = log(pCHead->size) / log(2);
  pCHead->ownerid = UNASSIGNED;

  rc = lock(bin); /* lock sem for bin */
  rc = pushchunk(pCHead, &_mwHeapInfo->chunk[bin], 
		 &_mwHeapInfo->freecount[bin]);
  if (rc != 0) {
    Error("mwfree: shm chunk of size %ld lost, reason %d", 
	  pCHead->size, rc);
  };
  _mwHeapInfo->inusecount --;
  unlock(bin);
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


