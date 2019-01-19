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


#include <sys/types.h>
#include <ctype.h>
#include <regex.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pwd.h>
#include <unistd.h>

#include <netinet/in.h>
#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <MidWay.h>

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
#define RE_IPC "^ipc:(([[:digit:]]+)|(((\/)([[:alnum:]]+))?((@)([[:alnum:]]+))?))$"

/* this is used for an SRB url, it don't verify that eithre or both of
   ipaddress, and domain is given, leaving both out is here legal, we
   must check for thsi below.*/
// regexp match numbers
//               0     12   3                4 5                 67  8
#define RE_SRBP "^srbp:((\/\/)([[:alnum:]\\.\\-]+)(:([[:alnum:]]*))?)?((\/)([[:alnum:]]+))?$"

/* http is missing here. */


#define MAXMATCH 20

static void debug_print_matches(regmatch_t *match, char * url) 
{
  int j = 0, len;
  while (j < MAXMATCH) {
    if (match[j].rm_so >=0 ){
      len = match[j].rm_eo - match[j].rm_so;
      DEBUG3("REGEXP Match %d in %s at %ld to %ld \"%*.*s\"", 
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
static int url_decode_ipc(mwaddress_t * mwadr, const char * ipcurl)
{
  int urllen = strlen(ipcurl);
  char url[urllen+1];
  strncpy(url, ipcurl, urllen+1);
  
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
    Error("This can't happen: error on compiling regex %d", errno);
    return -1;
  };

  for (j = 0; j < MAXMATCH; j++) {
    match[j].rm_eo = -1;
    match[j].rm_so = -1;
  }
  
  rc = regexec (&ipcexp, url, MAXMATCH, match,0);
  DEBUG3("regexec IPC returned %d on %-40s: ",rc, url);
  if (rc != 0) {
     regerror( rc, &ipcexp, url , 255);
    DEBUG1("regexec of \"%s\" failed on %s, ", RE_IPC, url);
    return -1;
  }

  debug_print_matches(match, url);

  /* IPC KEY */
  if (match[2].rm_so != -1) {
    char c;
    c = url[match[2].rm_eo];
    url[match[2].rm_eo] = '\0';
    key = atol(url+match[2].rm_so);
    DEBUG3("Decoded IPC key in URI to be %d", key);
    url[match[2].rm_eo] = c;
  } 
      /* IPC KEY is mutualy exclusive, we really should check.... */
  /* INSTANCE NAME */
  if (match[6].rm_so != -1) {
    int len;
    len = match[6].rm_eo - match[6].rm_so;
    if (len > MWMAXNAMELEN) {
      Error("Instance name too long");
      return -1;
    };
    instance = malloc(len+1);
    strncpy(instance, url+match[6].rm_so, len);
    instance[len] = '\0';
    DEBUG3(" Instancename=%s", instance);
  } 
  /* USERID */
  if (match[9].rm_so != -1) {
    int len;
    len = match[9].rm_eo - match[9].rm_so;
    if (len > MWMAXNAMELEN) {
      Error("user name too long");
      return -1;
    };
    userid=malloc(len+1); 
    strncpy(userid, url+match[9].rm_so, len);
    userid[len] = '\0';

    DEBUG3(" Userid=%s", userid);
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
      Error("userid %s in url %s don't exists", userid, url);
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
    Error("use of config file is not yet implemented");
    return -1;
  };
  /* check for posix ipc **/

  
  return -1;
};

static int url_decode_srbp (mwaddress_t * mwadr, const char * srburl)
{
  int urllen = strlen(srburl);
  char url[urllen+1];
  strncpy(url, srburl, urllen+1);

  int len=0, j, rc;
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
     Fatal("error on compiling regex %d\n", errno);
  };

  for (j = 0; j < MAXMATCH; j++) {
    match[j].rm_eo = -1;
    match[j].rm_so = -1;
  }
  
  rc = regexec (&srbexp, url, MAXMATCH, match,0);
  DEBUG3("regexec SRB returned %d on %-40s: ",rc, url);
  if (rc != 0) {
    regerror(rc, &srbexp, url , 255);
    DEBUG1("regexec of \"%s\" failed on %s", RE_SRBP, url);
    return -1;
  }

  /* IP ADDRESS */
  if (match[3].rm_so != -1) {
    int len;
    len = match[3].rm_eo - match[3].rm_so;
    ipaddress = malloc(len+1);
    strncpy(ipaddress, url+match[3].rm_so, len);
    ipaddress[len] = '\0'; 
    DEBUG3("ipaddress=%s", ipaddress);
  } 
  
  /* TCP PORT */
  if (match[5].rm_so != -1) {
    int len;
    len = match[5].rm_eo - match[5].rm_so;
    port = malloc(len+1);
    strncpy(port, url+match[5].rm_so, len);
    port[len] = '\0';
    DEBUG3("port=%s", port);
  } 

  /* DOMAIN */
  if (match[8].rm_so != -1) {
    len = match[8].rm_eo - match[8].rm_so;
    domain = malloc(len+1); 
    strncpy(domain, url+match[8].rm_so, len);
    domain[len] = '\0';
    DEBUG3("DOMAIN=%s", domain);
  } 

  /* OK, we've obtained all the values from the url, now we do the
     necessary copying, and convertion to binary data */
  if ( (domain == NULL) && (ipaddress == NULL) ) {
    Error("Either domain or ipaddress must be specified");
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
	Error("No such service %s", port);
	return -1;
      };
      ns_port = sent->s_port;
    };
  } else 
    ns_port = htons(SRB_BROKER_PORT);


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
      DEBUG1("_mw_getmcastreply returned %d, errno=%d", rc, errno);
      if ( (rc == -1) && (errno == EBADMSG) )	goto retry;
      
      if (rc == -1) return -1;
      {
	char buf[64];
	struct sockaddr_in * sin;
	sin = ((struct sockaddr_in *) &is.address);
	DEBUG1("Gateways address: family %d, port %d ipaddress V4 %s", 
	      sin->sin_family, 
	      ntohs(sin->sin_port), 
	      inet_ntop(AF_INET, &sin->sin_addr, buf, 64));
      };
      mwadr->ipaddress.sin4 = malloc(sizeof(struct sockaddr_in));
      memcpy(mwadr->ipaddress.sin4, &is.address, sizeof(struct sockaddr_in));
      close (s);
      {
	char buf[64];
	DEBUG1("Gateways address: family %d, port %d ipaddress V4 %s", 
	      mwadr->ipaddress.sin4->sin_family, 
	      ntohs(mwadr->ipaddress.sin4->sin_port), 
	      inet_ntop(AF_INET, &mwadr->ipaddress.sin4->sin_addr, buf, 64));
      };
  } else {

     //#define USE_GETHOSTBYNAME // old school
#ifdef USE_GET_HOSTBYNAME
    hent = gethostbyname(ipaddress);
    if (hent == NULL) {
      Error("Unable to resolve hostname %s", ipaddress);
      return -1;
    };
    DEBUG3("ipaddress %s => hostname %s type = %d, addr = %#X", 
	  ipaddress, hent->h_name, hent->h_addrtype, * (int *) hent->h_addr_list[0]);
    if (hent->h_length != 4) {
      Error("Failed to resolve hostname, address length %d != 4", 
	    hent->h_length );
      return -1;
    };
    
    /* ALL RIGHT! */
    mwadr->ipaddress.sin4 = malloc(sizeof(struct sockaddr_in));
    mwadr->ipaddress.sin4->sin_family = AF_INET;
    mwadr->ipaddress.sin4->sin_port = ns_port;
    mwadr->ipaddress.sin4->sin_addr = * (struct in_addr *) hent->h_addr_list[0];
#else
    struct addrinfo hints = { 0 };
    hints.ai_flags = 0; //AI_CANONNAME;
    hints.ai_family = AF_INET; // when we support IP6 : AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    
    struct addrinfo *res = NULL;
    
    int rc = getaddrinfo(ipaddress, NULL, &hints, &res);
    if (rc != 0) {
       Error("Failed to resolve hostname %s", ipaddress);
       return -1;
    }
    while(res != NULL) {
       
       if (res->ai_family == AF_INET) {
	  struct sockaddr_in *in = (struct sockaddr_in *) res->ai_addr;
	  mwadr->ipaddress.sin4 = malloc(sizeof(struct sockaddr_in));
	  mwadr->ipaddress.sin4->sin_family = AF_INET;
	  mwadr->ipaddress.sin4->sin_port = ns_port;
	  mwadr->ipaddress.sin4->sin_addr = in->sin_addr;
	  break;
       }
       res = res->ai_next;
    }
    freeaddrinfo(res);
  }
