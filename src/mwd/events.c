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
 * Revision 1.4  2003/04/25 13:03:05  eggestad
 * - fix for new task API
 * - new shutdown procedure, now using a task
 *
 * Revision 1.3  2002/09/22 23:01:16  eggestad
 * fixup policy on *ID's. All ids has the mask bit set, and purified the consept of index (new macros) that has the mask bit cleared.
 *
 * Revision 1.2  2002/09/04 07:13:31  eggestad
 * mwd now sends an event on service (un)provide
 *
 * Revision 1.1  2002/08/09 20:50:16  eggestad
 * A Major update for implemetation of events and Task API
 *
 */


#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <regex.h>
#include <fnmatch.h>

#include <MidWay.h>
#include <ipcmessages.h>
#include <ipctables.h>
#include <shmalloc.h>
#include "events.h"


static char * RCSId UNUSED = "$Id$";
static char * RCSName UNUSED = "$Name$"; /* CVS TAG */


struct _subscribe_event_t {
  char * pattern;
  regex_t regexp;
  int subscriptionid;
  MWID id;
  CLIENTID cid;
  SERVERID sid;
  GATEWAYID gwid;

  int flags;
  struct _subscribe_event_t * next;
};

typedef struct _subscribe_event_t subscribe_event_t;

struct _queued_event_t {
  char * eventname;

  Event evmsg;
  
  int * pending_ack_ids;
  int  pending_ack_len, pending_acks;
  
  struct _queued_event_t  * next;
};

typedef struct _queued_event_t  queued_event_t ;

static subscribe_event_t *  subscription_root = NULL;

/* when we receive an event we first just place it on a queue. We want
   to keep out IPC msg queue empty */
static queued_event_t * eventqueue_root = NULL, **eventqueue_tailp  = & eventqueue_root;
static int eventqueuelen = 0;

/* queue where we store events that has been propagagted, but we await acks from recipients */ 
static queued_event_t * ackqueue_root = NULL;
static int ackqueuelen = 0;

static void dumpsubscriptions(void)
{
  subscribe_event_t * se;

  DEBUG2("*******************************  subscriptions *********************************");

  for (se = subscription_root; se != NULL; se = se->next)
    DEBUG2(" - @ %p %s %#x [clt %d ser %d gw %d] subid %d glob=%s regex=%s eregex=%s",
	   se, se->pattern, se->id, se->cid, se->sid, se->gwid, se->subscriptionid,
	   se->flags & MWEVGLOB?"yes":"no", 
	   se->flags & MWEVREGEXP?"yes":"no", 
	   se->flags & MWEVEREGEXP?"yes":"no");
  DEBUG2("********************************************************************************");
};


static void dumpeventqueue(void)
{
  int i;
  queued_event_t * ev;

  if (eventqueuelen == 0) {
    DEBUG2("event queue empty eventqueue_tailp = %p *eventqueue_tailp = %p eventqueue_root = %p ",
	   eventqueue_tailp, *eventqueue_tailp, eventqueue_root); 
    return;
  };

  DEBUG2( "********************* event queue dump queuelen = %d ***********************\n", 
		    eventqueuelen);
  ev = eventqueue_root;
  for (i = 0; i < eventqueuelen; i++)
    DEBUG2("   - %5d: @ %p %s [%s sender %#x subid=%d data=%d datalen=%d] next %p\n", 
		      i, ev, ev->eventname, 
		      ev->evmsg.event, ev->evmsg.senderid, ev->evmsg.subscriptionid, ev->evmsg.data, ev->evmsg.datalen,
		      ev->next
		      );
  
  DEBUG2("   = eventqueue_tailp = %p *eventqueue_tailp = %p eventqueue_root = %p\n", 
		    eventqueue_tailp, *eventqueue_tailp, eventqueue_root);
  DEBUG2("****************************************************************************");
};

static void dumppendingqueue(void)
{
  char * buffer = NULL, * buff;
  int i, j;
  queued_event_t * ev;

  if (ackqueuelen == 0) {
    DEBUG3("ack queue empty ackqueue_root = %p ", ackqueue_root); 
    return;
  };

  DEBUG2("************************** ack queue dump queuelen = %d *******************************", 
	 ackqueuelen);
  ev = ackqueue_root;
  for (i = 0; i < eventqueuelen; i++) {
    DEBUG2(" - %5d: @%p %s [%s sender %#x subid=%d data=%d datalen=%d] next %p\n", 
	   i, ev, ev->eventname, 
	   ev->evmsg.event, ev->evmsg.senderid, ev->evmsg.subscriptionid, ev->evmsg.data, ev->evmsg.datalen,
	   ev->next);
    buffer = realloc (buffer, 12*ev->pending_ack_len);
    buff = buffer;
    for (j = 0; j < ev->pending_ack_len; j++)
      buff += sprintf(buff, "%#x ", ev->pending_ack_ids[j]);
    DEBUG2("   - pending acks [ %s]", buffer);
  };
  DEBUG2(" = eventqueue_root = %p\n", ackqueue_root);
  free(buffer);
  DEBUG2("***************************************************************************************");
};



