/*
  MidWay
  Copyright (C) 2005 Terje Eggestad

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

/** @file
 The purpose of this module is to make an "Object" that stores call
 state. 

 The call is referenced here while request has been sent to the
 outside or inside. This message needs to be retrived when the reply
 comes in order to send a complete reply back.  espesially since the
 request may hold fields we ignore, that shall be sent back.

 We now added the store of a Call pending data from SRB, this mean
 that I've moved the IPC acall in here.  Calls are kept in the store
 if data is pending or replies are pending. Call are now stored unless
 there are no additional data SRC messages AND noreply flag is set.

 The PendingCall is inserted into the hash with caller(client)
 Connection mwid and handle while we're getting the call request
 data. After the called (server) mwid and handle is used. The
 PendingCall is reinserted when the call is complete and routed onto
 SRB.

 The tuple (mwid + srbhandle) is always unique. The handles are
 unique for every mwid. The IPC handle may have collision with SRB
 handles, and the IPC originated IPC call must therefore me assigned a
 SRBHandle unique to the mwid ownin the Connection we're routing the
 service call to.

 There are only five primary public operations that handle the
 following events:
 
 - a SRCCALL request from the client arrived
 - a SRCCALL reply from the server arrived
 - a SRCDATA related to the last SRBCALL (req or reply) arrived
 - a IPC call request
 - a IPC call reply

 */

#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include <MidWay.h> 
#include <urlencode.h>
#include "callstore.h"
#include <ipcmessages.h>
#include <ipctables.h>
#include <SRBprotocol.h>
#include <shmalloc.h>
#include "ipcserver.h"


/*
   mutex for this module, the public functions here \e must be thread safe

   All the public functions shall lock on entry

   All the public functions shall lock on entry
*/
DECLAREMUTEX(callmutex);

static void storeLockCall(void)
{
   LOCKMUTEX(callmutex);
};

static void storeUnLockCall(void)
{
   UNLOCKMUTEX(callmutex);
};

/** 
    enable or disable workhole routing.
    If not workhole, the completed
    call/reply is recv'ed before passing on, else each SRBCALL/SRBDATA
    is routed directly on the connections. Non workhold is mandatory
    if either client or server are on IPC.
*/
//static int wormhole = 0;

//* Hash size, currenly hard coded, and shall be 2^N
#define HASHSIZE 256 

/** 
 Make hash from tuple(mwid, handle).

 To make a fast lookup of PendingCalls we have a hash, this make the key
*/
static unsigned char mkhash(MWID mwid, SRBhandle_t handle)
{
   unsigned char h;
   unsigned char a,b,c,d;
   
   DEBUG2("Hash of mwid %s and handle %#x", _mwid2str(mwid, NULL), handle);

   a = handle & 0xff;
   b = (handle >> 8) & 0xff;

   mwid <<= 5;
   c = (handle >> 16) & 0xff;
   d = (handle >> 24) & 0xff;

   mwid &= 0xff;

   h = ((a ^ b) ^ (c ^ d)) ^ mwid;

   DEBUG2("Hash = %hhu", h);   
   return h;
};


enum { FREE = 0, ALLOCED, CALLDATAPENDING, REPLYPENDING, REPLYDATAPENDING };

/**
   the structure that hold all need info on a pending call
 */

struct PendingCall {
   MWID src_mwid;      ///< citid or gwid
   Connection * srcconn; ///< if SRB client, a pointer to the connection 
   
   /** A Call struct in the case this call is routed tp IPC (if not,
       wormhole) or the original IPC message */
   Call call; 

   urlmap * callmsg;   ///< the original call srb
   urlmap * replymsg;  ///< the reply message while we wait for 

   MWID dst_mwid;      ///< a service id if dest is IPC, a gwid if dest is SRC
   Connection * dstconn; ///< if SRB server, a pointer to the connection 

   int status;         ///< The status either CALLDATAPENDING, REPLYPENDING, REPLYDATAPENDING

   char * data;        ///< IPC buffer for data still incomming
   int datalen;        ///< call data lenght
   int datarecv;       ///<  call data received
   
   mwhandle_t ipchdl;  ///< The IPC handle for IPC originated call 
   SRBhandle_t srchdl, dsthdl; ///< the SRB Handles
   
   struct PendingCall * next, **root; ///< double linked list 
};

//* the hash of all active pending call
static struct PendingCall * hash[HASHSIZE] = { NULL };
//* we cache used structs
static struct PendingCall * freelist = NULL;

