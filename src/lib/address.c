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
 * Revision 1.6  2001/10/03 22:45:10  eggestad
 * added multicast quert, and mem corruption fixes
 *
 * Revision 1.5  2000/11/29 23:16:29  eggestad
 * Parse of IPC url was wrong, IPC key ignored.
 *
 * Revision 1.4  2000/09/21 18:26:59  eggestad
 * - plugged a memory leak of mwadr
 * - cleared regexp matches at beginning, and reset
 *
 * Revision 1.3  2000/08/31 21:47:42  eggestad
 * Major rework for SRB
 *
 * Revision 1.2  2000/07/20 19:21:09  eggestad
 * url prefix for TCP/IP is now srb, not midway. fix up on double include prevention
 *
 * Revision 1.1.1.1  2000/03/21 21:04:06  eggestad
 * Initial Release
 *
 * Revision 1.1.1.1  2000/01/16 23:20:12  terje
 * MidWay
 *
 */
static char * RCSId = "$Id$";
static char * RCSName = "$Name$"; /* CVS TAG */

#include <sys/types.h>
#include <regex.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pwd.h>

#include <netinet/in.h>
#include <netdb.h>

#include <MidWay.h>

#define _ADDRESS_H
#include <address.h>
#include <SRBprotocol.h>
#include <multicast.h>


/* here we have the regexps that defined the legal formats for the URL.
   We rely on the regexpnot to allow thru any bad address.
*/

/* this is use solely to get the protocol name. */
// regexp match numbers
//                   01
#define RE_ALLPROTO "^([[:alpha:]]+):"

/* this is used to get the IPC values, 2 is the match for a numeric
   key.  6 is the instance name, and 9 is the userid. POSIX IPC is not
   yet supported.*/
// regexp match numbers
//              0    12              345  6               78  9
#define RE_IPC "^ipc:(([[:digit:]]+)|(((/)([[:alnum:]]+))?((@)([[:alnum:]]+))?))$"

/* this is used for an SRB url, it don't verify that eithre or both of
   ipaddress, and domain is given, leaving both out is here legal, we
   must check for thsi below.*/
// regexp match numbers
//               0     12   3                4 5                 67  8
#define RE_SRBP "^srbp:((//)([[:alnum:]\\.]+)(:([[:alnum:]]*))?)?((/)([[:alnum:]]+))?$"

/* http is missing here. */


#define MAXMATCH 20

static void debug_print_matches(regmatch_t *match, char * url) 
{
  int j = 0, len;
  while (j < MAXMATCH) {
    if (match[j].rm_so >=0 ){
      len = match[j].rm_eo - match[j].rm_so;
      mwlog(MWLOG_DEBUG3, "REGEXP Match %d in %s at %d to %d \"%*.*s\"", 
	    j, url, 
	    (long int) match[j].rm_so, (long int) match[j].rm_eo, 
	    len, len, url+match[j].rm_so);
    } 
    j++;    
  }
};


/* we now decode whats after ipc://, this is either
   a number for sysv ipc 
   a name (A directory ~/.MidWay/name is expected.  NYI
   a path for POSIX IPC */
