/*
  MidWay
  Copyright (C) 2000 Terje Eggestad

  MidWay is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License as
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

/* $Id$ */

/*
 * $Log$
 * Revision 1.5  2002/08/09 20:50:16  eggestad
 * A Major update for implemetation of events and Task API
 *
 * Revision 1.4  2002/07/07 22:45:48  eggestad
 * *** empty log message ***
 *
 * Revision 1.3  2001/10/03 22:38:32  eggestad
 * plugged mem leaks
 *
 * Revision 1.2  2000/08/31 22:10:02  eggestad
 * DEBUG level set propper
 *
 * Revision 1.1  2000/07/20 18:49:59  eggestad
 * The SRB daemon
 *
 */

/* the purpose of this module is to make an "Object" that stores
 * messages, while request has been sent to the outside or
 * inside. This message needs to be retrived when the reply comes in
 * order to send a complete reply back.  espesially since thhe request
 * may hold fields we ignore, that shall be sent back.
 */

/* it really should've been made in C++ */


#include <string.h>
#include <stdlib.h>

#include <MidWay.h> 
#include <urlencode.h>


/* we really should have a) an index for faster searches, maybe a hash
 * per cltid and b) a time stamp with a cleanup function.  Call
 * requests may dissapear, since servers may core dump while
 * processing them. In a well behaved app this should be a non
 * problem. However, we start searching with the latest additions,
 * since we do then not search thru old forgotten entries. This is a
 * deliberate memory leak. The time stamp would fix it, but since we
 * assume a non well behaved app, deadlines are probably -1 anyway...*/

struct PendingCall  {
  int fd;
  unsigned int nethandle;
  unsigned int ipchandle;
  CLIENTID cltid;
  urlmap * mappedmsg;
  struct PendingCall * next;
};

/*typedef struct _PendingCall PendingCall;*/

static struct PendingCall * CallsInProgress = NULL;
static struct PendingCall * CallsFreeList = NULL;

static struct PendingCall * freePendingCall(void)
{
  struct PendingCall * PCtmp;

  if (CallsFreeList == NULL) {
    PCtmp = (struct PendingCall *) malloc(sizeof(struct PendingCall));
    DEBUG2("Alloced new PendingCall struct");
  } else {
    PCtmp = CallsFreeList;
    CallsFreeList = PCtmp->next;
  };
  return PCtmp;
};

static void clearPendingCall(struct PendingCall * PCtmp)
{
  PCtmp->fd = -1;
  PCtmp->cltid = -1;
  PCtmp->mappedmsg = NULL;
  PCtmp->next = NULL;
  return;
};


void storePushCall(CLIENTID cid, int nethandle, int fd, urlmap *map)
{
  struct PendingCall * PCtmp;

  DEBUG2("Pushing PendingCall id=%d, handle=%u fd=%d, map @ %#x", 
	cid&MWINDEXMASK, nethandle, fd, map);
  PCtmp = freePendingCall();
  PCtmp->fd = fd;
  PCtmp->nethandle = nethandle;
  PCtmp->cltid = cid;
  PCtmp->mappedmsg = map;
  PCtmp->next = CallsInProgress;
  CallsInProgress = PCtmp;
  return;
};

/* the Pop and Get call look the same, but in the case of Get, the map
   points to the struct in the list, and MUST NOT be free'ed. In the
   case of Pop we remove the entry in the list, and the map MUST be
   free'ed.  Get is used in teh case the service reply has RC =
   MWMORE, while Pop is used if RC = {MWSUCESS|MWFAIL}
*/
int storeGetCall(CLIENTID cid, int ipchandle, int * fd,  urlmap **map)
{
  struct PendingCall * PCthis = NULL;

  if (CallsInProgress == NULL) {
    if (fd != NULL)
      *fd = -1;
    if (map != NULL)
      *map = NULL;
    DEBUG2("Failed to Get PendingCall id=%d, ipchandle=%u empty list", 
	  cid&MWINDEXMASK, ipchandle);
    return 0;
  }

  PCthis = CallsInProgress;
  while(PCthis != NULL) {
    if ( (PCthis->ipchandle == ipchandle) && (PCthis->cltid == cid) ) {
      if (fd != NULL)*fd = PCthis->fd;
      if (map != NULL)*map = PCthis->mappedmsg;

      DEBUG2("Getting PendingCall id=%d, ipchandle=%u", 
	    cid&MWINDEXMASK, ipchandle);
      return 1;
    }
    PCthis = PCthis->next;
  };
  DEBUG2("Failed to Get PendingCall id=%d, ipchandle=%u", 
	cid&MWINDEXMASK, ipchandle);
  if (fd != NULL)
    *fd = -1;
  if (map != NULL)
    *map = NULL;
  return 0;
};