#endif
  {
    char buf[64];
    DEBUG1("Gateways address: family %d, port %d ipaddress V4 %s", 
	mwadr->ipaddress.sin4->sin_family, 
	ntohs(mwadr->ipaddress.sin4->sin_port), 
	inet_ntop(AF_INET, &mwadr->ipaddress.sin4->sin_addr, buf, 64));
  };
  return 0;
};

/* the only exported function from this module. */
int _mwdecode_url(const char * url, mwaddress_t * mwadr)
{
  regex_t allexp; 
  regmatch_t match[MAXMATCH]; 
  int rc;

  memset (match, '\0', sizeof(regmatch_t) * MAXMATCH);

    /* if url is not set, check MWURL or MWADDRESS */
  if (url == NULL) {    
    DEBUG1("_mwdecode_url: Attempting to get the URL from Env var MWADDRESS");
    url = getenv ("MWADDRESS");
  }
  
  if (url == NULL) {    
    DEBUG1("_mwdecode_url: Attempting to get the URL from Env var MWURL");
    url = getenv ("MWURL");
  } 

  if (url == NULL) {
    /* We assume IPC, and use UID as key */
    mwadr->sysvipckey = getuid();
    mwadr->protocol = MWSYSVIPC;
    DEBUG1("_mwdecode_url: we have no url, assuming ipc:%d", mwadr->sysvipckey);
    return 0;
  };

  DEBUG1("_mwdecode_url: url = %s", url);

  if ( regcomp( & allexp, RE_ALLPROTO, REG_EXTENDED|REG_ICASE) != 0) {
    //  if ( regcomp( & testexp, exp, 0) != 0) 
    Error("error on compiling regex %s errno=%d\n", RE_ALLPROTO,  errno);
    return -EINVAL;
  };
  rc = regexec (&allexp, url, MAXMATCH, match,0);
  DEBUG3("regexec IPC returned %d on %-40s: ",rc, url);
  if (rc != 0) {
    char errbuf[255];
    regerror( rc, &allexp, errbuf , 255);
    DEBUG1("regexec of \"%s\" failed on %s, ", RE_ALLPROTO, errbuf);
    return -EINVAL;
  }


  /* here we get the url protocol header 
     protocol: 
     
     legal protocols are ipc, srbp, http we pass the rest of the url
     to the decode function of teh appropriate protocol.  */
  if (0 == strncasecmp("ipc", url+match[1].rm_so, 3)) {
    mwadr->protocol = MWSYSVIPC;
    rc = url_decode_ipc(mwadr, (char*)url);
    return rc;
  };
  
  if (0 == strncasecmp("srbp", url+match[1].rm_so, 4)) {
    mwadr->protocol = MWSRBP;
    rc = url_decode_srbp(mwadr, url);
    return rc;
  };
  /*
  if (0 == strncasecmp("http", url+match[1].rm_so, 4)) {
    mwadr->protocol = MWHTTP;
    rc = url_decode_srbp(mwadr, url);
    return rc;
  };
  */
  Error("Unknown protocol %s in the URL \"%s\" for MidWay instance, error %d", 
	url+match[1].rm_so, url, mwadr->protocol);
  return -EPROTONOSUPPORT;
};

#ifdef OBSOLETE
/* if buffer param is NULL, we use a static buffer, but we're not thread safe. */
const char * _mw_sprintsa(struct sockaddr * sa, char * buffer)
{
  static char sbuffer[256];
  char buff[16];
  SockAddress addr;

  if (buffer == NULL) buffer = sbuffer;

  if (sa == NULL) {
    buffer[0] = '\0';
    return buffer;
  };

  memcpy(&addr.sa, sa, sizeof(SockAddress));

  switch (addr.sa.sa_family) {
    
  case 0:
    buffer[0] = '\0';
    return buffer;

  case AF_INET:
    inet_ntop(AF_INET, &addr.sin4.sin_addr, buffer, 255);
    sprintf (buff, ":%d", ntohs(addr.sin4.sin_port));
    strcat (buffer, buff);
    return buffer;

  case AF_INET6:
    inet_ntop(AF_INET6, &addr.sin6.sin6_addr, buffer, 255);
    sprintf (buff, ":%d", ntohs(addr.sin6.sin6_port));
    strcat (buffer, buff);
    return buffer;

  default:
    sprintf (buffer, "unknown family %d", addr.sa.sa_family);
    return buffer;
  };

};
#endif