static int url_decode_ipc(mwaddress_t * mwadr, char * url)
{
  char * tmpptr;
  regex_t ipcexp; 
  regmatch_t match[MAXMATCH]; 
  int j, rc;
  int key = -1;
  static char * instance = NULL;
  static char * userid = NULL;
  uid_t uid = -1;
  struct passwd * pwent;

  if (instance != NULL) {
    free(instance); 
    instance = NULL;
  };
  if (userid != NULL) {
    free(userid); 
    userid = NULL;
  };

  if ( regcomp( & ipcexp, RE_IPC, REG_EXTENDED|REG_ICASE) != 0) {
    //  if ( regcomp( & testexp, exp, 0) != 0) 
    mwlog(MWLOG_ERROR, "This can't happen: error on compiling regex %d", errno);
    return -1;
  };

  for (j = 0; j < MAXMATCH; j++) {
    match[j].rm_eo = -1;
    match[j].rm_so = -1;
  }
  
  rc = regexec (&ipcexp, url, MAXMATCH, match,0);
  mwlog(MWLOG_DEBUG3, "regexec IPC returned %d on %-40s: ",rc, url);
  if (rc != 0) {
    regerror( rc, &ipcexp, url , 255);
    mwlog(MWLOG_DEBUG1, "regexec of \"%s\" failed on %s, ", RE_IPC, url);
    return -1;
  }

  debug_print_matches(match, url);

  /* IPC KEY */
  if (match[2].rm_so != -1) {
    char c;
    c = url[match[2].rm_eo];
    url[match[2].rm_eo] = '\0';
    key = atol(url+match[2].rm_so);
    mwlog(MWLOG_DEBUG3, "Decoded IPC key in URI to be %d", key);
    url[match[2].rm_eo] = c;
  } 
      /* IPC KEY is mutualy exclusive, we really should check.... */
  /* INSTANCE NAME */
  if (match[6].rm_so != -1) {
    int len;
    len = match[6].rm_eo - match[6].rm_so;
    if (len > MWMAXNAMELEN) {
      mwlog(MWLOG_ERROR, "Instance name too long");
      return -1;
    };
    instance = malloc(len+1);
    strncpy(instance, url+match[6].rm_so, len);
    instance[len] = '\0';
    mwlog(MWLOG_DEBUG3, " Instancename=%s", instance);
  } 
  /* USERID */
  if (match[9].rm_so != -1) {
    int len;
    len = match[9].rm_eo - match[9].rm_so;
    if (len > MWMAXNAMELEN) {
      mwlog(MWLOG_ERROR, "user name too long");
      return -1;
    };
    userid=malloc(len+1); 
    strncpy(userid, url+match[9].rm_so, len);
    userid[len] = '\0';

    mwlog(MWLOG_DEBUG3, " Userid=%s", userid);
  } 

  /* first we check for the key, if present, we're happy. */
  if (key != -1) {
    mwadr->sysvipckey = key;
    return 0;
  };

  /* first we handle the userid */
  if (userid != NULL) {
    pwent = getpwnam(userid);
    if (pwent == NULL) {
      mwlog(MWLOG_ERROR, "userid %s in url %s don't exists", userid, url);
      return -1;
    };
    uid = pwent->pw_uid;
  } else {
    uid = getuid();
  };

  return -1;
  /*  this was in the case for ipc:username or ipc:filepathtoconfig */
  if ( (instance == NULL) || (strcasecmp(instance, "default") == 0) ) {
    mwadr->sysvipckey = uid;
    return 0;
  } else {
    /* assume ftok on config. */
    mwlog(MWLOG_ERROR, "use of config file is not yet implemented");
    return -1;
  };
  /* check for posix ipc **/

  
  return -1;
};

