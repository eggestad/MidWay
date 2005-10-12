/*
  MidWay
  Copyright (C) 2000,2005 Terje Eggestad

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
 * Revision 1.10  2005/10/12 22:46:27  eggestad
 * Initial large data patch
 *
 * Revision 1.9  2004/11/17 20:56:41  eggestad
 * fix for double free (pointer not NULL'ed)
 *
 * Revision 1.8  2004/04/12 23:05:25  eggestad
 * debug format fixes (wrong format string and missing args)
 *
 * Revision 1.7  2003/03/16 23:50:24  eggestad
 * Major fixups
 *
 * Revision 1.6  2002/10/17 22:18:28  eggestad
 * fix to handle calls from a gateway, not just clients
 *
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

/** @file
 * the purpose of this module is to make an "Object" that stores
 * messages, while request has been sent to the outside or
 * inside. This message needs to be retrived when the reply comes in
 * order to send a complete reply back.  espesially since thhe request
 * may hold fields we ignore, that shall be sent back.

 We now added the store of a Call pending data from SRB, this mean
 that I've moved the IPC acall in here.  Calls are kept in the store
 if data is pending or replies are pending. Call are now stored unless
 there are no additional data SRC messages AND noreply flag is set.

 */

/* it really should've been made in C++ */


#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include <MidWay.h> 
#include <urlencode.h>
#include "store.h"
#include <ipcmessages.h>
#include <SRBprotocol.h>

static char * RCSId UNUSED = "$Id$";

/*
   mutex for this module
*/
DECLAREMUTEX(callmutex);

static int storeLockCall(void)
{
   LOCKMUTEX(callmutex);
};

static int storeUnLockCall(void)
{
   UNLOCKMUTEX(callmutex);
};

#if 0 // replaced by callstore

/** we really should have a) an index for faster searches, maybe a
 * hash per cltid and b) a time stamp with a cleanup function.  Call
 * requests may dissapear, since servers may core dump while
 * processing them. In a well behaved app this should be a non
 * problem. However, we start searching with the latest additions,
 * since we do then not search thru old forgotten entries. This is a
 * deliberate memory leak. The time stamp would fix it, but since we
 * assume a non well behaved app, deadlines are probably -1
 * anyway...*/

struct PendingCall  {
   Connection * origin_conn, * routed_conn;
   SRBhandle_t nethandle;
   mwhandle_t ipchandle;
   MWID mwid; 
   int callflag; 
   int complete;
   urlmap * map;

   int totaldata;
   int datalen;
   char * data;

   struct PendingCall * next;
};

struct PendingData {
   int totaldata;
   int datalen;
   char * data;
};


/*typedef struct _PendingCall PendingCall;*/

static struct PendingCall * CallsInProgress = NULL;
static struct PendingCall * CallsFreeList = NULL;

// We use a hash consisting of the lower 3 bit id mwid, and the lower 5 bit of callerid;
// it'll be in all probability unique, and should have a good enough distibution. 
// for now at least

static struct PendingCall * hash[256] = { NULL };

static unsigned char mkhash(MWID mwid, SRBhandle_t handle)
{
   unsigned char h;
   unsigned char a,b,c,d;
   
   a = handle & 0xff;
   b = (handle >> 8) & 0xff;

   mwid <<= 5;
   c = (handle >> 16) & 0xff;
   d = (handle >> 24) & 0xff;

   mwid &= 0xff;

   h = ((a ^ b) ^ (c ^ d)) ^ mwid;
   
   DEBUG2("Hash of mwid %d and callerid %d = %hhu", mwid, handle, h);
   return h;
};

/** 
    @brief Get a free PendingCall structure

    get from the free list or alloc a struct PendingCall 
*/
static struct PendingCall * getfreePendingCall(void)
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


/**
   zero out a struct PendingCall
*/
static inline void clearPendingCall(struct PendingCall * PCtmp)
{
   memset(PCtmp, 0, sizeof(struct PendingCall))
   PCtmp->fd = -1;
   PCtmp->mwid = -1;
   return;
};

static void freeCall(struct PendingCall * PCthis) 
{   
   clearPendingCall(PCthis);    
   PCthis->next = CallsFreeList;
   CallsFreeList = PCthis;
};

