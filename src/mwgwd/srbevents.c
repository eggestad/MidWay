/*
  MidWay
  Copyright (C) 2000,2001,2002 Terje Eggestad

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

/*
 * $Log$
 * Revision 1.3  2004/11/17 20:55:47  eggestad
 * - large data buffer
 * - protocol fix up in event messages
 *
 * Revision 1.2  2004/04/12 23:05:25  eggestad
 * debug format fixes (wrong format string and missing args)
 *
 * Revision 1.1  2004/03/20 18:57:47  eggestad
 * - Added events for SRB clients and proppagation via the gateways
 * - added a mwevent client for sending and subscribing/watching events
 * - fix some residial bugs for new mwfetch() api
 *
 */

#include <sys/types.h>
#include <regex.h>
#include <fnmatch.h>
#include <errno.h>

#include <MidWay.h>
#include <ipcmessages.h>
#include <SRBprotocol.h>
#include <shmalloc.h>
#include <ipctables.h>
#include "gateway.h"

static char * RCSId UNUSED = "$Id$";
static char * RCSName UNUSED = "$Name$"; /* CVS TAG */


/* handling of events for SRB clients and peers. Client must subscribe
   to events, however for efficiency, especially since it's necessary
   for sending events to peers anyway, the gateways subscribe to everything!

   Therefore clients subscriptions ends here. Here we keep track of
   which subscriptions been made, and dispatch. The good thing is that
   we only have to encode the SRB message once. 
*/

struct srbsubscription {
   char * pattern;
   regex_t regexp;
   int flags;
   int subscriptionid;
   Connection * conn;
   struct srbsubscription * next;
};

DECLAREMUTEX(srbevent_mutex);

#define FLAGSMASK (MWEVSTRING|MWEVGLOB|MWEVREGEXP|MWEVEREGEXP)

static struct srbsubscription * srbsubs_head = NULL, * srbsubs_tail = NULL;

static struct srbsubscription * findsub(Connection * conn, int id)
{
   struct srbsubscription * p;

   p = srbsubs_head;
   while(p) {
      if ( (conn == p->conn) && 
	   (id == p->subscriptionid))
	 return p;
      p = p->next;
   };
   return p;
};


static void dump_subscription_table(void)
{
   struct srbsubscription * p;
   DEBUG("     SUBSCRIPTION TABLE\n");
   DEBUG("---------------- HEAD = %p TAIL = %p\n", srbsubs_head, srbsubs_tail);

   p = srbsubs_head;
   while(p) {
      DEBUG(" %p: %s %d \'%s\' %d", p, p->conn->peeraddr_string, p->subscriptionid, 
	    p->pattern, p->flags);
      p = p->next;
   };

   DEBUG("=====================================================================");
   return;
};
   
int srb_subscribe(Connection * conn, char * pattern, int id, int flags)
{
   struct srbsubscription * sub;
   int rc;

   flags &= FLAGSMASK;

   DEBUG("adding subscription for conn %p for pattern=%s flags=%x", conn, pattern, flags);
   
   LOCKMUTEX(srbevent_mutex);

   sub = findsub(conn, id);
   if (sub) {
      Warning ("GOT DUPLICATE subscription from %s id=%d", conn->peeraddr_string, id);
      rc = -EEXIST;
      goto out;
   };

   DEBUG("New subscription for %s %x", pattern, flags);
   sub = malloc(sizeof(struct srbsubscription));

   dump_subscription_table();
   
   sub->pattern = strdup(pattern);
   sub->subscriptionid = id;
   sub->flags = flags;
   sub->conn = conn;

   sub->next = NULL;
   if (srbsubs_tail) {
      srbsubs_tail->next = sub;
   } else {
      srbsubs_head = sub;
   };
   srbsubs_tail = sub;
   
   DEBUG("adding subscription ok");
   
   if (flags & MWEVEREGEXP)
      rc =  regcomp(&sub->regexp, pattern, REG_NOSUB|REG_EXTENDED);
   else if (flags & MWEVREGEXP)
      rc =  regcomp(&sub->regexp, pattern, REG_NOSUB);

   dump_subscription_table();
 out:
   UNLOCKMUTEX(srbevent_mutex);
   return rc;
};