static int url_decode_srbp (mwaddress_t * mwadr, char * url)
{
  int len=0, urltype, j, rc;
  static char * ipaddress = NULL;
  static char * port = NULL;
  char * domain = NULL;
  regex_t srbexp; 
  regmatch_t match[MAXMATCH]; 

  struct hostent *hent;
  struct servent *sent;
  short ns_port;

  if (ipaddress != NULL) {
    free(ipaddress); 
    ipaddress = NULL;
  };
  if (port != NULL) {
    free(port); 
    port = NULL;
  };

  if ( regcomp( & srbexp, RE_SRBP, REG_EXTENDED|REG_ICASE) != 0) {
    mwlog(MWLOG_ERROR, "error on compiling regex %d\n", errno);
    exit;
  };

  for (j = 0; j < MAXMATCH; j++) {
    match[j].rm_eo = -1;
    match[j].rm_so = -1;
  }
  
  rc = regexec (&srbexp, url, MAXMATCH, match,0);
  mwlog(MWLOG_DEBUG3, "regexec SRB returned %d on %-40s: ",rc, url);
  if (rc != 0) {
    regerror(rc, &srbexp, url , 255);
    mwlog(MWLOG_DEBUG1, "regexec of \"%s\" failed on %s", RE_SRBP, url);
    return -1;
  }

  /* IP ADDRESS */
  if (match[3].rm_so != -1) {
    int len;
    len = match[3].rm_eo - match[3].rm_so;
    ipaddress = malloc(len+1);
    strncpy(ipaddress, url+match[3].rm_so, len);
    ipaddress[len] = '\0'; 
    mwlog(MWLOG_DEBUG3, "ipaddress=%s", ipaddress);
  } 
  
  /* TCP PORT */
  if (match[5].rm_so != -1) {
    int len;
    len = match[5].rm_eo - match[5].rm_so;
    port = malloc(len+1);
    strncpy(port, url+match[5].rm_so, len);
    port[len] = '\0';
    mwlog(MWLOG_DEBUG3, "port=%s", port);
  } 

  /* DOMAIN */
  if (match[8].rm_so != -1) {
    len = match[8].rm_eo - match[8].rm_so;
    domain = malloc(len+1); 
    strncpy(domain, url+match[8].rm_so, len);
    domain[len] = '\0';
    mwlog(MWLOG_DEBUG3, "DOMAIN=%s", domain);
  } 

  /* OK, we've obtained all the values from the url, now we do the
     necessary copying, and convertion to binary data */
  if ( (domain == NULL) && (ipaddress == NULL) ) {
    mwlog(MWLOG_ERROR, "Either domain or ipaddress must be specified");
    return -1;
  };
  
  if (domain != NULL) {
    mwadr->domain = domain;
  };
  
  /* TCP PORT */
  if (port != NULL) {
    int servname = 0;
    len = strlen(port);
    for (j = 0;  j < len; j++)
      servname += !isdigit(port[j]);
    if (servname == 0) {
      ns_port = htons(atol(port));
    } else {
      sent = getservbyname(port, "TCP");
      if (sent == NULL) {
	mwlog(MWLOG_ERROR, "No such service %s", port);
	return -1;
      };
      ns_port = sent->s_port;
    };
  } else 
    ns_port = htons(SRB_DEFAULT_PORT);


  /* IP ADDRESS */
  if (ipaddress == NULL) {
    int s, rc; 
    instanceinfo is;
    /* we've only the domain, and no IP address, do a multicast to get
       a broker with this domain.  this should be the normal case */
      s = socket(AF_INET, SOCK_DGRAM, 0);
      if (s == -1) return -1;

      rc = _mw_sendmcastquery(s, domain, NULL);
      if (rc == -1) {
	close(s);
	return -1;
      };

  retry:
      errno = 0;
      rc =  _mw_getmcastreply(s, &is, 2.0);
      mwlog(MWLOG_DEBUG1,  "_mw_getmcastreply returned %d, errno=%d", rc, errno);
      if ( (rc == -1) && (errno == EBADMSG) )	goto retry;
      
      if (rc == -1) return -1;
      {
	char buf[64];
	struct sockaddr_in * sin;
	sin = ((struct sockaddr_in *) &is.address);
	mwlog(MWLOG_DEBUG1,  "Gateways address: family %d, port %d ipaddress V4 %s", 
	      sin->sin_family, 
	      ntohs(sin->sin_port), 
	      inet_ntop(AF_INET, &sin->sin_addr, buf, 64));
      };
      mwadr->ipaddress_v4 = malloc(sizeof(struct sockaddr_in));
      memcpy(mwadr->ipaddress_v4, &is.address, sizeof(struct sockaddr_in));
      close (s);
      {
	char buf[64];
	mwlog(MWLOG_DEBUG1,  "Gateways address: family %d, port %d ipaddress V4 %s", 
	      mwadr->ipaddress_v4->sin_family, 
	      ntohs(mwadr->ipaddress_v4->sin_port), 
	      inet_ntop(AF_INET, &mwadr->ipaddress_v4->sin_addr, buf, 64));
      };
  } else {
    
    hent = gethostbyname(ipaddress);
    if (hent == NULL) {
      mwlog(MWLOG_ERROR, "Unable to resolve hostname %s", ipaddress);
      return -1;
    };
    mwlog(MWLOG_DEBUG3, "ipaddress %s => hostname %s type = %d, addr = %#X", 
	  ipaddress, hent->h_name, hent->h_addrtype, * (int *) hent->h_addr_list[0]);
    if (hent->h_length != 4) {
      mwlog(MWLOG_ERROR, "Failed to resolve hostname, address length %d != 4", 
	    hent->h_length );
      return -1;
    };
    
    /* ALL RIGHT! */
    mwadr->ipaddress_v4 = malloc(sizeof(struct sockaddr_in));
    mwadr->ipaddress_v4->sin_family = AF_INET;
    mwadr->ipaddress_v4->sin_port = ns_port;
    mwadr->ipaddress_v4->sin_addr = * (struct in_addr *) hent->h_addr_list[0];
  };

  {
    char buf[64];
    mwlog(MWLOG_DEBUG1,  "Gateways address: family %d, port %d ipaddress V4 %s", 
	mwadr->ipaddress_v4->sin_family, 
	ntohs(mwadr->ipaddress_v4->sin_port), 
	inet_ntop(AF_INET, &mwadr->ipaddress_v4->sin_addr, buf, 64));
  };
  return 0;
};

