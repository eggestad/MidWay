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


/*
 * $Log$
 * Revision 1.1  2002/07/07 22:45:48  eggestad
 * *** empty log message ***
 *
 * 
 */

#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <stdlib.h>


#include <MidWay.h>
#include "gateway.h"
#include "pattern.h"
#include "acl.h"
#include "xmlconfig.h"

static char * RCSId UNUSED = "$Id$";

static xmlDocPtr  configdoc = NULL;
static xmlNsPtr configns = NULL;

static char * configfilename = NULL;

static int gwrole = 0;
static int clientport  = -1 ;
static char * gwdomain = NULL;

#define ERRORFILE "mwgwd.err"

/* a utility function to remove leading and trailing whilespace */
static void trim(char * p)
{
  int i = 0, j = 0, postfill;

  /*  find first non whitespace */
  for(i = 0; isspace(p[i]); i++);

  postfill=i;
  /* if leading while, move, else find end of string */
  if (i != 0) {
    while(p[i] != '\0') p[j++] = p[i++];
    /* overwrite leftover from original */
    while(j < i) p[j++] = '\0';
  }
  else 
    while(p[i++] != '\0');
  
  /* replace all trailing whitespace with \0 */
  i--;
  while(isspace(p[i])) p[i--] = '\0';

    
  return;
}

int xmlConfigLoadFile(char * configfile)
{
    FILE * ferror;
    long off;
    char * errormesg;

    LIBXML_TEST_VERSION;
    DEBUG(	  "Version of libxml is %s", LIBXML_DOTTED_VERSION);

    
    if (configdoc != NULL) xmlFreeDoc(configdoc);

    ferror = fopen(ERRORFILE, "w+");
    //    if (ferror != NULL) 
    // xmlSetGenericErrorFunc (ferror, NULL);
    
    unlink(ERRORFILE);
    DEBUG("parsing config %s", configfile);
    configdoc = xmlParseFile(configfile);
    
    
   if (configdoc != NULL) {
     DEBUG("Config parsed successfully: Doc version %s encoding %s", 
	    configdoc->version, configdoc->encoding);

   } else {

     if (ferror) {
       off = ftell(ferror);
       fseek(ferror, 0, SEEK_SET);
       DEBUG("%d bytes in errorfile", off);
       errormesg = malloc(off+1);
       fread(errormesg, 1, off, ferror);
       errormesg[off] = '\0';
       DEBUG("we failed to parse the config, errno = %d, reason %s", 
	     errno, errormesg);
       free(errormesg);
     } else {
       DEBUG("we failed to parse the config %s , errno = %d", 
	     configfile, errno);
     };
   };

   configfilename = strdup(configfile);

   if (ferror) 
     fclose(ferror);
   if (configdoc == NULL) return -1;
   return 0;
};

/* obsolete */ 
static xmlNodePtr mwConfigFindNode(xmlNodePtr start, ...)
{
  va_list ap;
  xmlNodePtr cur;
  char * tagname;
  char  buffer1[1024], buffer2[1024];
  if (start == NULL) {
    start = configdoc->children;
  };

  va_start(ap, start);
  buffer1[0] = '\0';
  buffer2[0] = '\0';
  while ((tagname = va_arg(ap, char *)) != NULL) {
    //    DEBUG2("looking for tag %s", tagname);
    strcat (buffer1, tagname);
    strcat (buffer1, "/");
    cur = start;
    while(cur != NULL) {
      
      //DEBUG2("Found toplevel tag %s type=%d ", 
      //    cur->name, cur->type, xmlNodeGetContent(cur));

      if (cur->type == XML_ELEMENT_NODE) 
	if (strcasecmp(cur->name, tagname) == 0) {
	  //DEBUG2("found a match");
	  strcat (buffer2, tagname);
	  strcat (buffer2, "/");
	  start = cur->children;;
	  break;
	};
      cur = cur->next;
    };
    if (cur == NULL) {
      DEBUG2("found %s of %s: out on 1", buffer2, buffer1);
      return NULL;
    };
  }
  if (cur) {
    DEBUG2("found %s of %s: complete", buffer2, buffer1);
    return cur;
  };
  DEBUG2("found %s of %s: out on 2", buffer2, buffer1);
  return NULL;
};

/************************************************************************
 * Functions for parsing tags 
 ************************************************************************/

static int parsepattern(char * pattern)
{
  if (strcasecmp("string", pattern) == 0) return GW_PATTERN_STRING;
  if (strcasecmp("regexp", pattern) == 0) return GW_PATTERN_REGEXP_BASIC;
  if (strcasecmp("basic", pattern) == 0)  return GW_PATTERN_REGEXP_BASIC;
  if (strcasecmp("reg_extended", pattern) == 0) return GW_PATTERN_REGEXP_EXTENDED;
  if (strcasecmp("regexp_ext", pattern) == 0)   return GW_PATTERN_REGEXP_EXTENDED;
  if (strcasecmp("glob", pattern) == 0) return GW_PATTERN_GLOB;
  return -1;
};

