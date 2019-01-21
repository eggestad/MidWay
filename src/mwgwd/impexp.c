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


#include <errno.h>

#include <MidWay.h>
#include <ipctables.h>

#include "gateway.h"
#include "store.h"
#include "connections.h"
#include "impexp.h"
#include "ipcmessages.h"
#include "SRBclient.h"
#include "SRBprotocolServer.h"


static Export * exportlist = NULL;
static Import * importlist = NULL;

DECLAREMUTEX(impmutex);


/*******************************   for dumping internal data */

static void impdumplist(FILE * fp)
{
   Import * imp;
   peerlink * pl;

   fprintf(fp, "* DUMPING IMPORT LIST *\n");
   for (imp = importlist; imp != NULL; imp = imp->next) {
      fprintf (fp, "  imp=%p servicename=%s peerlist=%p next=%p\n", 
	       imp, imp->servicename, imp->peerlist, imp->next);
      for (pl = imp->peerlist; pl != NULL; pl = pl->next) {
	 fprintf(fp, "    peerlist=%p instance=%s svcid=%d cost=%d gwid=%d next=%p\n", 
		 pl, pl->peer->instance, 
		 SVCID2IDX(pl->svcid), pl->cost, GWID2IDX(pl->peer->gwid), pl->next);
      };
   };
};

static void log_impdumplist() {
   char printbuffer[64*1024];
   FILE * memfp = fmemopen(printbuffer, 64*1024, "w");
   impdumplist(memfp);
   fclose(memfp);
   DEBUG2("%s", printbuffer);
}

static void expdumplist(FILE * fp)
{
   Export * exp;
   peerlink * pl;

   fprintf (fp,"* DUMPING EXPORT LIST *\n");
   for (exp = exportlist; exp != NULL; exp = exp->next) {
      fprintf(fp," exp=%p servicename=%s cost=%d peerlist=%p next=%p\n", 
	      exp, exp->servicename, exp->cost, exp->peerlist, exp->next);
      for (pl = exp->peerlist; pl != NULL; pl = pl->next) {
	 fprintf(fp,"  peerlist=%p instance=%s gwid=%d next=%p\n", 
		 pl, pl->peer->instance, GWID2IDX(pl->peer->gwid), pl->next);
      };
   };
 
};
static void log_expdumplist() {
   char printbuffer[64*1024];
   FILE * memfp = fmemopen(printbuffer, 64*1024, "w");
   expdumplist (memfp);
   fclose(memfp);
   DEBUG2("%s", printbuffer);
}


void dumpImpExp(FILE * fp) {
   impdumplist(fp);
   expdumplist(fp);
}

/*********************************/


/* find the Import element in the import list if it exist, or return NULL) */
static Import * impfind(char * service)
{
  Import *  imp;
  if (importlist == NULL) {
    return NULL;
  };
  
  for (imp = importlist; imp != NULL; imp = imp->next) 
    if ( strcmp(imp->servicename, service) == 0) return imp;
  return NULL;
};
    
void impsetsvcid(char * service, SERVICEID svcid, GATEWAYID gwid)
{
  Import * imp;
  peerlink  * pl;

  LOCKMUTEX(impmutex);

  imp = impfind(service);
  if (imp) {
    DEBUG("imported service %s from gwid %d has serviceid %d", 
	  service, GWID2IDX(gwid), SVCID2IDX(svcid));

    for (pl = imp->peerlist; pl != NULL; pl = pl->next) {
      if (pl->peer->gwid == gwid) {
	pl->svcid = svcid;
	goto out;
      };
    };
  } 

  DEBUG("Apparently the peer unprovided %s before mwd replied with svcid, sending unprovide to mwd", service);
  _mw_ipcsend_unprovide_for_id (gwid, service, 0);

 out:
  UNLOCKMUTEX(impmutex);
};


  
/* add service to Import list if it's isn't there already, else return old
   entry. send a provide to mwd if it was there from before.  */
