/*
  MidWay
  Copyright (C) 2000 Terje Eggestad

  MidWay is free software; you can redistribute it and/or
  modify it under the terms of the GNU  General Public License as
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
 * $Id$
 * $Name$
 * 
 * $Log$
 * Revision 1.3  2000/09/21 18:55:18  eggestad
 * Corrected loglevel on a debugmessage
 *
 * Revision 1.2  2000/07/20 19:49:40  eggestad
 * prototype fixup.
 *
 * Revision 1.1.1.1  2000/03/21 21:04:27  eggestad
 * Initial Release
 *
 * Revision 1.1.1.1  2000/01/16 23:20:12  terje
 * MidWay
 *
 */

#include <stdio.h>
#include <errno.h>
#include <sys/shm.h>
#include <sys/sem.h>

#include <MidWay.h>
#include <ipctables.h>
#include <shmalloc.h>
#include "mwd.h"

static char * RCSId = "$Id$";
static char * RCSName = "$Name$"; /* CVS TAG */

/*
 * this need to change for more segments
 */
void * pSegmentStart = 0;
extern struct segmenthdr * _mwHeapInfo;

static int formatfreelist(int iStart, int basechunksize, 
		      int chunksize, int chunks)
{

  /* iStart is in offset and we return offset. But in order to use the 
     structs cinfo and fptr are assiged absolutes. This makes for 
     messy code. Remember that _mwfooter operate on absolutes as well
  */

  /* 
   * we assume that basechunksize is divisible by 4 (Wordsize)
   * chunksize is in basechunksize(s)
   */

  /* here I do something I otherwise selvdom do, i prefix each variable with 
     data type, i for int, p for pointer.
     THis is to keep oversigth over the caos we ahev joggeling absolute
     addresses (pointers) and offsets into the segment which is int.
     remember all mengers of the struct chunkhead and chunkfoot are in int.
  */
  chunkhead * pCHead;
  chunkfoot * pCFoot;
  int i, iCSize, iCHead;;

  iCSize = _mwHeapInfo->basechunksize * chunksize;
  mwlog(MWLOG_DEBUG, "formating %d chunks of %d octets (%d * base) at 0x%x",
	  chunks, iCSize, chunksize, iStart);
    
  for (i = 0; i < chunks; i++) {
    iCHead = iStart + i * (iCSize + CHUNKOVERHEAD);
    pCHead =  _mwoffset2adr(iCHead);
    pCHead->ownerid = -1;                  
    pCHead->size = chunksize;
    
    /* we probably should format the data region */
    pCFoot = _mwfooter(pCHead);
    pCFoot->above = iCHead;
    pCFoot->next = iCHead + (iCSize + CHUNKOVERHEAD);
    pCFoot->prev = iCHead - (iCSize + CHUNKOVERHEAD);
    
    mwlog(MWLOG_DEBUG2, "chunk %d: %d octes at %#x footer at %#x next at %#x prev at %#x", 
	  i, pCHead->size * _mwHeapInfo->basechunksize,  pCHead, pCFoot, pCFoot->next, pCFoot->prev);

  }
  /* end and begining closure, remember this is a ring */
  pCHead = _mwoffset2adr(iStart);
  pCFoot = _mwfooter(pCHead);
  pCFoot->prev = iStart + (chunks-1) * (iCSize + CHUNKOVERHEAD);
  mwlog(MWLOG_DEBUG1, "correcting first chunk at 0x%x next at 0x%x prev at 0x%x", 
	pCHead, pCFoot->next, pCFoot->prev);
      
  pCHead = _mwoffset2adr(pCFoot->prev);
  pCFoot = _mwfooter(pCHead);
  pCFoot->next = iStart;
  mwlog(MWLOG_DEBUG1, "correcting last chunk at 0x%x next at 0x%x prev at 0x%x", 
	pCHead, pCFoot->next, pCFoot->prev);

  return iStart + (chunks) * (iCSize + CHUNKOVERHEAD);;
};

/*
 * we return the shmid.
 */