/* called when we get a subscribe message from a member */
int event_subscribe(char * pattern, MWID id, int subid, int flags)
{
  int rc;
  subscribe_event_t * se;
  
  if (pattern == NULL) return -EINVAL;

  se = malloc(sizeof(subscribe_event_t));
  se->id = id;
  se->subscriptionid = subid;

  se->cid = CLTID(id); 
  se->sid = SRVID(id);
  se->gwid = GWID(id); 
  
  DEBUG("Subscribing to event \"%s\" clientid %d serverid %d gatewayid %d", 
	pattern, se->cid, se->sid, se->gwid);

  
  se->flags = flags;
  se->pattern = strdup(pattern);
  
  rc = 0;
  
  if (flags & MWEVEREGEXP)
    rc =  regcomp(&se->regexp, pattern, REG_NOSUB|REG_EXTENDED);
  else if (flags & MWEVREGEXP)
    rc =  regcomp(&se->regexp, pattern, REG_NOSUB);

  if (rc != 0) {
    char buffer[1024];
    regerror(rc, &se->regexp, buffer, 1023);
    Warning("erroneous regexp %s in subscribe from MWID %#x", buffer, id);
    regfree(&se->regexp);
    free (se->pattern);
    free(se);
    return -EINVAL;
  };

  se->next = subscription_root;
  subscription_root = se;

  dumpsubscriptions();

  DEBUG("subscription OK");
  return 0;
};

/* called when we get an unsubscribe message from a member, or
   internally if we're clearing up after a die/detached member that
   didnæt unsubscribe. In the later case subid = -1  */
int event_unsubscribe(int subid, MWID id)
{
  subscribe_event_t * se, ** pse;

  if (subid < UNASSIGNED) return -EINVAL;

  DEBUG("unsubscribe subid %d mwid %#x", subid, id);

  for (pse = &subscription_root; *pse != NULL; pse = &((*pse)->next)) {
    se = *pse;
    DEBUG("  comparing to \"%s\" subid %d mwid %#x", se->pattern, se->subscriptionid,  se->id);

    if (se->id == id) {
      // if called with a real subscriptionid we also match it,
      // otherwise this is a post mortem cleanup
      if ((subid != UNASSIGNED) && (se->subscriptionid != subid)) continue;
      DEBUG("  match");
      if (se->flags & (MWEVREGEXP|MWEVEREGEXP))
	regfree(&se->regexp);
      free(se->pattern);
      free(se);

      *pse = se->next;
      if (se->next == NULL) break;
      
      // if this was called with sub != -1 it was a proper unsubscribe
      // req, and there can be only one subid/mwid tuple, no need to
      // search further
      if (subid != UNASSIGNED) break; 
    };

    dumpsubscriptions();
  };
  
  dumpsubscriptions();

  return 0;
};


/* we need to do something about makeing sure event id's are
   unique. Gateways may assign event id's for foreign recv events. */
static int last_eventid = 0x0;
static int next_eventid(void)
{

  if (last_eventid > 100000000)
    last_eventid = 0;
  return ++last_eventid;
};

static int clear_pendingack_byid(MWID id)
{
  queued_event_t * ev, **pev ;
  int i;

  for (pev = &ackqueue_root; *pev != NULL; pev = & (*pev)->next) {
    ev = *pev;
    
    for (i = 0; i < ev->pending_ack_len; i++) {
      if (ev->pending_ack_ids[i] != id) 
	continue;
      ev->pending_ack_ids[i] = UNASSIGNED; 
      ev->pending_acks--;

      DEBUG("found the pending event ID entry pending acks is now %d", ev->pending_acks);
      /* if the last pwnding ack, dequeue and free shmbuffer */
      if (ev->pending_acks == 0) {
	DEBUG("last panding ack, deleting");
	_mwfree( _mwoffset2adr(ev->evmsg.data));
	
	free(ev->pending_ack_ids);
	*pev = ev->next;
	ackqueuelen--;
	DEBUG("dequeued from ackqueue");	  
	free (ev);
      };
      dumppendingqueue();
      break; // the pending acl list of MWID is unique, each recipient was gotten the event only once
    };
  };
  return 0;
};