static void remove_conn_from_sub(struct srbsubscription * p)
{
   struct srbsubscription * rp;

   DEBUG("start");

   if (srbsubs_head == p) {
      srbsubs_head = p->next;
      if (srbsubs_tail == p) srbsubs_tail = srbsubs_head;	 
      goto out;
   };

   rp = srbsubs_head;
   while(rp) {
      if (rp->next == p) {

	 rp->next = p->next;
	 if (srbsubs_tail == p) srbsubs_tail = rp;	 
	 break;
      };
      rp = rp->next;
   } 

 out:
   DEBUG("removing subscription");
   if (p->flags & (MWEVEREGEXP| MWEVREGEXP) )
      regfree(&p->regexp);
   free(p->pattern);
   free(p);
   return;
};

int srb_unsubscribe(Connection * conn, int id)
{
   struct srbsubscription * p;

   DEBUG("deleting subscription for conn %s for id=%d", conn->peeraddr_string, id);

   LOCKMUTEX(srbevent_mutex);
   dump_subscription_table();

   p = findsub(conn, id);
   if (p) {
      remove_conn_from_sub(p);
   };

   dump_subscription_table();
   UNLOCKMUTEX(srbevent_mutex);
   DEBUG("done");
   return 0;
};

void srb_unsubscribe_all(Connection * conn)
{
   struct srbsubscription * np, *p;

   DEBUG("deleting all subscription for conn %s ", conn->peeraddr_string);
   LOCKMUTEX(srbevent_mutex);
   dump_subscription_table();

   p = srbsubs_head;
   while(p) {
      np = p->next;
      if (p->conn == conn) 
	 remove_conn_from_sub(p);
      p = np;
   };

   dump_subscription_table();
   UNLOCKMUTEX(srbevent_mutex);
   DEBUG("done");
   return;
};

static inline int checkmatch(char * name, struct srbsubscription * p) 
{
   int rc;

   if (p->flags & MWEVGLOB) {
      DEBUG2("glob");
      rc = fnmatch(p->pattern, name, FNM_PERIOD);
   } else if (p->flags & (MWEVREGEXP|MWEVEREGEXP)) {
      DEBUG2("regexp");
      rc = regexec(&p->regexp, name, 0, NULL, 0);
   } else {
      DEBUG2("string");
      rc = strcmp(p->pattern, name);
   };
   DEBUG2("rc=%d", rc);
   return rc;
};
   
void do_srb_event_dispatch(Event * ev)
{
   SRBmessage srbmsg = { 0 };
   struct srbsubscription * p;
   int rc;

   DEBUG("start with event %s", ev->event);
   
   strncpy(srbmsg.command, SRB_EVENT, MWMAXSVCNAME);
   srbmsg.marker = SRB_NOTIFICATIONMARKER;    
   
   _mw_srb_setfield(&srbmsg, SRB_NAME, ev->event);
   
   if (ev->datalen > 0) {
      _mw_srb_nsetfield(&srbmsg, SRB_DATA, 
			_mwoffset2adr(ev->data, _mw_getsegment_byid(ev->datasegmentid)), 
			ev->datalen);
   };
   
   // first we send events to subscribers. That's clients, and "in the
   // future" remote domains.
   LOCKMUTEX(srbevent_mutex);
   dump_subscription_table();
   
   p = srbsubs_head;
   while(p) {
      DEBUG("checking match in %s",  p->pattern);
      rc = checkmatch(ev->event, p);
      DEBUG("match = %d", rc);
      if (rc == 0) {
	 _mw_srb_setfieldi(&srbmsg, SRB_SUBSCRIPTIONID, ev->subscriptionid);
	 rc = _mw_srbsendmessage(p->conn, &srbmsg);
	 DEBUG ("sent event to %s rc=%d", p->conn->peeraddr_string, rc);
      };
      p = p->next;      
   };

   UNLOCKMUTEX(srbevent_mutex);


   // now to the peers in our domain, provided that the event was
   // generated in this instance.
   if  ( !(ev->flags & MWEVENTPEERGENERATED)) {

      // the peers need two more fields, but no subscription id

      _mw_srb_delfield(&srbmsg, SRB_SUBSCRIPTIONID);
   
      if (ev->username[0]) {     
	 _mw_srb_setfield(&srbmsg, SRB_USER, ev->username);
      };
      if (ev->clientname[0]) {     
	 _mw_srb_setfield(&srbmsg, SRB_CLIENTNAME, ev->clientname);
      };      

      DEBUG("local genereated event, distributing to peers");
      gw_send_to_peers(&srbmsg);
   };

   if (ev->datalen > 0) {
      DEBUG("sending event ack");
      ev->mtype = EVENTACK;
      ev->senderid = _mw_get_my_gatewayid();
      _mw_ipc_putmessage(0, (char *)ev, sizeof(Event), 0);
   };

   return;
};