static Import *  condnewimport(char * service, int cost) 
{
  Import * imp;
  
  /* special case: empty list */
  if (importlist == NULL) {

    DEBUG("primed the Import list with the service %s", service);
    imp = malloc(sizeof(Import));
    imp->next = NULL;
    imp->peerlist = NULL;
    strncpy(imp->servicename, service, MWMAXSVCNAME);

    importlist = imp;

    return imp;
  };
  
  imp = impfind(service);
  
  // if the for loop found a match we return it. 
  if (imp != NULL) {
    DEBUG("We already have an import if the service %s ", imp->servicename); 
    return imp;
  };

  DEBUG("adding service %s to the top of importlist", service);
  imp = malloc(sizeof(Import));
  imp->next = NULL;
  imp->peerlist = NULL;
  strncpy(imp->servicename, service, MWMAXSVCNAME);
  imp->next = importlist;
  importlist = imp;
  
  log_impdumplist();

  return imp;
};
  
  
int importservice(char * service, int cost, struct gwpeerinfo * peerinfo)
{
  int rc;
  Import * imp;
  peerlink ** ppl, * pl;
    
  LOCKMUTEX(impmutex);
  log_impdumplist();
  imp = condnewimport(service, cost);

  /* find the last peerlink, but we check to see if we already has a
     lonk to the peerinfo struct */
  for(ppl =  &imp->peerlist; *ppl != NULL; ppl = &(*ppl)->next) {
    if ( (*ppl)->peer == peerinfo) {
      DEBUG("Hmmm import of service from a peer we already have an import from, " 
	    "this is probably a repeat from peer, OK!");
      goto out;
    };
  };
    
  *ppl = pl = malloc(sizeof(peerlink));
  pl->next = NULL;
  pl->svcid = UNASSIGNED;
  pl->peer = peerinfo;

  rc = _mw_ipcsend_provide_for_id (peerinfo->gwid, service, cost, 0);

  if (rc < 0) {
    Error("in sending a provide message to mwd, should not be able to happen, rc = %d", rc);
    UNLOCKMUTEX(impmutex);
    unimportservice(service, peerinfo);
    return rc;
  };

 out:  
  log_impdumplist();
  UNLOCKMUTEX(impmutex);
  return 0;
};


/* OK, fairly hary pointer stuff. We got a single linked list of
   Imports, each has a single linked list of peerlink's that inturn
   has pointers to the gwpeerinfo's.  we go thru the Import list, and
   for each we go thru the peerlink list and chech to see if we got a
   peerlink to this peer gwpeerinfo struct. If so remove the peerlink
   element for this gwpeerinfo.  AND if this was the last peerlink for
   this Import, remove this Import from the Imports list.*/
 
static void impcleanuppeer(struct gwpeerinfo * pi)
{
  Import * imp, **pimp;
  peerlink ** ppl, * pl;

  LOCKMUTEX(impmutex);

  log_impdumplist();

  for (pimp = &importlist; *pimp != NULL; ) {
    imp = *pimp;
    DEBUG(" pimp = %p *pimp = %p imp = %p service %s ", pimp, *pimp, imp, imp->servicename);

    for(ppl =  &imp->peerlist; *ppl != NULL; ppl = &(*ppl)->next) {
      pl = * ppl;
      if ( pl->peer != pi) continue;
      
      DEBUG("found the peerlist element, removing");
      *ppl = pl->next;

      if (imp->peerlist == NULL)  {
	
	 DEBUG("  GWID %x ?=  %x", pi->gwid, pl->peer->gwid);

	 if (pl->svcid != UNASSIGNED) {
	    DEBUG("sending unprovide for GWID %d service %s service id %d", 
		  GWID2IDX(pl->peer->gwid), imp->servicename, SVCID2IDX(pl->svcid));
	    _mw_ipcsend_unprovide_for_id (pl->peer->gwid, imp->servicename, pl->svcid);
	 } 
      } else {
	 
	 DEBUG("Not the last peer to provide %s, not sending unprovide to mwd", imp->servicename); 
	 // TODO: recalc cost
	 break;
      };
      
      free(pl);
      
      DEBUG("now we must remove the  Import element from the importlist %p", importlist);
      *pimp = imp->next;
      free(imp);
      imp = NULL; // to signal below that we pimp already point to the next
      break;
    }; 
    if (*pimp == NULL) break;
    DEBUG("imp %p , pimp = %p", imp, pimp);
    if (imp != NULL)
       pimp = &(*pimp)->next;
  };

  log_impdumplist();
  UNLOCKMUTEX(impmutex);
  return;
}  

