/*
  MidWay
  Copyright (C) 2002 Terje Eggestad

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
 * Revision 1.3  2002/09/22 22:54:57  eggestad
 * - removed getsvccost, new and improved lies in gateway.c
 * - added unexportservice()
 *
 * Revision 1.2  2002/08/09 20:50:16  eggestad
 * A Major update for implemetation of events and Task API
 *
 * Revision 1.1  2002/07/07 22:45:48  eggestad
 * *** empty log message ***
 *
 */

#include <MidWay.h>
#include <ipctables.h>

#include "gateway.h"
#include "store.h"
#include "connections.h"
#include "impexp.h"
#include "ipcmessages.h"
#include "SRBclient.h"
#include "SRBprotocolServer.h"



static char * RCSId UNUSED = "$Id$";
static char * RCSName UNUSED = "$Name$";

static Export * exportlist = NULL;
static Import * importlist = NULL;

/* add service to Import list if it's isn't there already, else return old
   entry. send a provide to mwd if it was there from before.  */
static Import *  condnewimport(char * service, int cost) 
{
  Import * imp, **previmp;
  
  /* special case: empty list */
  if (importlist == NULL) {

    DEBUG("primed the Import list with the service %s", service);
    imp = malloc(sizeof(Import));
    imp->svcid = UNASSIGNED;
    imp->next = NULL;
    imp->peerlist = NULL;
    strncpy(imp->servicename, service, MWMAXSVCNAME);

    importlist = imp;
    _mw_ipcsend_provide (service, cost, 0);

    return imp;
  };
  
  previmp = &importlist;
  for (imp = importlist; imp != NULL; previmp = &imp->next, imp = imp->next) 
    if ( strcmp(imp->servicename, service) == 0) break;
  
  // if the for loop found a match we return it. 
  if (imp != NULL) {
    DEBUG("We already have an import if the service %s with svcid = %#x", imp->servicename, imp->svcid); 
    return imp;
  };

  if (*previmp != NULL) {
    Fatal("Internal error: didn't find service %s imported but *previmp  is not NULL", service);
  };

  DEBUG("adding service %s to the end of importlist", service);
  imp = malloc(sizeof(Import));
  imp->svcid = UNASSIGNED;
  imp->next = NULL;
  imp->peerlist = NULL;
  strncpy(imp->servicename, service, MWMAXSVCNAME);
  
  *previmp = imp;
  _mw_ipcsend_provide (service, cost, 0);
  
  return imp;
};
  
  
int importservice(char * service, int cost, struct gwpeerinfo * peerinfo)
{
  
  Import * imp;
  peerlink ** ppl, * pl;

  imp = condnewimport(service, cost);

  /* find the last peerlink, but we check to see if we already has a
     lonk to the peerinfo struct */
  for(ppl =  &imp->peerlist; *ppl != NULL; ppl = &(*ppl)->next) {
    if ( (*ppl)->peer == peerinfo) {
      DEBUG("Hmmm import of service from a peer we already have an import from, " 
	    "this is probably a repeat from peer, OK!");
      return 0;
    };
  };
    
  *ppl = pl = malloc(sizeof(peerlink));
  pl->next = NULL;
  pl->peer = peerinfo;
  return 0;
};


/* add service to Export list if it's isn't there already, else return old
   entry. send a provide to mwd if it was there from before.  */
static Export *  condnewexport(char * service) 
{
  Export * exp, **prevexp;
  
  /* special case: empty list */
  if (exportlist == NULL) {

    DEBUG("primed the Export list with the service %s", service);
    exp = malloc(sizeof(Export));
    exp->next = NULL;
    exp->peerlist = NULL;
    strncpy(exp->servicename, service, MWMAXSVCNAME);

    exportlist = exp;
    return exp;
  };
  
  prevexp = &exportlist;
  for (exp = exportlist; exp != NULL; prevexp = &exp->next, exp = exp->next) 
    if (strcmp(exp->servicename, service) == 0) break;
  
  // if the for loop found a match we return it. 
  if (exp != NULL) {
    DEBUG("We already have an export for the service %s", exp->servicename); 
    return exp;
  };

  if (*prevexp != NULL) {
    Fatal("Internal error: didn't find service %s exported but *prevexp  is not NULL", service);
  };

  DEBUG("adding service %s to the end of exportlist", service);
  exp = malloc(sizeof(Export));
  exp->next = NULL;  
  exp->cost = -1;  
  exp->peerlist = NULL;
  strncpy(exp->servicename, service, MWMAXSVCNAME);
  
  *prevexp = exp;
  return exp;
};

int exportservicetopeer(char * service, struct gwpeerinfo * peerinfo)
{
  Export * exp;
  peerlink ** ppl, * pl;
  char pn[256];

  assert (peerinfo != NULL) ;
  assert (service != NULL) ;


  conn_getpeername(peerinfo->conn, pn, 255);

  DEBUG("exporting service %s to peer %s domid %d at %s", 
	service , peerinfo->instance, peerinfo->domainid, pn); 

  if (peerinfo->domainid == 0) {
      DEBUG("peer %s is localdomain", pn);
      if (service[0] == '.') {
	DEBUG("service %s begins with a '.', we do not export it to localdomain", service);
	return -1;
      };
    } else {
      DEBUG("peer %s is remotedomain id", pn, peerinfo->domainid);
      return -1;
    };

  exp = condnewexport(service);
  if (exp->cost == -1) 
    exp->cost = gw_getcostofservice(service);

  /* check to see if we already has exported to this peer */
  for (ppl = &exp->peerlist; *ppl != NULL; ppl = &(*ppl)->next) {
    if ((*ppl)->peer == peerinfo) {
      DEBUG("We've already exported service");
      return 1;
    };
  }

  pl = malloc(sizeof(peerlink));
  pl->peer = peerinfo;
  pl->next = NULL;

  *ppl = pl;


  DEBUG("Sending SRB PROVIDE cost = %d", exp->cost);
  return  _mw_srbsendprovide (peerinfo->conn, service, exp->cost);
};

static void unexportservice(char * servicename)
{
    Export * exp, **prevexp;
    peerlink * pl, *pl2;

    prevexp = &exportlist;
    for (exp = exportlist; exp != NULL; prevexp = &exp->next, exp = exp->next) 
      if (strcmp(exp->servicename, servicename) == 0) break;
    
    if (exp == NULL) {
      DEBUG("We've not exported %s", servicename); 
      return;
    };

    // unlink it
    *prevexp = exp->next;
    DEBUG("deleting export at %p", exp);
    for (pl = exp->peerlist; pl != NULL; ){
      _mw_srbsendunprovide(pl->peer->conn, servicename);
      pl2 = pl->next;
      free(pl);
      pl = pl2;
    };

    free(exp);
    DEBUG("unexport complete");
};


/* called when we get ab newprovire event from mwd */
void doprovideevent(char * service)
{
  Export * exp;
  int cost; 

  exp = condnewexport(service);

  cost = gw_getcostofservice(service);
  DEBUG2("cost of %s is %d", service, cost);
  if (cost == -1) unexportservice(service);

  if (cost != exp->cost) {
    exp->cost = cost;
    gw_provideservice_to_peers(service);
  };
};

void dounprovideevent(char * servicename)
{
  SERVICEID * svclist;

  svclist = _mw_get_services_byname(servicename, 0);
   
  if (svclist == NULL) unexportservice(servicename);
  else free(svclist);
};