int shmb_format(int mode, long chunksize, long chunkspersize)
{
  int i, firstcorrection;
  int iChunkRoot;
  int segmentid;
  long segmentsize;
  unsigned short * semarray;

  /*
   * segment size are the size of all chunks minus overheads and header.
   */
  
  mwlog(MWLOG_DEBUG, "shmhead format with chunksize = %d, chunkspersize %d",
	  chunksize, chunkspersize);
  
  segmentsize = sizeof(struct segmenthdr) 
    + 6*chunkspersize*CHUNKOVERHEAD
    + 32*chunksize*chunkspersize + 100000;

  mwlog(MWLOG_DEBUG, "there are %d onesizes, %d chunks per size overhead is %d, total %d",
	 32*chunkspersize,
	 chunkspersize,
	 6*chunkspersize*CHUNKOVERHEAD,
	 segmentsize);
  
  /* permission are the responsibility of... someone else LOOKATME */
  segmentid = shmget(IPC_PRIVATE,segmentsize,IPC_CREAT|IPC_EXCL|mode); 
  
  if (segmentid == -1) {
    mwlog(MWLOG_ERROR,"Failed to create shm seg errno=%d",errno);
    return -1;
  }
  
  pSegmentStart = shmat(segmentid,NULL,0);
  if (pSegmentStart == (void *) -1) {
    shmctl(segmentid,IPC_RMID,NULL);
    mwlog(MWLOG_ERROR,"Failed to attach shm seg errno=%d",errno);
    return -1;
  };
  
  mwlog(MWLOG_DEBUG,"Shared memory seg %d attached at 0x%x size %d ends at 0x%x",
	  segmentid,pSegmentStart, segmentsize, pSegmentStart+segmentsize);

  /* now we have a attached shm segment, formating... */
  
  _mwHeapInfo = pSegmentStart;

  /* Magic number is "MW" thus thus 0x4D57 */
  /*_mwHeapInfo->magic = (short) 'M';
  
    _mwHeapInfo->magic <<8;
    _mwHeapInfo->magic += (short) 'W';*/
  _mwHeapInfo->magic =  0x4D57;
  _mwHeapInfo->chunkspersize = chunkspersize;
  _mwHeapInfo->basechunksize = chunksize;
  _mwHeapInfo->segmentsize = segmentsize;

  /* initialize semaphores */
  _mwHeapInfo->semid = semget(IPC_PRIVATE, BINS, mode);

  if (_mwHeapInfo->semid == -1) {
    mwlog(MWLOG_ERROR,
	  "Failed to get semaphores for shm buffers errno=%d",errno);
    return -1;
  };

  semarray = (ushort *) malloc(sizeof(unsigned short) * BINS);
  for (i = 0; i < BINS; i++) {
    semarray [i] = 1; /* unlock */
  };
  semctl(_mwHeapInfo->semid, BINS, SETALL, semarray);
  free(semarray);

  _mwHeapInfo->inusecount = 0;
  _mwHeapInfo->inusehighwater = 0;
  _mwHeapInfo->inuseaverage = 0;
  _mwHeapInfo->inuseavgcount = 0;
  _mwHeapInfo->numbins = BINS;

  /* all pointers in heap are offsets from start of shm segment */
  iChunkRoot =  sizeof(struct segmenthdr);
  for (i = 0; i < _mwHeapInfo->numbins-1; i++) {
    /* remember double number of chunk of size = base * 1. */
    if (i == 0) firstcorrection = 2;
    else firstcorrection = 1;
    _mwHeapInfo->freecount[i] = chunkspersize * firstcorrection; 
    _mwHeapInfo->chunk[i] = iChunkRoot;
    iChunkRoot = formatfreelist(_mwHeapInfo->chunk[i],  chunksize,  1<<i, 
			      chunkspersize * firstcorrection);
  };

  /* the rest group is currently noe allocated */
  _mwHeapInfo->freecount[_mwHeapInfo->numbins-1] = 0; 
  _mwHeapInfo->chunk[_mwHeapInfo->numbins-1] = 0;
  
  return segmentid;
};

int shmb_info(int chunksize, 
	      int * usage, int * average, int * highwatermark, int * max)
{
  void * bufferlist = NULL;
  struct segmenthdr * seghdr;

  if (pSegmentStart == NULL) {
    return -EILSEQ;
  };
  seghdr = pSegmentStart;

  switch(chunksize) {
  case 0:
    if (usage != NULL) {         * usage =         seghdr->inusecount;};
    if (highwatermark != NULL) { * highwatermark = seghdr->inusehighwater;};
    if (average != NULL) {       * average =       seghdr->inuseaverage;};
    if (max != NULL) {           * max =           seghdr->chunkspersize*32;
    };
    return 0;
  case 1:
    if (usage != NULL) *usage = (seghdr->chunkspersize*2) - seghdr->freecount[chunksize];
    if (max != NULL)   *max = (seghdr->chunkspersize*2);
    break;
  case 2:
    if (usage != NULL) 
      *usage = (seghdr->chunkspersize) - seghdr->freecount[chunksize];
    if (max != NULL)   
      *max = (seghdr->chunkspersize);
    break;
  case 4:
    if (usage != NULL) *usage = (seghdr->chunkspersize) - seghdr->freecount[chunksize];
    if (max != NULL)   *max = (seghdr->chunkspersize);    
    break;
  case 8:
    if (usage != NULL) *usage = (seghdr->chunkspersize) - seghdr->freecount[chunksize];
    if (max != NULL)   *max = (seghdr->chunkspersize);    
    break;
  case 16:
    if (usage != NULL) *usage = (seghdr->chunkspersize) - seghdr->freecount[chunksize];
    if (max != NULL)   *max = (seghdr->chunkspersize);    
    break;
  default:
    return EINVAL;
  };
  if (average != NULL) { * average = -1;};
  if (highwatermark != NULL) { * highwatermark = -1;};
  return 0;
};

/* used to clean up IPC from prev instancen, NOT our own! */
int shm_cleanup(int shmid)  
{
  pSegmentStart = shmat(shmid,NULL,0);
  _mwHeapInfo = pSegmentStart;
  semctl(_mwHeapInfo->semid, 0, IPC_RMID, 0);
  shmdt(pSegmentStart);
};

int shm_destroy(void)  
{
  mwlog(MWLOG_DEBUG, "destroying heap");
  if (_mwHeapInfo == NULL) return -ENOENT;
  semctl(_mwHeapInfo->semid, 0, IPC_RMID, 0);
  if (ipcmain != NULL) {
    shmctl(ipcmain->heap_ipcid, IPC_RMID, NULL);
    ipcmain->heap_ipcid = -1;
  };
  shmdt(_mwHeapInfo);
  _mwHeapInfo = NULL;
  pSegmentStart = 0;
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