int unimportservice(char * service, struct gwpeerinfo * pi)
{
  int error;
  Import * imp, **pimp;
  peerlink ** ppl, * pl;
  
  if ( (service == NULL) || (pi == NULL) ) return -EINVAL;

  LOCKMUTEX(impmutex);

  log_impdumplist();
  imp = impfind(service);
  
  if (imp == NULL) {
    Warning("attempted unimport of a service we've not imported");
    goto out;
  };
  
  for(ppl =  &imp->peerlist; *ppl != NULL; ppl = &(*ppl)->next) {
    pl = * ppl;
    if ( pl->peer != pi) continue;
    
    DEBUG("found the peerlist element, removing");

    DEBUG("last peer to provide %s (%d), sending unprovide to mwd", service, SVCID2IDX(pl->svcid));
    /* if we fail, we fail... */
    if (pl->svcid != UNASSIGNED) 
      _mw_ipcsend_unprovide_for_id (pl->peer->gwid, service, pl->svcid);
    else 
      DEBUG("we got an unimport before svcid has been assigned, probably because mwd has not yet answered."
	    " we're not sending unprovide to mwd, it will be done in the impsetsvcid()"
	    " when the provide replyy comes. ");


    *ppl = pl->next;
    free(pl);
    
    if (imp->peerlist != NULL)  {
      DEBUG("Not the last peer to provide %s, not sending unprovide to mwd", service); 
      // TODO: recalc cost
      goto out;
    };

    DEBUG("now we must remove the  Import element from the importlist %p", importlist);

    for (pimp = &importlist; *pimp != NULL; *pimp = (*pimp)->next) {
      DEBUG(" pimp = %p *pimp = %p imp = %p", pimp, *pimp, imp);
      if (imp ==  *pimp) {
	*pimp = imp->next;
	free(imp);
	goto out;
      }; 
    };
    Error("unable to find the import elemennt (we just found it before)");
    error= -EFAULT;
    goto out;
  };
  Warning("Hmm went thru the peerlist but didn't find the peer, could ab a dublicate unprovide message");

 out:
  log_impdumplist();
  UNLOCKMUTEX(impmutex);
  return error;
};

/* for a given service and serviceid, (service is just for perf only */
Connection * impfindpeerconn(char * service, SERVICEID svcid)
{
  Import * imp;
  peerlink * pl;
  Connection * rconn = NULL;

  LOCKMUTEX(impmutex);
  log_impdumplist();

  for (imp = importlist; imp != NULL; imp = imp->next) {
    if (strcmp(service, imp->servicename) != 0) continue;
    DEBUG2("found imp object @ %p, searching peerlist", imp);
    for (pl = imp->peerlist; pl != NULL; pl = pl->next) {
      DEBUG2("peerlist @ %p, svcid = %#x instance %s", pl, pl->svcid, pl->peer->instance);
      if (pl->svcid == svcid) {
	rconn = pl->peer->conn;
	DEBUG2("returning connection %p: fd=%d instance %s", rconn, rconn->fd, rconn->peeraddr_string);
	goto out;
      };
    };
  };  

 out:
  UNLOCKMUTEX(impmutex);
  return rconn;
};

/************************************************************************
 *
 * Export related functions
 *
 ************************************************************************/

/* add service to Export list if it's isn't there already, else return old
   entry. send a provide to mwd if it was there from before.  */
