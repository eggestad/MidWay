/*
  MidWay
  Copyright (C) 2002 Terje Eggestad

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



#define _GNU_SOURCE  

#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include <fnmatch.h>
#include <errno.h>

#include <MidWay.h>
#include "gateway.h"
#include "acl.h"
#include "pattern.h"

/* the client Access Control List. 

   The list is parsed in reverse order during test, and all adds is
   pushed at the end.  Any deny/allow match is taken, there is no test
   all. THere is an implicit glob: [!._]* at the top which allows all
   service calls that don't begin with a  '.' or '_'.

   e.g. (using only globs):

   list
   pos   glob    
   ----------------------
   [-1] "*"      deny    (just implicit)
   [0]  "[!._]*" allow   (implicityly added first in initacl)
   [1]  "mw*"    deny
   [2]  "mwtest" allow

   
   now if client calls XYZ, the [glob in [2] don't match, neither do
   [1], but [0] do and the call is allowed.

   If ".xyz" is called neither [2], [1], nor [0] matches that the call
   is denied (with ENOENT, not EPERM).
   
   A call to mwadmin would not match [2], but match [1] and is denied. 

   capiche?
   

   of couse the pattern may be a string (no wild cards) a basic nor
   extended regexp (see regex(7)), or the glob (globs are the shell
   file wild card matches. see glob(7). (NOTE: (t)csh uses ^ not ! as
   the [] negate) 

*/

  
struct acl 
{ 
  char * pattern; 
  int pattern_type; 
  int icase; 
  int allow_deny; 
  regex_t regexp;
};

static struct acl * acllist = NULL;
static int aclcount = 0;

static void initacl(void)
{
  if (acllist != NULL) return;
  
  acllist = malloc(sizeof(struct acl));

  /* the implisit last */
  acllist[0].pattern = "[a-z]*";
  acllist[0].pattern_type = GW_PATTERN_GLOB;
  acllist[0].icase = 0;
  acllist[0].allow_deny = GW_ACL_ALLOW;
  
  aclcount = 1;
};

int acl_clientadd(int allow_deny, char * pattern, int patterntype)
{
  int rc, errbuflen = 1024;
  char errbuf[1024];

  DEBUG(" begin allow/deny = %d, pattern %s, pattertype = %d", 
	allow_deny, pattern, patterntype);

  if (pattern == NULL) {
    Error("Internal error: got a call to acl_clientadd() with NULL pattern");
    errno = EINVAL;
    return -1;
  };

  if ( (patterntype != GW_ACL_ALLOW) && (patterntype != GW_ACL_ALLOW) ) {
    Error("Internal error: got a call to acl_clientadd() with illegal allow/deny flag %d",
	  allow_deny);
    errno = EINVAL;
    return -1;
  };

  acllist = realloc(acllist, sizeof(struct acl) * (aclcount+1));

  acllist[aclcount].pattern = strdup(pattern);
  acllist[aclcount].icase = 0;
  acllist[aclcount].allow_deny = allow_deny;

  switch (patterntype) {
    
  case  GW_PATTERN_STRING:
    acllist[aclcount].pattern_type = GW_PATTERN_STRING;

    break;

  case	GW_PATTERN_GLOB:
    rc = fnmatch(pattern, "dummy string", 0);
    /* if rc == 0 (a match) or FNM_MATCH pattern is a glob pattern,
       else complain. We do it here so we avoid an error for every
       service call later */
    if ( (rc != 0) && (rc != FNM_NOMATCH) ) {
      Error("attempted to add pattern %s to client acl, but pattern is not a glob", 
	    pattern);
      
      return -1;
    };
    acllist[aclcount].pattern_type = GW_PATTERN_GLOB;
    break;

    /* the regexp are compiled at this point to save time at test, sp
       we don't have to compile it for every call. A bit unsude of the
       memory consumption, but I declare it irrelevat compres to
       speed. */

  case	GW_PATTERN_REGEXP_BASIC:
    acllist[aclcount].pattern_type = GW_PATTERN_REGEXP_BASIC;
    rc = regcomp( &acllist[aclcount].regexp, pattern, REG_NOSUB);
    if (rc != 0) {
       regerror(rc, &acllist[aclcount].regexp, errbuf, errbuflen);     
       Error("failed to compile regular expression (basic) %s, reason %s", 
	     pattern , errbuf);
       regfree(&acllist[aclcount].regexp);
       return -1;
    };

  case	GW_PATTERN_REGEXP_EXTENDED:
    acllist[aclcount].pattern_type = GW_PATTERN_REGEXP_EXTENDED;
    rc = regcomp( &acllist[aclcount].regexp, pattern, REG_EXTENDED|REG_NOSUB);
    if (rc != 0) {
       regerror(rc, &acllist[aclcount].regexp, errbuf, errbuflen);     
       Error("failed to compile regular expression (extended) %s, reason %s", 
	     pattern , errbuf);
       regfree(&acllist[aclcount].regexp);
       return -1;
    };
    break;

  default:

    Error("Internal error: got a call to acl_clientadd() with unknown patterntype %d",
	  patterntype);
    errno = EFAULT;
    return -1;
  }

  DEBUG("added pattern %s to acl pos %d %s", 
	pattern, aclcount, 
	allow_deny == GW_ACL_ALLOW ? "ALLOW" : "DENY");
  aclcount++;
  return 0;
};


int acl_testservice(char * service)
{
  int i;

  DEBUG("testing service %s agains %d acls", service, aclcount);
  for (i = aclcount-1; i >= 0; i++) {
    
    DEBUG("  testing service %s agains pattern #%d", service, i);
    switch (acllist[i].pattern_type) {
      
    case  GW_PATTERN_STRING:
      if (acllist[i].icase) {
	if (strcasecmp(service, acllist[i].pattern) == 0) 
	  return acllist[i].allow_deny;
      } else {
	if (strcmp(service, acllist[i].pattern) == 0) 
	  return acllist[i].allow_deny;
      };
      break;
      
      /* we tested in acl_clientadd() that the pattern is a globm fnmatch can't return an error */
    case  GW_PATTERN_GLOB:
      if (fnmatch(acllist[i].pattern, service, acllist[i].icase?FNM_CASEFOLD:0) == 0)
	return acllist[i].allow_deny;
      break;
      
      /* hmm, regexec can't fail, assuming I've not screwed up the code somehere */
    case GW_PATTERN_REGEXP_BASIC:
    case GW_PATTERN_REGEXP_EXTENDED:
      if ( regexec( &acllist[aclcount].regexp, service, 0, NULL, 0) == 0)
	return acllist[i].allow_deny;
      break;
      
    };
  };

  /* the implisit [-1] entry. If no pattern matched, we deny. */
  return GW_ACL_DENY;
};