int storePopCall(CLIENTID cid, int ipchandle, int * fd,  urlmap **map)
{
  struct PendingCall * PCthis = NULL, * PCprev = NULL;

  if (CallsInProgress == NULL) {
    if (fd != NULL)
      *fd = -1;
    if (map != NULL)
      *map = NULL;
    DEBUG2("Failed to Pop PendingCall id=%d, ipchandle=%u empty list", 
	  cid&MWINDEXMASK, ipchandle);
    return 0;
  }
    
  PCthis = CallsInProgress;
  
  /* if top in list, should be the majority of the cases */
  if ( (PCthis->ipchandle == ipchandle) && (PCthis->cltid == cid) ) {
    if (fd != NULL)*fd = PCthis->fd;
    if (map != NULL)*map = PCthis->mappedmsg;
    CallsInProgress = PCthis->next;
    PCthis->next = CallsFreeList;
     
    clearPendingCall(PCthis);    
    PCthis->next = CallsFreeList;
    CallsFreeList = PCthis;

    DEBUG2("Poping PendingCall id=%d, ipchandle=%u (top)", 
	  cid&MWINDEXMASK, ipchandle);  
    return 1;
  };
  
  PCprev = PCthis;
  PCthis = PCthis->next;
  while(PCthis != NULL) {
    if ( (PCthis->ipchandle == ipchandle) && (PCthis->cltid == cid) ) {
      if (fd != NULL)*fd = PCthis->fd;
      if (map != NULL)*map = PCthis->mappedmsg;
      PCprev->next = PCthis->next;

      clearPendingCall(PCthis);
      PCthis->next = CallsFreeList;
      CallsFreeList = PCthis;

      DEBUG2("Poping PendingCall id=%d, ipchandle=%u", 
	    cid&MWINDEXMASK, ipchandle);
      return 1;
    }
    PCprev = PCthis;
    PCthis = PCthis->next;
  };
  DEBUG2("Failed to Pop PendingCall id=%d, ipchandle=%u", 
	cid&MWINDEXMASK, ipchandle);
  if (fd != NULL)
    *fd = -1;
  if (map != NULL)
    *map = NULL;
  return 0;
};

int storeSetIPCHandle(CLIENTID cid, int nethandle, int fd, int ipchandle)
{
  struct PendingCall * PCthis = NULL;

  if (CallsInProgress == NULL) return -1;
  
  PCthis = CallsInProgress;
  while (PCthis != NULL) {
    if ( (PCthis->cltid == cid) &&
	 (PCthis->nethandle == nethandle) &&
	 (PCthis->fd == fd) ) {
      PCthis->ipchandle = ipchandle;
      DEBUG2("Set ipc handle %#x pending call client=%d handle=%#x",
	    ipchandle, cid, nethandle);
      return 0;
    };
    PCthis = PCthis->next;
  };
  DEBUG2("Set ipc handle %#x pending call client=%d handle=%#x FAILED",
	ipchandle, cid, nethandle);
  return -1;
};

DECLAREMUTEX(callmutex);

int  storeLockCall(void)
{
  LOCKMUTEX(callmutex);
};

int  storeUnLockCall(void)
{
  UNLOCKMUTEX(callmutex);
};



/***********************************************************************
 * DATABUFFERS
 ***********************************************************************/

struct DataBuffer
{
  unsigned int nethandle;
  int fd;
  int chunk;
  int leftout;
  char * data;
  int datalen;
  struct DataBuffer * next;
};


static struct DataBuffer * DataBuffersIncomming = NULL;
//static struct DataBuffer * DataBuffersOutgoing = NULL;
static struct DataBuffer * DataBuffersFreeList = NULL;


static struct DataBuffer * freeDataBuffer(void)
{
  struct DataBuffer * DBtmp;

  if (CallsFreeList == NULL) {
    DBtmp = (struct DataBuffer *) malloc(sizeof(struct DataBuffer));
    DEBUG2("Alloced new DataBuffer struct");
  } else {
    DBtmp = DataBuffersFreeList;
    DataBuffersFreeList = DBtmp->next;
  };
  return DBtmp;
};