static void debugPC(char * msg, struct PendingCall * pc) {  
   char src[64], dst[64];
   _mwid2str(pc->src_mwid, src);
   _mwid2str(pc->dst_mwid, dst);
   DEBUG("%s PendingCall src: id=%s hdl=%#x dst: id=%s hdl=%#x  sts=%d", 
	 msg, src, pc->srchdl, dst, pc->dsthdl, pc->status);
};

/* we save the very last PendingCall we looked up, it'e most probably
   the once we're goint to lookup next */
static struct PendingCall * lastpc = NULL;
   
/** lookup from dst tuple used for messages from server */
static struct PendingCall * lookupPendingCall(MWID mwid,  SRBhandle_t hdl)
{
   struct PendingCall * pc;

   pc = hash[mkhash(mwid, hdl)];
   DEBUG("lookup for tuple %s %x", _mwid2str(mwid, NULL), hdl);

   while(pc != NULL) {
      /* the tuple srbhandle + mwid must always be unique */
      debugPC("checking", pc);
      if ( ((pc->dst_mwid == mwid) && (pc->dsthdl == hdl)) 
	   || ((pc->src_mwid == mwid) && (pc->srchdl == hdl)) ) {
	 debugPC("Found", pc);
	 return pc;
      };
      pc = pc->next;
   };
   DEBUG("Not Found");
   return NULL;
};

#if 0
/** lookup from src tuple used for data messages from client*/
static struct PendingCall * lookupPendingCallSRC(MWID mwid,  SRBhandle_t hdl)
{
   struct PendingCall * pc;
   int i;

   DEBUG("idid = %s HDL %#x", _mwid2str(meid, NULL), hdl);
   if ( (lastpc->src_mwid == mwid) && (lastpc->srchdl == hdl) ) {
      DEBUG("match on last");
      return lastpc;
   };
   
   for (i = 0; i < HASHSIZE; i++) {

      pc = hash[i];
      DEBUG("lookup for SRC  %s %x", _mwid2str(mwid, NULL), hdl);
      
      while(pc != NULL) {
	 if ( (pc->src_mwid == mwid) && (pc->srchdl == hdl) ) {
	    debugPC("Found", pc);
	    return pc;
	 };
	 pc = pc->next;
      };
   };

   return NULL;
};
#endif
/** clear a PendingCall */
static void clearPendingCall(struct PendingCall * pc)
{
   memset(pc, 0, sizeof(struct PendingCall));
};

//@{
/**
   Link a PendingCall into a linked list.

   we got a slighly unusal double linked list, each PC have a root
   pointer that point to the pointer that refs it.
   
   Always use link/unlink when changing it's place, a PendingCall struct \e must
*/
static void linkPendingCall(struct PendingCall * pc, struct PendingCall ** ppc) 
{
   struct PendingCall * npc;
   
   npc = *ppc;
   if (npc != NULL) 
      npc->root = &pc->next;
   pc->root = ppc;
   pc->next = npc;
   *ppc = pc;

   debugPC("LINK", pc);
   return;
};

/**
   Link a PendingCall into a linked list. @see linkPendingCall()
*/
static void unlinkPendingCall(struct PendingCall * pc) 
{
   debugPC("UNLINK", pc);
   *(pc->root) = pc->next;

   if (pc->next != NULL) 
      pc->next->root = pc->root;

   pc->root = NULL;
   pc->next = NULL;

   return;
};

//@}

/**
   reinsert with new tuple.
   
   This is done after all the data is arrived and we changed from src to dest tuple
*/

static void rehashPendingCall(struct PendingCall * pc, MWID mwid, SRBhandle_t hdl)
{   
   unlinkPendingCall(pc);   
   linkPendingCall(pc, &hash[mkhash(mwid, hdl)]);
};

   
/**
   allocate a PendingCall.
    
   Get a free or new PendingCall and insert it into the hash. 
   
   Note that for IPC servers, the src tuple is used
   
   Note that state is equal to ALLOCED, santy checking may/shall
   check that no structs are in ALLOCED when public functions retrun.
*/ 
static struct PendingCall * newPendingCall(MWID srcid, SRBhandle_t srchdl) 
{
   struct PendingCall * pc;
   int hashidx = mkhash(srcid, srchdl);