/* called by mwd on a members exit. */
void event_clear_id(MWID id)
{
  DEBUG("cleaning up after %#x", id);
  clear_pendingack_byid(id);
  event_unsubscribe(UNASSIGNED, id);
  return;
};


/* called when a client/member acks an event. When all pending acks
   are recv, we remove the event from the ack pending queue, and
   finnaly delete it. */ 
int event_ack(Event * evmsg) 
{
  queued_event_t * ev, **pev ;
  int i;

  if (evmsg == NULL) return -EINVAL;

  DEBUG("Got an ack from %x for event %d", evmsg->senderid, evmsg->eventid);
  for (pev = &ackqueue_root; *pev != NULL; pev = & (*pev)->next) {
    ev = *pev;
    
    if (evmsg->eventid != ev->evmsg.eventid) 
      continue;
    DEBUG("found the pending event");
    for (i = 0; i < ev->pending_ack_len; i++) {
      if (ev->pending_ack_ids[i] != evmsg->senderid) 
	continue;
      ev->pending_ack_ids[i] = UNASSIGNED; 
      ev->pending_acks--;

      DEBUG("found the pending event ID entry pending acks is now %d", ev->pending_acks);
      /* if the last pwnding ack, dequeue and free shmbuffer */
      if (ev->pending_acks == 0) {
	DEBUG("last panding ack, deleting");
	_mwfree( _mwoffset2adr(ev->evmsg.data));
	
	free(ev->pending_ack_ids);
	*pev = ev->next;
	ackqueuelen--;
	DEBUG("dequeued from ackqueue");	  
	free (ev);
      };
      dumppendingqueue();
      break; // the pending acl list of MWID is unique, each recipient was gotten the event only once
    };
    break; /* there can be only one match for eventid */
  };
  return 0;
};

/* this is used for event generated by mwd */
int internal_event_enqueue(char * event, void * data, int datalen, char * user, char * client)
{
  Event evmsg;
  char * dbuf;

  if (event == NULL) return -EINVAL;

  evmsg.mtype = EVENT;
  strncpy(evmsg.event, event, MWMAXSVCNAME);
  if (data != NULL) {
    if (datalen == 0) datalen = strlen(data);

    dbuf = _mwalloc(datalen+1);
    memcpy(dbuf, data, datalen+1);
    /* just for safety we make sure there is a trailing \0 */
    dbuf[datalen] = '\0';
    
    evmsg.data = _mwadr2offset(dbuf);
    evmsg.datalen = datalen;
  } else {
    evmsg.data = 0;
    evmsg.datalen = 0;
  };

  DEBUG("queueing event %s with %d bytes data user=%s client=%s", 
	event, evmsg.datalen, user?user:"", client?client:"");

  evmsg.senderid = 0;
  evmsg.eventid = UNASSIGNED;
  if (user != NULL) {
    strncpy(evmsg.username, user, MWMAXSVCNAME);
  } else {
    evmsg.username[0] = '\0';
  };

  if (client != NULL) {
    strncpy(evmsg.clientname, client, MWMAXSVCNAME);
  } else {
    evmsg.clientname[0] = '\0';
  };
  return event_enqueue(&evmsg); 
}

/* called when we get a event message from a client to distribute. We
   place it on the a internal queue until a tasklet processes it. evmsg is copied.
*/
int event_enqueue(Event * evmsg)
{
  queued_event_t * ev;
  
  if (evmsg == NULL) return -EINVAL;
  
  ev  = malloc(sizeof(queued_event_t));
  
  memcpy(&ev->evmsg, evmsg, sizeof(Event));

  ev->eventname = ev->evmsg.event;

  DEBUG("queued event \"%s\" senderid %#x eventid=%d", evmsg->event, evmsg->senderid, evmsg->eventid);
  ev->pending_ack_len = 0;
  ev->pending_acks = 0;
  ev->pending_ack_ids = NULL;

  if (ev->evmsg.eventid == UNASSIGNED) 
    ev->evmsg.eventid = next_eventid();
  
  *eventqueue_tailp = ev;
  eventqueue_tailp = &ev->next;
  eventqueuelen ++;
  DEBUG("eventqueuelen = %d", eventqueuelen);

  dumpeventqueue();  
  return 0;
};
  