static void clearDataBuffer(struct DataBuffer * DBtmp)
{
  DBtmp->fd = -1;
  if (DBtmp->data != NULL) free(DBtmp->data);
  DBtmp->data = NULL;
  DBtmp->datalen = -1;
  DBtmp->chunk = -1;
  DBtmp->next = NULL;
  return;
};

void storePushDataBuffer(unsigned int nethandle, int fd, char * data, int datalen)
{
  struct DataBuffer * DBtmp;

  
  DEBUG2("Pushing DataBuffer for nethandle=%u fd=%d, %d bytes", 
	nethandle, fd, datalen);

  DBtmp = freeDataBuffer();
  DBtmp->fd = fd;
  DBtmp->nethandle = nethandle;
  DBtmp->data = malloc(datalen+1);
  DEBUG2("memcpy %d bytes %p => %p", datalen, data, DBtmp->data);
  memcpy(DBtmp->data, data, datalen);
  DBtmp->datalen = datalen;
  DBtmp->next = DataBuffersIncomming;
  DataBuffersIncomming = DBtmp;
  return;
};

int storePopDataBuffer(unsigned int nethandle, int fd, char ** data, int *datalen)
{
  struct DataBuffer * DBthis, * DBprev;

  if (data == NULL) return -2;
  if (datalen != NULL) return -2;

  DBthis = DataBuffersIncomming;
  

  while(DBthis != NULL) {

    if ( (DBthis->nethandle == nethandle) && (DBthis->fd == fd) ) {

      /* if top in list, should be the majority of the cases */
      if (DBprev == NULL) {
	DataBuffersIncomming = DBthis->next;
	DBthis->next = DataBuffersFreeList;
      } else {
	DBprev->next = DBthis->next;
      };

      *data = DBthis->data;
      *datalen = DBthis->datalen;

      clearDataBuffer(DBthis);
      DBthis->next = DataBuffersFreeList;
      DataBuffersFreeList = DBthis;

      DEBUG2("Poping DataBuffer fd=%d, nethandle=%u", 
	    fd, nethandle);

      return 1;
    }
    DBprev = DBthis;
    DBthis = DBthis->next;
  };
  DEBUG2("Failed to Pop DataBuffer fd=%d, nethandle=%u", 
	fd, nethandle);
  *data = NULL;
  *datalen = -1;
  return 0;
};
  

int storeAddDataBufferChunk(unsigned int nethandle, int fd, char * data, int len)
{
  struct DataBuffer * DBtmp;

  DBtmp = DataBuffersIncomming;
  while (DBtmp) {
    if ( (DBtmp->fd == fd) || (DBtmp->nethandle == nethandle) ) {
      DBtmp->datalen += len;
      DBtmp->data = realloc(DBtmp->data, DBtmp->datalen);  
      DEBUG2("memcpy add %d bytes %p => %p", len, data, DBtmp->data);
      memcpy(DBtmp->data, data, len);
      DEBUG2("Adding %d data to DataBuffer fd=%d, nethandle=%u", 
	    len, fd, nethandle);
      return DBtmp->chunk;
    }
    DBtmp = DBtmp->next;
  };
  Warning("Failed to add data to DataBuffer fd=%d, nethandle=%u", 
	fd, nethandle);
  return -1;
};

int storeGetDataBufferChunk(unsigned int nethandle, int fd)
{
  struct DataBuffer * DBtmp;

  if (DataBuffersIncomming == NULL) return -1;
  DBtmp = DataBuffersIncomming;
  while (DBtmp) {
    if ( (DBtmp->fd == fd) || (DBtmp->nethandle == nethandle) ) {
      return DBtmp->chunk;
    }
    DBtmp = DBtmp->next;
  };
  return -1;
};

int storeSetDataBufferChunk(unsigned int nethandle, int fd, int chunk)
{
  struct DataBuffer * DBtmp;
  int x;

  if (DataBuffersIncomming == NULL) return -1;
  DBtmp = DataBuffersIncomming;
  while (DBtmp) {
    if ( (DBtmp->fd == fd) || (DBtmp->nethandle == nethandle) ) {
      x = DBtmp->chunk;
      DBtmp->chunk = chunk;
      return x;
    }
    DBtmp = DBtmp->next;
  };
  return -1;
};

int storeDataBufferChunk(unsigned int nethandle, int fd, int chunk)
{
  struct DataBuffer * DBtmp;
  int x;

  if (DataBuffersIncomming == NULL) return -1;
  DBtmp = DataBuffersIncomming;
  while (DBtmp) {
    if ( (DBtmp->fd == fd) || (DBtmp->nethandle == nethandle) ) {
      x = DBtmp->chunk;
      DBtmp->chunk = chunk;
      return x;
    }
    DBtmp = DBtmp->next;
  };
  return -1;
};