   DEBUG("srcid = %s HDL %#x", _mwid2str(srcid, NULL), srchdl);
   if (freelist == NULL) {
      pc = malloc(sizeof(struct PendingCall));
      DEBUG("alloced a new PC");
      clearPendingCall(pc);
   } else {   
      pc = freelist;
      unlinkPendingCall(pc);
   };
   pc->src_mwid = srcid;
   pc->srchdl = srchdl;
   linkPendingCall(pc, &hash[hashidx]);
	
   return pc;
};

/**
   release a PendingCall. 

   Clear it and insert it into the freelist
*/
static void freePendingCall(struct PendingCall * pc)
{
   if (pc->callmsg) urlmapfree(pc->callmsg);
   if (pc->replymsg) urlmapfree(pc->replymsg);

   unlinkPendingCall(pc);   
   clearPendingCall(pc);
   linkPendingCall(pc, &freelist);   
   return;
};
   
static void check_if_complete(struct PendingCall * pc)
{
   MWID srvid, svcid;
   int rc;

   ENTER();
   debugPC("Checking if complete", pc);

   switch(pc->status) {

      /* check if all pieces of data has arrived, if so, do a IPC call */
   case CALLDATAPENDING:

      DEBUG("checking to see if call data is complete: %d == %d : %s",
	    pc->datarecv,  pc->datalen, 
	    (pc->datarecv == pc->datalen)?"Yes":"No");
      if (pc->datarecv != pc->datalen) break;

      if(pc->datalen > 0) 
	 _mw_putbuffer_to_call(&pc->call, pc->data, pc->datalen);

      pc->call.handle = _mw_nexthandle();
      pc->status = REPLYPENDING;
            
      
      /// @todo retry on server disappearing
      svcid = _mw_get_best_service(pc->call.service, pc->call.flags);
      srvid = _mw_get_provider_by_serviceid(svcid);

      // for IPC calls we use the IPC client id and IPC handle as dst tuple.
      pc->dst_mwid = pc->call.cltid;
      pc->dsthdl = pc->call.handle;
      rehashPendingCall(pc, pc->call.cltid, pc->call.handle);

      pc->call.svcid = svcid;
      DEBUG("routing call %s to %s %s", pc->call.service, _mwid2str(srvid, NULL), _mwid2str(svcid, NULL));
      ipc_sendmessage(srvid, &pc->call, sizeof(Call));

      break;
   
   case REPLYDATAPENDING:
      DEBUG("checking to see if reply  data is complete: %d == %d : %s",
	    pc->datarecv,  pc->datalen, 
	    (pc->datarecv == pc->datalen)?"Yes":"No");

      if (pc->datarecv != pc->datalen) break;
      
      if (pc->datalen > 0)
	 _mw_putbuffer_to_call(&pc->call, pc->data, pc->datalen);

      pc->call.mtype = SVCREPLY;
      pc->call.srvid = _mw_get_my_gatewayid();
      ipc_sendmessage(pc->src_mwid, &pc->call, sizeof(pc->call));
      DEBUG("send ipc reply %d", rc);

      // reset if more replies
      if (pc->call.returncode != MWMORE) {
	 freePendingCall(pc);
      } else {
	 pc->status = REPLYPENDING;
	 pc->datarecv = 0;
	 pc->datalen = 0;
	 pc->data = NULL;
      };
      break;


   default:
      Error("pending call in wrong state (%d)for completion check.", pc->status);
      break;
   };

   
/*
  if (data == NULL) datalen = 0;
  else {
    _mw_putbuffer_to_call(&cmsg, data, datalen);
  };
*/
   LEAVE();
   return;
};