static Export *  condnewexport(char * service) 
{
  Export * exp, **prevexp;
  
  /* special case: empty list */
  prevexp = &exportlist;
 
  if (exportlist == NULL) {
    DEBUG("primed the Export list with the service %s", service);
    goto addandinit;
  };
  
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

 addandinit:

  exp = malloc(sizeof(Export));
  exp->next = NULL;  
  exp->cost = -1;  
  exp->peerlist = NULL;
  strncpy(exp->servicename, service, MWMAXSVCNAME);
  
  *prevexp = exp;

  log_expdumplist();
  return exp;
};

int exportservicetopeer(char * service, struct gwpeerinfo * peerinfo)
{
  Export * exp;
  peerlink ** ppl, * pl;
  
  assert (peerinfo != NULL) ;
  assert (service != NULL) ;

  log_expdumplist();
  
  DEBUG("exporting service %s to peer %s domid %d at %s", 
	service , peerinfo->instance, peerinfo->domainid, peerinfo->conn->peeraddr_string); 

  if (peerinfo->domainid == 0) {
      DEBUG("peer %s is localdomain", peerinfo->conn->peeraddr_string);
      if (service[0] == '.') {
	DEBUG("service %s begins with a '.', we do not export it to localdomain", service);
	return -1;
      };
    } else {
      DEBUG("peer %s is remotedomain id %d", peerinfo->conn->peeraddr_string, peerinfo->domainid);
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

  log_expdumplist();
  DEBUG("Sending SRB PROVIDE cost = %d", exp->cost);
  return  _mw_srbsendprovide (peerinfo->conn, service, exp->cost);
};

static void unexportservice(char * servicename)
{
    Export * exp, **prevexp;
    peerlink * pl, *pl2;

    log_expdumplist();
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
    log_expdumplist();
    DEBUG("unexport complete");
};

static void expcleanuppeer(struct gwpeerinfo * pi)
{
  Export * exp;
  peerlink * pl, **ppl;

  log_expdumplist();

  for (exp = exportlist; exp != NULL; exp = exp->next) {
    DEBUG(" export = %s", exp->servicename);
    for (ppl = &exp->peerlist; *ppl != NULL; ppl = &(*ppl)->next) {
      pl = *ppl;
      DEBUG("  peerlink %p pi = %p", pl->peer, pi);
      if (pl->peer != pi) continue;
      
      *ppl = pl->next;
      free(pl);
      break;
    };
  };  

  log_expdumplist();
  DEBUG("unexport complete");  
};

/**********************************************************************************/

void impexp_cleanuppeer(struct gwpeerinfo * pi)
{
   if (pi == NULL) return;
   DEBUG("Cleaning up after GW %#x hostname = %s instance %s @ %s", 
	 pi->gwid, 
	 pi->hostname, 
	 pi->instance, 
	 (pi->conn == NULL) ? "unconnected" : pi->conn->peeraddr_string);

   expcleanuppeer(pi);
   impcleanuppeer(pi);
};

/* called when we get ab newprovire event from mwd */
void doprovideevent(char * service)
{
  Export * exp;
  int cost; 

  DEBUG("service %s", service);
  exp = condnewexport(service);

  cost = gw_getcostofservice(service);
  DEBUG2("cost of %s is %d", service, cost);
  if (cost == -1) unexportservice(service);

  DEBUG2("cost = %d, already exported cost = %d", cost, exp->cost);

  // we use the exp->cost flags to see if we' done a general provide before.
  // if teh cost changes, we must do a general provide 
  if (cost != exp->cost) {
    exp->cost = cost;
    gw_provideservice_to_peers(service);
  };
};

void dounprovideevent(char * servicename)
{
  SERVICEID * svclist;

  // TODO: we should have a more efficient function than this one */
  svclist = _mw_get_services_byname(servicename, NULL, 0);
   
  if (svclist == NULL) unexportservice(servicename);
  else free(svclist);
};