/***********************************************************************
 * ATTACHES
 * while Calls may dissapear, Attaches may not, and not index, nor
 * clean up should be neccessary 
 ***********************************************************************/

struct PendingAttach {
  int fd;
  int connid;
  char * cname;
  urlmap * mappedmsg;
  struct PendingAttach * next;
};

static struct PendingAttach * AttachInProgress = NULL;
static struct PendingAttach * AttachFreeList = NULL;

static struct PendingAttach * freePendingAttach(void)
{
  struct PendingAttach * PAtmp;

  if (AttachFreeList == NULL) {
    PAtmp = (struct PendingAttach *) malloc(sizeof(struct PendingAttach));
    DEBUG2("Alloced new PendingAttach struct");
  } else {
    PAtmp = AttachFreeList;
    AttachFreeList = PAtmp->next;
  };
  return PAtmp;
};

static void clearPendingAttach(struct PendingAttach * PAtmp)
{
  PAtmp->fd = -1;
  if (PAtmp->cname != NULL) free(PAtmp->cname);
  PAtmp->cname = NULL;
  PAtmp->mappedmsg = NULL;
  PAtmp->next = NULL;
  return ;
};

void storePushAttach(char * cname, int connid, int fd, urlmap * map)
{
  struct PendingAttach * PAtmp;

  DEBUG2("Pushing PendingAttach cname=%s connid=%d, fd=%d", 
	cname, connid, fd);
  PAtmp = freePendingAttach();
  PAtmp->fd = fd;
  PAtmp->connid = connid;
  PAtmp->cname = strdup(cname);
  PAtmp->mappedmsg = map;
  PAtmp->next = AttachInProgress;
  AttachInProgress = PAtmp;
  return;
};

int  storePopAttach(char * cname, int *connid, int * fd, urlmap ** map)
{
  struct PendingAttach * PAthis = NULL, * PAprev = NULL;

  if (AttachInProgress == NULL) {
    if (fd != NULL)
      *fd = -1;
    if (map != NULL)
      *map = NULL;
    if (connid != NULL)
      *connid = -1;
    return 0;
  }

  PAthis = AttachInProgress;
  /* if top in list, should be the majority of the cases */
  if (strcmp(PAthis->cname, cname) == 0)  {
    if (fd != NULL)
      *fd = PAthis->fd;
    if (map != NULL)
      *map = PAthis->mappedmsg;
    if (connid != NULL)
      *connid = PAthis->connid;

    free(PAthis->cname);
    PAthis->mappedmsg = NULL;
    PAthis->connid = -1;
    
    AttachInProgress = PAthis->next;
    PAthis->next = AttachFreeList;
    AttachFreeList = PAthis;
    DEBUG2("Poping PendingAttach cname=%s (top)", 
	  cname);  
    return 1;
  };
  
  PAprev = PAthis;
  PAthis = PAthis->next;
  while(PAthis != NULL) {
    if (strcmp(PAthis->cname, cname) == 0) {
      if (fd != NULL)
	*fd = PAthis->fd;
      if (map != NULL)
	*map = PAthis->mappedmsg;
      if (connid != NULL)
	*connid = PAthis->connid;
      
      free(PAthis->cname);
      PAthis->mappedmsg = NULL;
      PAthis->connid = -1;
      
      PAprev->next = PAthis->next;
      clearPendingAttach(PAthis);
      PAthis->next = AttachFreeList;
      AttachFreeList = PAthis;
      DEBUG2("Poping PendingAttach cname=%s", 
	    cname);  
      return 1;
    }
    PAprev = PAthis;
    PAthis = PAthis->next;
  };
  DEBUG2("Failed to Pop PendingAttach cname=%s", 
	cname);
  
  if (fd != NULL)
    *fd = -1;
  if (map != NULL)
    *map = NULL;
  if (connid != NULL)
    *connid = -1;
  return 0;
};


#ifdef MODULETEST