/**
   Add a SRC call request to store. 

   There is no data param here, the data must be supplied with a
   storeSRBCallData() explicitly. Will check for complete request and
   route request to either IPC or SRB services.

   @param mwid mwid of the caller 
   @param nethandle the SRBHANDLE of the message
   @param map the urlmap of the decoded SRBMessage
   @param conn the Connection for mwid (we're just passed it for speed (avoid lookup))
   @param datalen the total number of octets of data associated with this request >= 0
   @param service The service name
   @param instance originating nistance
   @param domain originating domain
   @param timeout The time in millisecs within the service call must complete
   @param flags  the IPC call flags
   @param

   @return always 0
*/ 
int storeSRBCall(MWID mwid, SRBhandle_t nethandle, urlmap *map, Connection * conn, int datatotal, 
		 char * service, char * instance, char * domain, char * clientname, int timeout, int flags )
{
   struct PendingCall * pc;

   ENTER();
   storeLockCall();
   
   DEBUG2("Pushing PendingCall id=%s, handle=%x totaldata=%d", 
	  _mwid2str(mwid, NULL), nethandle, datatotal);
   
   pc = newPendingCall(mwid, nethandle);
   pc->srcconn = conn;
   DEBUG("call on conn %s", conn->peeraddr_string);
   pc->status = CALLDATAPENDING;
   pc->callmsg = map;
   
   pc->datalen = datatotal;
   
   lastpc = pc;
   pc->call.mtype = SVCCALL;
   pc->call.cltid = mwid;
   pc->call.srvid = UNASSIGNED;
   pc->call.gwid = _mw_get_my_gatewayid();

   strncpy(pc->call.service, service, MWMAXSVCNAME);
   /// @todo   strcpy(pc->call.clientname, clientname); howto for foreign clients??
   if (instance) 
      strncpy(pc->call.instance, instance, MWMAXNAMELEN);
   if (domain) 
      strncpy(pc->call.domainname, domain, MWMAXNAMELEN);

   pc->call.flags = flags;
   check_if_complete(pc);

   storeUnLockCall();
   LEAVE();
   return 0;
};

/**
   Add  SRC call  data to a PendingCallstore. 

   The call request/reply must be in the store already. Note that if
   we're still receiving call req data, the PendingCall is inserted
   into the hash with caller(client) Connection mwid and handle, after
   the called (server) mwid and handle is used. The PendingCall is
   reinserted when the call is complete and routed onto SRB.
 
   @param mwid mwid of the owner of the Connection
   @param nethandle the SRBHANDLE of the message
   @param map the urlmap of the decoded SRBMessage
   @param data the databuffer (may be in local of shared memory)
   @param datalen the number of bytes to add
   @return always 0 or -ENOENT if the call is not in store. 
*/ 
int storeSRBData(MWID mwid, SRBhandle_t nethandle, char * data, int datalen) 
{
   struct PendingCall * pc;
   int rc = 0;

   ENTER();
   storeLockCall();
   
   DEBUG("got %d call data for %s hdl=%x", datalen, _mwid2str(mwid, NULL), nethandle);
   pc = lookupPendingCall(mwid, nethandle);
   if (pc == NULL) {
      rc = -ENOENT;
      Warning("got an unexpected call data!");
      goto out;
   }
   
   if (pc->datalen < 0) pc->datalen = datalen;

   if (pc->data == NULL) {
      pc->data = _mwalloc(pc->datalen);
   };

   // the total data must be greater or equal what we've got so far.
   // TODO: reject the massage and close connection instead of assert
   Assert(pc->datalen >= pc->datarecv + datalen);
   
   memcpy(pc->data + pc->datarecv, data, datalen);
   pc->datarecv += datalen;
   
   lastpc = pc;
   check_if_complete(pc);
 out:
   storeUnLockCall();
   LEAVE();
   return 0;
};

/**
   Add a SRC call reply to store. 

   There is no data param here, the data must be supplied with a
   storeSRBReplyData() explicitly. Will check for complete reply and
   route reply to either the IPC or SRB client.

   @param mwid mwid of the replier
   @param nethandle the SRBHANDLE of the message
   @param map the urlmap of the decoded SRBMessage
   @param datalen the total number of octets of data associated with this reply >= 0
   @return always 0
*/ 
int storeSRBReply(MWID mwid, SRBhandle_t nethandle, urlmap *map, int rcode, int apprcode, int datatotal)
{
   struct PendingCall * pc;
   int rc = 0;
   
   ENTER();
   storeLockCall();
   
   DEBUG2("looking up PendingCall id=%s, handle=%x totaldata %d", 
	  _mwid2str(mwid,NULL), nethandle, datatotal);
   
   pc = lookupPendingCall(mwid, nethandle);
   if (pc == NULL) {
      rc = -ENOENT;
      Warning("got an unexpected call reply!");
      goto out;
   }

   Assert(pc->status == REPLYPENDING);

   pc->call.returncode = rcode;
   pc->call.appreturncode = apprcode;

   pc->status = REPLYDATAPENDING;

   pc->replymsg = map;
   
   pc->datalen = datatotal;
   
   lastpc = pc;
   
   check_if_complete(pc);
   
 out:
   storeUnLockCall();
   LEAVE();
   return 0;
};