static int ParseClientAuthTag(xmlNodePtr node)
{
  xmlNodePtr cur;
  int rc;
  char * propvalue;

  propvalue = xmlGetProp(node, "type");
  if (!propvalue) {
    Error("type property not set on auth tag");
  };


  Warning("authentocation not yet implemented");

};

static int ParseClientACL(xmlNodePtr node)
{
  xmlNodePtr cur;
  int rc;
  char * propvalue;
  xmlChar * acl;

  int patterntype = GW_PATTERN_STRING;
  int allow_deny = 0;


  if (strcasecmp(node->name, "deny") == 0) {
    allow_deny = GW_ACL_DENY;
  } else if (strcasecmp(node->name, "allow") == 0) {
    allow_deny = GW_ACL_ALLOW;
  };
  
  propvalue = xmlGetProp(node, "pattern");
  if (propvalue) {
    patterntype = parsepattern(propvalue);
  };

  acl = xmlNodeGetContent(node);
  
  DEBUG("gwaddclientacl(%d, %s, %d)", allow_deny, acl, patterntype);
  return acl_clientadd(allow_deny, acl, patterntype);

};

/* needed? */
static int ParseDomainACL(xmlNodePtr node)
{
  xmlNodePtr cur;
  int rc;
  char * propvalue;
  xmlChar * acl;

  int pattern = GW_PATTERN_STRING;
  int allow_deny = 0;


  if (strcasecmp(node->name, "deny") == 0) {
    allow_deny = GW_ACL_DENY;
  } else if (strcasecmp(node->name, "allow") == 0) {
    allow_deny = GW_ACL_ALLOW;
  };
  
  propvalue = xmlGetProp(node, "pattern");
  if (propvalue) {
    pattern = parsepattern(propvalue);
  };

  acl = xmlNodeGetContent(node);

  DEBUG("gwaddclientacl(%d, %s, %d)", allow_deny, acl, pattern);
  acl_clientadd(allow_deny, acl, pattern);

};


static int ParseClientsTag(xmlNodePtr node)
{
  xmlNodePtr cur;
  char * propvalue;
  int rc;

  DEBUG("beginning %s tag", node->name);

  for (cur = node->children; cur != NULL; cur = cur->next) {
    
    if (cur->type == XML_TEXT_NODE) {
      DEBUG("skipping unexpected xml breadtext");
      continue;
    };
    
    if (cur->type == XML_COMMENT_NODE) {
      DEBUG("skipping xml comment");
      continue;
    };
    
    if (cur->type != XML_ELEMENT_NODE) {
      DEBUG("found unexpected xml element %d", cur->type);
      continue;
    };

    DEBUG("found %s tag %s", node->name, cur->name);

    if (strcasecmp(cur->name, "auth") == 0) {
      ParseClientAuthTag(cur);
    } else if (strcasecmp(cur->name, "deny") == 0) {
      ParseClientACL(cur);
    } else if (strcasecmp(cur->name, "allow") == 0) {
      ParseClientACL(cur);
    } else {
      Warning("ignoring unexpected tag \"%s\" under node \"%s\" in config %s", 
	    cur->name, cur->parent->name, configfilename);
    };    
  };

  return 0;
};

static int ParseRemoteDomainTag(xmlNodePtr node)
{
  xmlNodePtr cur;
  char * propvalue;
  int rc;

  DEBUG("beginning %s tag", node->name);

  propvalue = xmlGetProp(node, "name");
  if (propvalue) {
    // remote domain name    ;
  };


  for (cur = node->children; cur != NULL; cur = cur->next) {
    
    if (cur->type == XML_TEXT_NODE) {
      DEBUG("skipping unexpected xml breadtext");
      continue;
    };
    
    if (cur->type == XML_COMMENT_NODE) {
      DEBUG("skipping xml comment");
      continue;
    };
    
    if (cur->type != XML_ELEMENT_NODE) {
      DEBUG("found unexpected xml element %d", cur->type);
      continue;
    };

    DEBUG("found %s  tag %s", node->name, cur->name);

    if (strcasecmp(cur->name, "peer") == 0) {
      ;
    } else {
      Warning("ignoring unexpected tag \"%s\" under node \"%s\" in config %s", 
	    cur->name, node->name, configfilename);
    };    
  };
  return 0;

};