/* the only exported function from this module. */
mwaddress_t * _mwdecode_url(char * url)
{
  mwaddress_t * mwadr;
  regex_t allexp; 
  regmatch_t match[MAXMATCH]; 
  int rc;

  memset (match, '\0', sizeof(regmatch_t) * MAXMATCH);

  if (url == NULL) {    
    mwlog(MWLOG_DEBUG1,  "_mwdecode_url: Attempting to get the URL form Env var MWURL");
    /* if url is not set, check MWURL or MWADDRESS */
    url = getenv ("MWURL");
    if (url == NULL) url = getenv ("MWADDRESS");
    
  } 

  mwadr = malloc(sizeof(mwaddress_t));
  if (url == NULL) {
    /* We assume IPC, and use UID as key */
    mwadr->sysvipckey = getuid();
    mwadr->protocol = MWSYSVIPC;
    mwlog(MWLOG_DEBUG1,  "_mwdecode_url: we have no url, assuming ipc:%d", mwadr->sysvipckey);
    return mwadr;
  };

  mwlog(MWLOG_DEBUG1,  "_mwdecode_url: url = %s", url);

  mwadr->protocol =  -1;
  if ( regcomp( & allexp, RE_ALLPROTO, REG_EXTENDED|REG_ICASE) != 0) {
    //  if ( regcomp( & testexp, exp, 0) != 0) 
    mwlog(MWLOG_ERROR, "error on compiling regex %s errno=%d\n", RE_ALLPROTO,  errno);
    free(mwadr);
    return NULL;
  };
  rc = regexec (&allexp, url, MAXMATCH, match,0);
  mwlog(MWLOG_DEBUG3, "regexec IPC returned %d on %-40s: ",rc, url);
  if (rc != 0) {
    regerror( rc, &allexp, url , 255);
    mwlog(MWLOG_DEBUG1, "regexec of \"%s\" failed on %s, ", RE_ALLPROTO, url);
    free(mwadr);
    return NULL;
  }


  /* here we get the url protocol header 
     protocol: 
     
     legal protocols are ipc, srbp, http we pass the rest of the url
     to the decode function of teh appropriate protocol.  */
  if (0 == strncasecmp("ipc", url+match[1].rm_so, 3)) {
    mwadr->protocol = MWSYSVIPC;
    rc = url_decode_ipc(mwadr, url);
    if (rc != 0) return NULL;
    else return mwadr;
  };
  
  if (0 == strncasecmp("srbp", url+match[1].rm_so, 4)) {
    mwadr->protocol = MWSRBP;
    rc = url_decode_srbp(mwadr, url);
    if (rc != 0) return NULL;
      else return mwadr;
  };
  if (0 == strncasecmp("http", url+match[1].rm_so, 4)) {
    mwadr->protocol = MWHTTP;
    rc = url_decode_srbp(mwadr, url);
    if (rc != 0) return NULL;
    else return mwadr;
  };
  
  mwlog(MWLOG_ERROR, 
	"Unknown protocol %s in the URL \"%s\" for MidWay instance, error %d", 
	url+match[1].rm_so, url, mwadr->protocol);
  return NULL;
};