/**
   when a client closed connection we need to clear any pending calls
   the client left hanging
   
   @param mwid mwid of the client
   @return always 0
*/
int storeSRCClearClient(MWID mwid)
{
   struct PendingCall * pc, *pc_tmp;
   int rc = 0;
   
   ENTER();
   storeLockCall();

   DEBUG("clearing all PendingCall for id=%#x", mwid);

   int clearcount = 0;
   for (int i = 0; i < HASHSIZE; i++) {
      pc = hash[i];
      while(pc != NULL) {
	 pc_tmp = pc;
	 pc = pc->next;
	 
	 if (pc_tmp->src_mwid == mwid) {
	    debugPC("Found a pending call", pc_tmp);
	    freePendingCall(pc_tmp);
	 }
      }
	    
   }
   
out:
   storeUnLockCall();
   LEAVE();
   return 0;
}


/**
   Add a IPC originated service call to pending calls. 

   @param cmsg the Call message received from the client.
   @param conn the Connection that the call is routed out on. 
   @return always 0
*/
int storeIPCCall(Call * cmsg, Connection *conn)
{
   struct PendingCall * pc;
   char * data;
   size_t datalen;
   int rc;

   ENTER();
   storeLockCall();
   
   DEBUG2("Pushing IPC PendingCall id=%s, handle=%x", 
	  _mwid2str(conn->gwid, NULL), cmsg->handle);
   
   pc = newPendingCall( _mw_get_caller_mwid(cmsg), cmsg->handle);
   Assert(pc != NULL);

   pc->ipchdl = cmsg->handle;
   pc->dst_mwid = conn->gwid;
   pc->dsthdl = _mw_nexthandle();

   rehashPendingCall(pc, conn->gwid, pc->dsthdl);

   pc->dstconn = conn;

   rc = _mw_getbuffer_from_call(cmsg, &data, &datalen);
   if (rc < 0) {
      DEBUG1("_mw_getbuffer_from_call returned errno=%d", -rc);
      unlinkPendingCall(pc);
      return rc;
   };

   // _mw_srbsendcall() will split into call and data if data is over the limit
   rc = _mw_srbsendcall(conn, pc->dsthdl, cmsg->service, data, datalen, 
			cmsg->flags);

   if (data)
      mwfree(data);

   if (rc < 0) {
      DEBUG1("_mw_srbsendcall returned errno=%d", -rc);
      unlinkPendingCall(pc);
      return rc;
   };

   memcpy(&pc->call, cmsg, sizeof(Call));
   pc->status = REPLYPENDING;

   storeUnLockCall();
   LEAVE();
   return 0;
};


/** 
    Given an IPC call reply get the conn and urlmap. If the call has
    \a returncode equal to #MWMORE we leave the pending call in the
    store, else release it.


    @param cmsg the Call message received from the server
    @param ** pconn return the Connection that the call came from, may not be NULL.
    @param ** pmap return the urlmap from the original SRB message, may not be NULL. 
    @return  0, or -1 if there was no Pending Call. 
*/
int storeIPCReply(Call * cmsg, Connection ** pconn, urlmap ** pmap)
{
   struct PendingCall * pc;
   MWID mwid;
   int rc = 0;

   ENTER();
   storeLockCall();
  /* there are more replies, we get and not pop the map from the
     pending call list */
   
   mwid =  _mw_get_caller_mwid(cmsg);


   DEBUG("fetching pending call from store for %s handle = %x", _mwid2str(mwid, NULL), cmsg->handle);

   // The IPC handle on SRB originated calls are the SRB Handle
   pc = lookupPendingCall(mwid, cmsg->handle);
   if (pc == NULL) {
      Warning("got unexpected IPC call reply");
      rc = -1;
      goto out;
   };
   
   Connection * conn = pc->srcconn;
   if (conn->cid != mwid && conn->gwid != mwid) {
      Error("got reply to mwid %#x but connection had cid %#x gwid %#x, discarding",
	    mwid, conn->cid, conn->gwid );
      rc = -1;
      goto out;
   };
   
   *pconn = conn;
   if (cmsg->returncode == MWMORE) {
      *pmap = urlmapdup(pc->callmsg);
   } else {
      *pmap = pc->callmsg;
      pc->callmsg = NULL;
      freePendingCall(pc);
   };
 out:
  storeUnLockCall();
  LEAVE();
  return rc;
};