static int ParseDomainTag(xmlNodePtr node)
{
  xmlNodePtr cur;
  char * propvalue;
  int rc;

  DEBUG("beginning %s tag", node->name);

  for (cur = node->children; cur != NULL; cur = cur->next) {
    
    if (cur->type == XML_TEXT_NODE) {
      DEBUG("skipping unexpected xml breadtext");
      continue;
    };
    
    if (cur->type == XML_COMMENT_NODE) {
      DEBUG("skipping xml comment");
      continue;
    };
    
    if (cur->type != XML_ELEMENT_NODE) {
      DEBUG("found unexpected xml element %d", cur->type);
      continue;
    };

    DEBUG("found %s  tag %s", node->name, cur->name);

    if (strcasecmp(cur->name, "clients") == 0) {
      ParseClientsTag(cur);
    } else if (strcasecmp(cur->name, "peer") == 0) {
      rc = gw_addknownpeerhost(xmlNodeGetContent(cur));
    } else if (strcasecmp(cur->name, "remotedomain") == 0) {
      ParseRemoteDomainTag(cur);
    } else {
      Warning("ignoring unexpected tag \"%s\" under node \"%s\" in config %s", 
	    cur->name, node->name, configfilename);
    };    
  };
  return 0;

};


static int ParseGatewaysTag(xmlNodePtr node)
{
  xmlNodePtr cur;
  int rc;

  DEBUG("beginning %s tag", node->name);
  /* here in mwd we don't really parse the gateway part, only the domains
     since we must start one mwgwd for each domain */
  
  for (cur = node->children; cur != NULL; cur = cur->next) {
    
     if (cur->type == XML_TEXT_NODE) {
      DEBUG("skipping unexpected xml breadtext");
      continue;
    };

    if (cur->type == XML_COMMENT_NODE) {
      DEBUG("skipping xml comment");
      continue;
    };

    if (cur->type != XML_ELEMENT_NODE) {
      DEBUG("found unexpected xml element %d", cur->type);
      continue;
    };

    DEBUG("found %s  tag %s", node->name, cur->name);

    if (strcasecmp(cur->name, "clients") == 0) {
      char * port;
      int p;

      port = xmlGetProp(cur, "port");
      if (port) {
	p = atoi(port);
      } else {
	Error("Missing port property in config tree: MidWay->mwgwd->clients");
	continue;
      }
      /* we found our config data, parse, and we're done */
      if ( (gwrole ==  GWROLE_STANDALONE) && (p == clientport) ) {
	ParseClientsTag(cur);
	return 0;
      };
      
    } else if (strcasecmp(cur->name, "domain") == 0) {
      char * domain;
      
      domain = xmlGetProp(cur, "name");
      if (domain == NULL) {
	Error("Missing name property in config tree: MidWay->mwgwd->domain");
	continue;
      }
      
      /* we found our config data, parse, and we're done */
      if ( (gwrole = GWROLE_DOMAIN) && (strcmp(domain, gwdomain) == 0) ) {
	ParseDomainTag(cur);
	return 0;
      };

    } else {
      Warning("ignoring unexpected tag \"%s\" under node \"%s\" in config %s", 
	    cur->name, cur->parent->name, configfilename);
    };    
  };
  
  /* if we got here we found no config data for us. */
  return -1;
};

static int ParseMidWayTag(xmlNodePtr node)
{
  int rc = -1;
  xmlNodePtr cur;
  DEBUG("beginning %s tag", node->name);

  for (cur = node->children; cur != NULL; cur = cur->next) {
    
     if (cur->type == XML_TEXT_NODE) {
      DEBUG("skipping unexpected xml breadtext");
      continue;
    };

    if (cur->type == XML_COMMENT_NODE) {
      DEBUG("skipping xml comment");
      continue;
    };

    if (cur->type != XML_ELEMENT_NODE) {
      DEBUG("found unexpected xml element %d", cur->type);
      continue;
    };

    DEBUG("found %s  tag %s", node->name, cur->name);

    if (strcasecmp(cur->name, "mwgwd") == 0) rc = ParseGatewaysTag(cur);
    else {
      Warning("ignoring unexpected tag \"%s\" under node \"%s\" in config %s", 
	    cur->name, cur->parent->name, configfilename);
    };    
  };
  return rc;
};

/* the role tell us if we're a standalone client only server or if we
   provide a domain gw */
int xmlConfigParseTree(int role, char * domainname, int port)
{
  int rc = -1; 
  xmlNodePtr cur;

  DEBUG("***** beginning traversing tree");

  if (configdoc == NULL) return -1;

  for (cur = configdoc->children; cur != NULL; cur = cur->next) {
    
    if (cur->type == XML_TEXT_NODE) {
      DEBUG("skipping unexpected xml breadtext");
      continue;
    };

    if (cur->type == XML_COMMENT_NODE) {
      DEBUG("skipping xml comment");
      continue;
    };

    if (cur->type != XML_ELEMENT_NODE) {
      DEBUG("found unexpected xml element %d", cur->type);
      continue;
    };

    gwrole = role;
    clientport = port;
    gwdomain = domainname;

    DEBUG("found top level tag %s", cur->name);
    if (strcasecmp(cur->name, "midway") == 0) rc = ParseMidWayTag(cur);
    else {
      Warning("ignoring unexpected tag \"%s\" in config %s", 
	    cur->name, configfilename);
    };    
  };
  DEBUG("***** completed  traversing tree");
  return rc;
};


