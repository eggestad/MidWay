/*
  The MidWay API
  Copyright (C) 2005 Terje Eggestad

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
 * $Log$
 * Revision 1.1  2005/10/12 22:46:27  eggestad
 * Initial large data patch
 *
 */

#include <errno.h>

#include <MidWay.h>
#include <SRBprotocol.h>

#include "call-pending-data.h"


struct callpendingdata {
   MWID mwid;
   Call cmsg;

   SRBhandle_t srbhandle;

   int totaldata;
   int datalen;
   char * data;
   
   int callflag; 

   struct callpendingdata * next;
};

// We use a hash consisting of the lower 3 bit id mwid, and the lower 5 bit of callerid;
// it'll be in all probability unique, and should have a good enough distibution. 
// for now at least

struct callpendingdata * hash[256] = { NULL };

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


void del_pending_data(MWID mwid, SRBhandle_t handle, int  callreplyflag)
{
   unsigned char h;
   struct callpendingdata * tmp, **ptmp;

   DEBUG2("enter");
   tmp = lookup(mwid, handle, callreplyflag, &ptmp);
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

int add_pending_call_data(char * svcname, int totaldatalen, int flags,  
		      MWID mwid, char * instance, char * domain, MWID callerid, 
		      SRBhandle_t handle, int hops)
{
   struct callpendingdata * s;
   Call * call;
   DEBUG2("enter");

   s = add_pending_data(mwid, handle, CALLFLAG);
   if (!s) return -ENOMEM;

   s->cmsg.mtype = SVCCALL;
   strncpy(s->cmsg.service, svcname, strlen(svcname));
   s->totaldata = totaldatalen;
   s->cmsg.flags = flags;
   s->mwid = mwid;
   s->srbhandle = handle;

   s->cmsg.cltid = CLTID(mwid);
   s->cmsg.gwid = GWID(mwid);
   DEBUG2("mwid = %x cltid = %x gwid = %x", mwid,  s->cmsg.cltid, s->cmsg.gwid);
   
   if (instance)
      strncpy(s->cmsg.instance, instance, strlen(instance));

   if (domain) 
      strncpy(s->cmsg.domainname, domain, strlen(domain));

   s->cmsg.callerid = callerid;
   s->cmsg.hops = hops;


   s->datalen = 0;
   s->data = mwalloc(totaldatalen);

   return 0;
};

int add_pending_reply_data(MWID mwid, SRBhandle_t handle, Call * cmsg, int totallen)
{
   struct callpendingdata * s;
   
   DEBUG2("enter");
   s = add_pending_data(mwid, handle, REPLYFLAG);
   if (!s) return -ENOMEM;
   
   memcpy(&s->cmsg, cmsg, sizeof(Call));
   s->datalen = 0;
   s->data = mwalloc(totallen);

   return 0; 
};
	
static int append_pending_data(MWID mwid, SRBhandle_t handle, int callreply, char * data, int datalen, 
			       struct callpendingdata ** ps)
{
   struct callpendingdata * s;

   s = lookup(mwid, handle, callreply, NULL);
   
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

int append_pending_call_data(MWID mwid, SRBhandle_t handle, char * data, int datalen)
{
   int rc;
   struct callpendingdata * s;

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

int append_pending_reply_data(MWID mwid, SRBhandle_t handle, char * data, int datalen)
{
   int rc;
   struct callpendingdata * s;
   
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

#endif