void storePushCall(MWID mwid, int nethandle, Connection * conn, urlmap *map)
{
  struct PendingCall * PCtmp;

  DEBUG2("Pushing PendingCall id=%#x, handle=%u fd=%d, map @ %p", 
	mwid, nethandle, fd, map);
  PCtmp = getfreePendingCall();
  PCtmp->conn = conn;
  PCtmp->nethandle = nethandle;
  PCtmp->mwid = mwid;
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
static struct PendingCall * storeGetCall(MWID mwid, int ipchandle);
{
  struct PendingCall * PCthis = NULL;

  if (CallsInProgress == NULL) {
    DEBUG2("Failed to Get PendingCall id=%x, ipchandle=%u empty list", 
	  mwid, ipchandle);
    return NULL;
  }

  PCthis = CallsInProgress;
  while(PCthis != NULL) {
    DEBUG2("checking pending call: ipckey  %u ?= %u and mwid  %x ?= %x", 
	   PCthis->ipchandle,  ipchandle, PCthis->mwid, mwid);
    if ( (PCthis->ipchandle == ipchandle) && (PCthis->mwid == mwid) ) {
      DEBUG2("Getting PendingCall id=%x, ipchandle=%u", 
	    mwid, ipchandle);
      return PCthis;
    }
    PCthis = PCthis->next;
  };
  DEBUG2("Failed to Get PendingCall id=%x, ipchandle=%u", 
	mwid, ipchandle);
  return NULL;
};


static struct PendingCall * storePopCall(MWID mwid, int ipchandle)
{
  struct PendingCall * PCthis = NULL, * PCprev = NULL;

  if (CallsInProgress == NULL) {
    DEBUG2("Failed to Pop PendingCall id=%x, ipchandle=%u empty list", 
	  mwid, ipchandle);
    return NULL;
  }
    
  PCthis = CallsInProgress;
  
  /* if top in list, should be the majority of the cases */
  DEBUG2("checking pending call: ipckey  %u ?= %u and mwid  %x ?= %x", 
	 PCthis->ipchandle,  ipchandle, PCthis->mwid, mwid);
  if ( (PCthis->ipchandle == ipchandle) && (PCthis->mwid == mwid) ) {
    CallsInProgress = PCthis->next;


    DEBUG2("Poping PendingCall id=%x, ipchandle=%u (top)", 
	  mwid, ipchandle);  
    return PCthis;
  };
  
  PCprev = PCthis;
  PCthis = PCthis->next;
  while(PCthis != NULL) {
    DEBUG2("checking pending call: ipckey  %u ?= %u and mwid  %x ?= %x", 
	 PCthis->ipchandle,  ipchandle, PCthis->mwid, mwid);
    if ( (PCthis->ipchandle == ipchandle) && (PCthis->mwid == mwid) ) {
      PCprev->next = PCthis->next;

      DEBUG2("Poping PendingCall id=%x, ipchandle=%u", 
	    mwid, ipchandle);
      return PCthis;
    }
    PCprev = PCthis;
    PCthis = PCthis->next;
  };
  DEBUG2("Failed to Pop PendingCall id=%x, ipchandle=%u", 
	mwid, ipchandle);
  return NULL;
};

static int storeSetIPCHandle(MWID mwid, int nethandle, Connection * conn, int ipchandle)
{
  struct PendingCall * PCthis = NULL;

  if (CallsInProgress == NULL) return -1;
  
  PCthis = CallsInProgress;
  while (PCthis != NULL) {
    if ( (PCthis->mwid == mwid) &&
	 (PCthis->nethandle == nethandle) &&
	 (PCthis->conn == conn) ) {
      PCthis->ipchandle = ipchandle;
      DEBUG2("Set ipc handle %#x pending call client=%d handle=%#x",
	    ipchandle, mwid, nethandle);
      return 0;
    };
    PCthis = PCthis->next;
  };
  DEBUG2("Set ipc handle %#x pending call client=%d handle=%#x FAILED",
	ipchandle, mwid, nethandle);
  return -1;
};



/***********************************************************************
 * DATABUFFERS
 ***********************************************************************/

/**
 * the unique tuple is: (mwid, hanlde)
 * a client/server may only have one large data transfer going at one time. 
 * a gateway may have many, but on behalf on their clients.
 */
static struct PendingData * lookupPendingData(MWID mwid, SRBhandle_t handle, 
						   int callreplyflag, struct PendingData *** root)
{
   unsigned char h;
   struct PendingData * tmp = NULL, **ptmp = NULL;
   
   h = mkhash(mwid, handle);

   ptmp = &hash[h];
   tmp = hash[h];
   DEBUG2("lookup (mwid=%d, handle=%u %s hash=%d)", mwid, handle, CALLREPLYSTR(callreplyflag), h);

   while(tmp != NULL) {
      DEBUG2("checking mwid=%d, handle=%u %s svc=%s data %d/%d", 
	     tmp->mwid, tmp->srbhandle, CALLREPLYSTR(tmp->callflag), 
	     tmp->cmsg.service, tmp->datalen, tmp->totaldata);
      if ( (tmp->mwid == mwid) && (tmp->srbhandle == handle) && (tmp->callflag == callreplyflag) )
	 break;
      ptmp = &tmp->next;
      tmp = tmp->next;      
   }
   if (!tmp) {
      DEBUG2("lookup (mwid=%d, handle=%u %s) failed", 
	  mwid, handle, CALLREPLYSTR(callreplyflag));
      return NULL;;
   };
   
   if (root) *root = ptmp;
   DEBUG2("lookup (mwid=%d, handle=%u %s) found %s data %d/%d", 
	  mwid, handle, CALLREPLYSTR(callreplyflag), tmp->cmsg.service, tmp->datalen, tmp->totaldata);
   return tmp;
};

/**
   used to release data data when all data has been  received and forwarded
*/
static void delPendingData(MWID mwid, SRBhandle_t handle, int  callreplyflag)
{
   unsigned char h;
   struct PendingData * tmp, **ptmp;

   DEBUG2("enter");
   tmp = lookupPendingData(mwid, handle, callreplyflag, &ptmp);
   if (!tmp) {
      return;
   };

   DEBUG2("freeing pending data: svc %s mwid=%d handle=%d call=%d", 
	  tmp->cmsg.service, tmp->mwid, tmp->srbhandle, tmp->callflag );

   *ptmp = tmp->next;

   if (tmp->data) mwfree(tmp->data);
   free(tmp);

   return;
};

/**
   FIXME: OBSOLETE?
*/
static struct PendingData *  addPendingData(MWID mwid, SRBhandle_t handle, int callflag)
{
   struct PendingData * s;
   unsigned char h;

   s = lookupPendingData(mwid, handle, callflag, NULL);
   
   if (s != NULL) {
      Error("already had a pending data flow for svc=%s mwid=%d handle=%d, discard the old", 
	    s->cmsg.service, mwid, handle);      
      del_pending_data(mwid, handle, CALLFLAG);
   };
   
   s = malloc(sizeof (struct PendingData));
   if (!s) {
      Error ("malloc failed for %d bytes", sizeof (struct PendingData));
      return NULL;
   };
   memset (s, '\0', sizeof (struct PendingData));

   s->callflag = callflag;

   DEBUG2("inserting pending data for svc mwid=%d handle=%u call=%d", 
	  s->mwid, s->srbhandle, callflag);

   h = mkhash(mwid, handle);   
   s->next = hash[h];
   hash[h] = s;
   return s;
};

/**
 *  @brief Used to add a inbound call that has pending data
 *  @param mwid the id of the remote gateway
 *  @param handle  the SRB Handle
 *  @param cmsg the IPC message to send upun complete data
 *  @param totallen the total amount of data in the reply
 *  
 *  @return 0 or -ENOMEM
 */
int addPendingReplyData(MWID mwid, SRBhandle_t handle, Call * cmsg, int totallen)
{
   struct PendingData * s;
   
   DEBUG2("enter");
   s = add_pending_data(mwid, handle, REPLYFLAG);
   if (!s) return -ENOMEM;
   
   memcpy(&s->cmsg, cmsg, sizeof(Call));
   s->datalen = 0;
   s->data = mwalloc(totallen);

   return 0; 
};

 int appendPendingData(MWID mwid, SRBhandle_t handle, int callreply, char * data, int datalen, 
			       struct PendingData ** ps)
{
   struct PendingData * s;

   s = lookupPendingData(mwid, handle, callreply, NULL);
   
   if (!s) {
      Error ("lookup of %d %u %d failed, nowhere to append incomming srb data", mwid, handle, callreply);
      return -1;
   };

   if (datalen + s->datalen > s->totaldata) {
      Error("Attempt to add more data to a pending %s than expected: totallen %d already recv %d to add %d", 
	    CALLREPLYSTR(callreply), s->totaldata, s->datalen, datalen);
      return -E2BIG;
   };

   memcpy(s->data+s->datalen, data, datalen);
   s->datalen += datalen;
   DEBUG("added %*.*s data now %*.*s", datalen, datalen, data, s->datalen, s->datalen, s->data);
   if (ps) *ps = s;
   return s->totaldata - s->datalen;   
};



int storeAppendPendingCallData(MWID mwid, SRBhandle_t handle, char * data, int datalen)
{
   int rc;
   struct PendingData * s;

   DEBUG2("enter");
   rc = append_pending_data (mwid, handle, CALLFLAG, data, datalen, &s);

   if (rc > 0) { 
      return rc;
   };
   if (rc == 0) {
      DEBUG("all data recv for call doing IPC: %s %d %d %s", s->cmsg.service, s->mwid, s->srbhandle, s->data);
      TIMEPEGNOTE("doing _mwacallipc");
      rc = _mwacallipc (s->cmsg.service, s->data, s->datalen, s->cmsg.flags, s->mwid, 
			s->cmsg.instance, s->cmsg.domain, s->cmsg.callerid, s->cmsg.hops);
      TIMEPEGNOTE("done _mwacallipc");
   };
   del_pending_data(mwid, handle, CALLFLAG);
   return rc;
};


int storeAppendPendingReplyData(MWID mwid, SRBhandle_t handle, char * data, int datalen)
{
   int rc;
   struct PendingData  * s;
   
   DEBUG2("enter");
   rc = append_pending_data (mwid, handle, REPLYFLAG, data, datalen, &s);
   
   if (rc > 0) { 
      return rc;
   };
   if (rc == 0) {
      DEBUG("all data recv for reply doing IPC: %s %d %d %s", s->cmsg.service, s->mwid, s->srbhandle, s->data);
      // IPC      
   };
   del_pending_data(mwid, handle, REPLYFLAG);
   return rc;
};

/***********************************************************************
 * interface
 ***********************************************************************/

/**
   @brief insert a new Call from SRB into the store
   
   
 */
void storeNewSRBCall(Connection * conn, char * svcname, MWID mwid, MWID callerid, 
		  char * data, int datalen, int totallen, 
		  SRBhandle_t handle, char * instance, char * domain, SRBmessage * srbmsg)
{
   int datapending = 0, noreply = 0;
   int rc;
   struct PendingCall *pcall = NULL;
   Call cmsg = { 0 };

   noreply = flags & MWNOREPLY;
   DEBUG2("totalen = %d datalen = %d noreply=%d", totallen, datalen, noreply);
   
   if ( (totallen > 0) && ( totallen >= datalen) ) 
      datapending = 1;
   
   Assert(totallen >= datalen);
   
   // shortcut, in this case there is nothing to store
   if ( !datapending && noreply) {
      DEBUG("no data pending and no reply, calling ipc directly");
      rc = _mwacallipc (svcname, data, datalen, flags, mwid, instance, domain, callerid, hops);
      if (rc > 0) {
	 DEBUG("_mwacallipc failed with %d ", rc);   
	 _mw_srbsendcallreply(conn, srbmsg, NULL, 0, 0, _mw_errno2srbrc(rc), 0);
      };
      urlmapfree(srbmsg->map);
      srbmsg->map = NULL;
      return ;
   };
   
   storeLockCall();

   if (datapending || ! noreply) {
      pcall = storePushCall(mwid, handle, conn, srbmsg->map);
      srbmsg->map = NULL;
      
      DEBUG("Storing call fd=%d nethandle=%u mwid=%#x ipchandle=%d",
	    conn->fd, handle, mwid, rc);
   };

   storeUnLockCall();


   //* @todo WORMHOLE
   
   if (datapending) {
      if (noreply) pcall->complete = 1;
      pcall->data = mwalloc(totallen);
      memcpy(pcall->data, data, datalen);
      return;
   };

   pcall->callflag = CALLFLAG;

   rc = _mwacallipc (svcname, data, datalen, flags, mwid, instance, domain, callerid, hops);
   if (rc > 0) {
      DEBUG("_mwacallipc failed with %d ", rc);   
      _mw_srbsendcallreply(conn, srbmsg, NULL, 0, 0, _mw_errno2srbrc(rc), 0);
   };
   
   pcall->ipchandle = rc;
   
   DEBUG("done,  IPC handle = %d", rc);
   return;
};

void storeNewSRBReply(Connection * conn, char * svcname,  MWID callerid, int returncode, int appretcode, 
		  char * data, int datalen, int totallen, 
		  SRBhandle_t handle, char * instance, char * domain, SRBmessage * srbmsg, int hops)
{
   int datapending = 0, freeit = 0;
   int rc;
   struct PendingCall *pcall = NULL;
   struct PendingData *pdata = NULL; 

   DEBUG("new SRB reply on conn to %s service %s callerid %s rc=%d handle %x", 
	 conn->peeraddr_string, svcname, _mwid2str(callerid), returncode, handle);

   if ( (totallen >= 0) && (totallen > datalen) ) datapending = 1;
   
   if ((returncode == MWMORE) || datapending) freeit = 1;

   storeLockCall();

   if (freeit) {
      DEBUG("got a reply but not the last piece more=%d datapending = %d", returncode == MORE, datapending);
      pcall = storeGetCall(mwid, handle);
      if (returcode != MWMORE) pcall->complete = 1;
      if (datapending) pdata = storeAddPendingReply();
   } else {
      DEBUG("got the final piece of reply poping");
      pcall = storePopCall(mwid, handle);
   };

   Assert(pcall);
   storeUnLockCall();   
   
   
   if (pcall->origin_conn) {
      rc = _mw_srbsendmessage(origin_conn, srbmsg);      
      DEBUG("routed reply back to origin connection rc=%d", rc);
   } else {
      Call cmsg = { 0 };

      DEBUG("got a service reply that originated in this instance");
      cmsg.mtype = SVCREPLY;
      
      strncpy(cmsg.svcname, svcname, MWMAXSVCNAME);
   
      cmsg.hops = hops;
      cmsg.cltid = CLTID(callerid);
      cmsg.srvid = SRVID(); // TODO
      cmsg.callerid = callerid;
      cmsg.svcid = ;
      _mw_putbuffer_to_call(&cmsg, data, datlen);

      cmsg.appreturncode = appretcode;
      cmsg.returncode = returncode;

      ipc_sendmessage(callerid, (void *) &cmsg, sizeof(Call));
   };
   if (freeit) {
      storeLockCall();
      freeCall(pcall);
      storeUnLockCall();   
   };
};

void storeNewIPCCall(Call * cmsg, SRBhandle_t handle)
{
   
#ifdef MODULETEST
main()
{
   int rc;
   Call cmsg;

   mwopenlog("mtest", "./test", MWLOG_DEBUG2);

   rc = add_pending_call_data("testc", 15, 0, 678, NULL , NULL, -1, 42, 0);
   Info("add rc=%d", rc);   
   rc = add_pending_call_data("testc", 15, 0, 678, NULL , NULL, -1, 43, 0);
   Info("add rc=%d", rc);   

   cmsg.returncode = 2;
   cmsg.callerid = 99;
   cmsg.appreturncode = 20;
   strcpy(cmsg.service, "testr");
   strcpy(cmsg.domainname, "DOM");
   strcpy(cmsg.instance, "I");

   rc = add_pending_reply_data(678, 42, &cmsg, 15);
   Info("add rc=%d", rc);   
   rc = add_pending_reply_data(678, 43, &cmsg, 15);
   Info("add rc=%d", rc);   

   rc = append_pending_call_data(678, 42, "Part1", 5);
   Info("add rc=%d", rc);   
   rc = append_pending_call_data(678, 43, "Part1", 5);
   Info("add rc=%d", rc);   
   rc = append_pending_call_data(678, 42, "Part2", 5);
   Info("add rc=%d", rc);   
   rc = append_pending_call_data(678, 43, "Part2", 5);
   Info("add rc=%d", rc);   
   rc = append_pending_call_data(678, 42, "Part3", 5);
   Info("add rc=%d", rc);   
   rc = append_pending_call_data(678, 43, "Part3", 5);
   Info("add rc=%d", rc);   
   rc = append_pending_call_data(678, 42, "Partn", 5);
   Info("add rc=%d", rc);   
   rc = append_pending_call_data(678, 43, "Partn", 5);
   Info("add rc=%d", rc);   


   rc = append_pending_reply_data(678, 42, "part1", 5);
   Info("add rc=%d", rc);   
   rc = append_pending_reply_data(678, 42, "part2", 5);
   Info("add rc=%d", rc);   
   rc = append_pending_reply_data(678, 42, "part3", 5);
   Info("add rc=%d", rc);   
   rc = append_pending_reply_data(678, 42, "partN", 5);
   Info("add rc=%d", rc);   

   rc = append_pending_reply_data(678, 43, "part1", 5);
   Info("add rc=%d", rc);   
   rc = append_pending_reply_data(678, 43, "part2", 5);
   Info("add rc=%d", rc);   
   rc = append_pending_reply_data(678, 43, "part3", 5);
   Info("add rc=%d", rc);   
   rc = append_pending_reply_data(678, 43, "partN", 5);
   Info("add rc=%d", rc);   

};

#endif // MODULE TEST
#endif // replaced by callstore

/***********************************************************************
 * DATABUFFERS
 ***********************************************************************/

#if 0
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
#endif



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
    PAthis->cname = NULL;
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
      PAthis->cname = NULL;
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
  int fd, rc, mwid, handle;
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

  handle = 888; mwid = 666; fd = 78; m = m1;
  map1 = urlmapdecode(m);
  printf("push call mwid=%d, handle=%d fd=%d msg=%s\n", mwid, handle, fd, m);
  storePushCall(mwid, handle, fd, map1);

  handle = 884; mwid = 664; fd = 76; m = m2;
  map1 = urlmapdecode(m);
  printf("push call mwid=%d, handle=%d fd=%d msg=%s\n", mwid, handle, fd, m);
  storePushCall(mwid, handle, fd, map1);

  handle = 887; mwid = 667; fd = 74; m = m3;
  map1 = urlmapdecode(m);
  printf("push call mwid=%d, handle=%d fd=%d msg=%s\n", mwid, handle, fd, m);
  storePushCall(mwid, handle, fd, map1);




  handle = 888;
  mwid = 666;
  rc = storePopCall(mwid,handle,&fd,&map);
  printf("pop of mwid=%d handle=%d rc=%d fd=%d message=%s\n", mwid, handle, rc, 
	 fd, urlmapencode(map)); 

  handle = 888;
  mwid = 666;
  rc = storePopCall(mwid,handle,&fd,&map);
  printf("pop of mwid=%d handle=%d rc=%d fd=%d message=%s\n", mwid, handle, rc, 
	 fd, urlmapencode(map)); 

  handle = 884;
  mwid = 664;
  rc = storePopCall(mwid,handle,&fd,&map);
  printf("pop of mwid=%d handle=%d rc=%d fd=%d message=%s\n", mwid, handle, rc, 
	 fd, urlmapencode(map)); 

  handle = 887;
  mwid = 667;
  rc = storePopCall(mwid,handle,&fd,&map);
  printf("pop of mwid=%d handle=%d rc=%d fd=%d message=%s\n", mwid, handle, rc, 
	 fd, urlmapencode(map)); 

  handle = 883;
  mwid = 1;
  rc = storePopCall(mwid,handle,&fd,&map);
  printf("pop of mwid=%d handle=%d rc=%d fd=%d message=%s\n", mwid, handle, rc, 
	 fd, urlmapencode(map)); 

  handle = 883;
  mwid = 663;
  rc = storePopCall(mwid,handle,&fd,&map);
  printf("pop of mwid=%d handle=%d rc=%d fd=%d message=%s\n", mwid, handle, rc, 
	 fd, urlmapencode(map)); 

  handle = 1;
  mwid = 1;
  rc = storePopCall(mwid,handle,&fd,&map);
  printf("pop of mwid=%d handle=%d rc=%d fd=%d message=%s\n", mwid, handle, rc, 
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