/* this is called by do_event() and take one queue event form the
   internal queue and send it to all members that has subscribed to
   it. The event is than placed on the ack pending queue until all
   recipients has acked the event. 
*/
static int do_event(void)
{
  queued_event_t * ev;
  subscribe_event_t * se;
  cliententry * ce;
  int n = 0, rc;

  if (eventqueue_root == NULL) {
    eventqueuelen = 0;
    return -ENOENT;
  };
  
  /* dequeue an event */
  ev = eventqueue_root; 
  eventqueue_root = ev->next;
  eventqueuelen--;
  if (eventqueuelen == 0) eventqueue_tailp = &eventqueue_root;

  DEBUG("dequeued %s from eventqueue senderid %#x eventid %d", 
	ev->eventname, 
	ev->evmsg.senderid, 
	ev->evmsg.eventid);

  dumpeventqueue();

  ev->evmsg.senderid = 0;

  for (se = subscription_root; se != NULL; se = se->next) {
    
    DEBUG2("Checking to se if event %s matches %s cid=%d sid=%d gwid=%d", 
	   ev->eventname, se->pattern, 
	   se->cid, 
	   se->sid, 
	   se->gwid);

    if (se->cid != UNASSIGNED) {
      ce = _mw_get_client_byid(se->cid);
      if (ce == NULL) continue;      
      DEBUG2("checking username %s ?= %s and clientname %s ?= %s", 
	     ev->evmsg.username, ce->username,
	     ev->evmsg.clientname, ce->clientname);
      
      if (ev->evmsg.username[0] != '\0')
	if (strcmp(ev->evmsg.username, ce->username) != 0) continue;
      if (ev->evmsg.clientname[0] != '\0')
	if (strcmp(ev->evmsg.clientname, ce->clientname) != 0) continue;
      DEBUG2("nomatch");
    };

    
    if (se->flags & MWEVGLOB) {
      DEBUG2("glob");
      rc = fnmatch(se->pattern, ev->eventname, FNM_PERIOD);
    } else if (se->flags & (MWEVREGEXP|MWEVEREGEXP)) {
      DEBUG2("regexp");
      rc = regexec(&se->regexp, ev->eventname, 0, NULL, 0);
    } else {
      DEBUG2("string");
      rc = strcmp(se->pattern, ev->eventname);
    };
    
    if (rc != 0) {
      DEBUG2("no pattern match");
      continue;
    };

    DEBUG2("event subscription match");

    ev->evmsg.subscriptionid = se->subscriptionid;

    rc = _mw_ipc_putmessage(se->id, (char *) &ev->evmsg, sizeof (Event), 0); 
    n++;

    /* iff there is a data buffer attached to the event, then we need
       to pass it on to the clients, but'is our resposibility to free
       it. Thus the recipients must ack the event. */

    DEBUG2("ev->evmsg.data = %d, rc = %d", ev->evmsg.data, rc);
    if (ev->evmsg.data != 0) {
      if (rc == 0) {
	ev->pending_ack_len++;
	ev->pending_ack_ids = realloc(ev->pending_ack_ids, sizeof(int) * ev->pending_ack_len);
	DEBUG2("ev->pending_ack_ids[ev->pending_acks = %d] = %d", 
	       ev->pending_acks,  ev->pending_ack_ids[ev->pending_acks]);
	ev->pending_ack_ids[ev->pending_acks] = se->id;
	ev->pending_acks++;
      };
    };
  };

  DEBUG("We sent event to %d recipients", n);
  if (ev->pending_acks) {
    ev->next = ackqueue_root;
    ackqueue_root = ev;
    ackqueuelen ++;
    DEBUG("queued in ackqueue with %d pending acks; ackqueuelen = %d", ev->pending_acks, ackqueuelen);
    dumppendingqueue();
  } else {
    // free up if there was no 
    if (ev->evmsg.data > 0) _mwfree( _mwoffset2adr(ev->evmsg.data));
    free (ev);
  };
  
  return 0;
};

/* THis is called as a tasklet. we only do events for 10ms before
   returning the remaining queuelen. THis is to ensure thay the mwd is
   not hung by an event storm. */
int do_events(PTask pt)
{
  clock_t clk;
  int rc;

  DEBUG(" start with queuelen %d", eventqueuelen);
  clk = clock();
  
  do {
    if (eventqueuelen == 0) break;
    rc = do_event();
    if (rc == -ENOENT) {
      break;
    };
  } while((clock() - clk) < (CLOCKS_PER_SEC/100) );

  DEBUG(" return %d", eventqueuelen);
  return eventqueuelen;
};