int main(int argc, char ** argv)
{
  int fd, rc, cid, handle;
  char *cname, *m;
  int connid;

  char * m1 = "hei=hello&ja=yes";
  char * m2 = "hei=hello&ja=yes&Terje=master";
  char * m3 = NULL;
  urlmap * map, * map1, * map2, *map3;
  
  map1 = urlmapdecode(m1);
  map2 = urlmapdecode(m2);
  map3 = NULL; /*urlmapdecode("hei=hello&ja=yes&Terje=master");*/
  map3 = urlmapdecode(m3);

  if (argc > 1)
    mwsetloglevel(MWLOG_DEBUG2);

  /* TESTING CALL STACK */
  printf("\nCall stack tests\n");

  handle = 888; cid = 666; fd = 78; m = m1;
  map1 = urlmapdecode(m);
  printf("push call cid=%d, handle=%d fd=%d msg=%s\n", cid, handle, fd, m);
  storePushCall(cid, handle, fd, map1);

  handle = 884; cid = 664; fd = 76; m = m2;
  map1 = urlmapdecode(m);
  printf("push call cid=%d, handle=%d fd=%d msg=%s\n", cid, handle, fd, m);
  storePushCall(cid, handle, fd, map1);

  handle = 887; cid = 667; fd = 74; m = m3;
  map1 = urlmapdecode(m);
  printf("push call cid=%d, handle=%d fd=%d msg=%s\n", cid, handle, fd, m);
  storePushCall(cid, handle, fd, map1);




  handle = 888;
  cid = 666;
  rc = storePopCall(cid,handle,&fd,&map);
  printf("pop of cid=%d handle=%d rc=%d fd=%d message=%s\n", cid, handle, rc, 
	 fd, urlmapencode(map)); 

  handle = 888;
  cid = 666;
  rc = storePopCall(cid,handle,&fd,&map);
  printf("pop of cid=%d handle=%d rc=%d fd=%d message=%s\n", cid, handle, rc, 
	 fd, urlmapencode(map)); 

  handle = 884;
  cid = 664;
  rc = storePopCall(cid,handle,&fd,&map);
  printf("pop of cid=%d handle=%d rc=%d fd=%d message=%s\n", cid, handle, rc, 
	 fd, urlmapencode(map)); 

  handle = 887;
  cid = 667;
  rc = storePopCall(cid,handle,&fd,&map);
  printf("pop of cid=%d handle=%d rc=%d fd=%d message=%s\n", cid, handle, rc, 
	 fd, urlmapencode(map)); 

  handle = 883;
  cid = 1;
  rc = storePopCall(cid,handle,&fd,&map);
  printf("pop of cid=%d handle=%d rc=%d fd=%d message=%s\n", cid, handle, rc, 
	 fd, urlmapencode(map)); 

  handle = 883;
  cid = 663;
  rc = storePopCall(cid,handle,&fd,&map);
  printf("pop of cid=%d handle=%d rc=%d fd=%d message=%s\n", cid, handle, rc, 
	 fd, urlmapencode(map)); 

  handle = 1;
  cid = 1;
  rc = storePopCall(cid,handle,&fd,&map);
  printf("pop of cid=%d handle=%d rc=%d fd=%d message=%s\n", cid, handle, rc, 
	 fd, urlmapencode(map)); 




  /* TESTING CALL STACK */
  printf("\nAttach stack tests\n");
  map1 = urlmapdecode(m1);
  map2 = urlmapdecode(m2);
  map3 = NULL; /*urlmapdecode("hei=hello&ja=yes&Terje=master");*/
  map3 = urlmapdecode(m3);

  storePushAttach("test", 567,78,map1);
  storePushAttach("test", 568,74,map3);
  storePushAttach("junk", 569,79,map2);


  cname = "tes";
  rc = storePopAttach(cname, &connid,&fd,&map);
  printf("pop of cname=%s connid=%d rc=%d fd=%d message=%s\n", 
	 cname, connid, rc, fd, urlmapencode(map)); 

  cname = "test";
  rc = storePopAttach(cname, &connid,&fd,&map);
  printf("pop of cname=%s connid=%d rc=%d fd=%d message=%s\n", 
	 cname, connid, rc, fd, urlmapencode(map)); 

  cname = "testx";
  rc = storePopAttach(cname, &connid,&fd,&map);
  printf("pop of cname=%s connid=%d rc=%d fd=%d message=%s\n", 
	 cname, connid, rc, fd, urlmapencode(map)); 

  cname = "junk";
  rc = storePopAttach(cname, &connid,&fd,&map);
  printf("pop of cname=%s connid=%d rc=%d fd=%d message=%s\n", 
	 cname, connid, rc, fd, urlmapencode(map)); 

  cname = "test";
  rc = storePopAttach(cname, &connid,&fd,&map);
  printf("pop of cname=%s connid=%d rc=%d fd=%d message=%s\n", 
	 cname, connid, rc, fd, urlmapencode(map)); 

};

#endif
