/*
  MidWay
  Copyright (C) 2001 Terje Eggestad

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


#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <stdlib.h>


#include <MidWay.h>
#include "mwd.h"
#include "xmlconfig.h"
#include "servermgr.h"

static char * RCSId UNUSED = "$Id$";
static char * RCSName UNUSED = "$Name$"; /* CVS TAG */

static xmlDocPtr  configdoc = NULL;
//static xmlNsPtr configns = NULL;

static char * configfilename = NULL;

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

    ferror = fopen("mwd.err", "w+");
    //    if (ferror != NULL) 
    // xmlSetGenericErrorFunc (ferror, NULL);
    
    unlink("mwd.err");
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

/*
int mwConfigLoadFile2(char * configfile)
{
  int i = 0;

  if (configdoc != NULL) xmlFreeDoc(configdoc);
  FILE * fxml, * ferror;
  int rc, size;
  char chunk[1024];
  xmlParserCtxtPtr ctxt;

   size = 1024;
   ferror = fopen("mwd.err", "w");
   xmlSetGenericErrorFunc (ferror, NULL);


   fxml = fopen(configfeil, "r");

   if (fxml == NULL ) exit(11);
   
   rc = fread(chunk, 1, 4, fxml);
   if (rc > 0) {
     ctxt = xmlCreatePushParserCtxt(NULL, NULL,
				    chunk, rc, argv[1]);
     while ((rc = fread(chunk, 1, size, fxml)) > 0) {
       rc = xmlParseChunk(ctxt, chunk, rc, 0);

       if (rc) {
	 //xmlParserError                  (ctxt, "hei hei", NULL);
       }

     }
     rc = xmlParseChunk(ctxt, chunk, 0, 1);
     doc = ctxt->myDoc;
     xmlFreeParserCtxt(ctxt);
   }

   
   if (configdoc != NULL) {
     DEBUG(" Global tag is %s children %s", 
	    configdoc->name, configdoc->children->name);
     DEBUG(" Doc version %s encoding %s", 
	    configdoc->version, configdoc->encoding);

     //      parseInstance(configdoc, mw_config_ns, configdoc->children);
   } else {
     DEBUG("we failed to parse the config, errno = %d", errno);
   };
   
   return configdoc != NULL;
};
*/

xmlNodePtr mwConfigFindNode(xmlNodePtr start, ...)
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

static int ParseEnvTag(xmlNodePtr node, int servergroup)
{
  xmlNodePtr cur;
  char * envs;
  int envslength;
  char * name;
  char * value;
  int i, rc;
  int in, iv, inval;

  for (cur = node->children; cur != NULL; cur = cur->next) {

    if (cur->type == XML_COMMENT_NODE) {
       DEBUG("skipping xml comment");
       continue;
    };
    
    if (cur->type == XML_ELEMENT_NODE) {
       DEBUG("found unexpected xml element %d", cur->type);
       continue;
    };

    
    if (cur->type != XML_TEXT_NODE) {
       Warning("ignoring unexpected tag \"%s\" under node \"%s\" in config %s", 
	       cur->name, cur->parent->name, configfilename);
    };

    envs = XML_GET_CONTENT(cur);
    DEBUG("envs are %s", envs);
    
    envslength = strlen(envs);
    name = malloc(envslength);
    value = malloc(envslength);
    name[0] = '\0';
    value[0] = '\0';
    in = iv = inval = 0;
    for (i = 0; i < envslength; i++) {
       if (envs[i] == '\n') {
	  if (in == 0) {
	     continue;
	  };
	  name[in++] = '\0';
	  value[iv++] = '\0';	  
	  DEBUG("  env par \"%s\" = \"%s\"", name, value);
	  if (servergroup) 
	     rc = smgrSetGroupSetEnv(name, value);
	  else 
	     rc = smgrSetServerSetEnv(name, value);
	  if (rc != 0) {
	     Warning("setenv failed with errno %d", errno);
	  };
	  in = iv = inval = 0;
	  name[0] = '\0';
	  value[0] = '\0';    
	  continue;
       }
       if ((envs[i] == '=') && (!inval)) {
	  inval = 1;
	  continue;
       }
       if (inval) 
	  value[iv++] = envs[i];
       else {
	  if (isspace(envs[i])) continue;
	  name[in++] = envs[i];
       };
    };

    // in case there is no new line at the end.
    if (in != 0) {
       name[in++] = '\0';
       value[iv++] = '\0';	  
       DEBUG("  last env par \"%s\" = \"%s\"", name, value);
       if (servergroup) 
	  smgrSetGroupSetEnv(name, value);
       else 
	  smgrSetServerSetEnv(name, value);
    };
  };
  return 1;
};

static int ParseExecTag(xmlNodePtr node)
{
  xmlNodePtr cur;
  char * exec;

  for (cur = node->children; cur != NULL; cur = cur->next) {
    
 
    if (cur->type != XML_TEXT_NODE) {
      DEBUG("found unexpected xml element %d", cur->type);
      continue;
    };

    exec = xmlNodeGetContent (cur);
    trim(exec);

    DEBUG("   exec for this server is %s", exec);
    smgrSetServerExec(exec);
  };
};

static int ParseArgListTag(xmlNodePtr node)
{
  xmlNodePtr cur;
  char * arglist;

  for (cur = node->children; cur != NULL; cur = cur->next) {
    
 
    if (cur->type != XML_TEXT_NODE) {
      DEBUG("found unexpected xml element %d", cur->type);
      continue;
    };

    arglist = xmlNodeGetContent (cur);
    trim(arglist);

    DEBUG("   arglist for this server is \"%s\"", arglist);
    smgrSetServerArgs(arglist);
  };
};

static int ParseServerInstancesTag(xmlNodePtr node)
{
  char * propvalue;
  int min = 1, max = 1;

  propvalue = xmlGetProp(node, "min");
  if (propvalue) {
    min = atoi(propvalue);
  };
  propvalue = xmlGetProp(node, "max");
  if (propvalue) {
    max = atoi(propvalue);
  };

  DEBUG("smgrSetServerMinMax(min = %d,  max = %d)", min, max);
  smgrSetServerMinMax(min, max);
};

static int ParseServerTag(xmlNodePtr node)
{
  xmlNodePtr cur;
  char * propvalue;
  char * servername;

  DEBUG("*** beginning server %s tag", node->name);

  servername = xmlGetProp(node, "name");
  if (servername == NULL) return -1;

  smgrBeginServer(servername);

  propvalue = xmlGetProp(node, "autoboot");
  if (propvalue) {
    DEBUG("autoboot = %s", propvalue);
    if (strcasecmp(propvalue, "yes") == 0) 
      smgrSetServerAutoBoot(TRUE);
    else if (strcasecmp(propvalue, "no") == 0) 
      smgrSetServerAutoBoot(atoi(propvalue));
    else 
      smgrSetServerAutoBoot(atoi(propvalue));
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

    /* Now we do subtags */
    DEBUG("found %s tag %s", node->name, cur->name);

    if (strcasecmp(cur->name, "exec") == 0) {
      ParseExecTag(cur);
    } else if (strcasecmp(cur->name, "arglist") == 0) {
      ParseArgListTag(cur);
    } else if (strcasecmp(cur->name, "instances") == 0) {
      ParseServerInstancesTag(cur);
    } else if (strcasecmp(cur->name, "env") == 0) {
      ParseEnvTag(cur, 0);
    } else {
      Warning("ignoring unexpected tag \"%s\" under node \"%s\" in config %s", 
	    cur->name, cur->parent->name, configfilename);
    };    
  };
  DEBUG("*** ending  server %s tag", node->name);
  smgrEndServer();
  return 0;
};

static int ParseGroupTag(xmlNodePtr node)
{
  xmlNodePtr cur;
  char * propvalue;
  char * groupname;

  DEBUG("beginning %s tag", node->name);

  groupname = xmlGetProp(node, "name");
  smgrBeginGroup(groupname);

  propvalue = xmlGetProp(node, "autoboot");
  if (propvalue) {
    DEBUG("autoboot = %s", propvalue);
    if (strcasecmp(propvalue, "yes") == 0) 
      smgrSetGroupAutoBoot(TRUE);
    else if (strcasecmp(propvalue, "no") == 0) 
      smgrSetGroupAutoBoot(atoi(propvalue));
    else 
      smgrSetGroupAutoBoot(atoi(propvalue));
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

    DEBUG("found %s tag %s", node->name, cur->name);

    if (strcasecmp(cur->name, "Server") == 0) {
      ParseServerTag(cur);
    } else if (strcasecmp(cur->name, "env") == 0) {
      ParseEnvTag(cur, 1);
    } else {
      Warning("ignoring unexpected tag \"%s\" under node \"%s\" in config %s", 
	    cur->name, cur->parent->name, configfilename);
    };    
  };
  smgrEndGroup();
  return 0;
};

static int ParseServersTag(xmlNodePtr node)
{
  xmlNodePtr cur;
  char * propvalue;

  DEBUG("beginning %s tag", node->name);

  propvalue = xmlGetProp(node, "autoboot");
  if (propvalue) {
    DEBUG("autoboot = %s", propvalue);
    if (strcasecmp(propvalue, "yes") == 0) 
      smgrSetDefaultAutoBoot(TRUE);
    else if (strcasecmp(propvalue, "no") == 0) 
      smgrSetDefaultAutoBoot(atoi(propvalue));
    else 
      smgrSetDefaultAutoBoot(atoi(propvalue));
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

    if (strcasecmp(cur->name, "group") == 0) ParseGroupTag(cur);
    else if (strcasecmp(cur->name, "Server") == 0) {
      smgrBeginGroup("");
      ParseServerTag(cur);
      smgrEndGroup();
    } else {
      Warning("ignoring unexpected tag \"%s\" under node \"%s\" in config %s", 
	    cur->name, node->name, configfilename);
    };    
  };
  return 0;

};

static int ParseMWDTag(xmlNodePtr node)
{
  xmlNodePtr cur;
  char * propvalue;

  DEBUG("beginning %s tag", node->name);

  propvalue = xmlGetProp(node, "clients");
  if (propvalue) {
    DEBUG("maxclients = %d", atoi(propvalue));
    mwdSetIPCparam(MAXCLIENTS, atoi(propvalue));
  };

  propvalue = xmlGetProp(node, "servers");
  if (propvalue) {
    DEBUG("maxservers = %d", atoi(propvalue));
    mwdSetIPCparam(MAXSERVERS, atoi(propvalue));
  };

  propvalue = xmlGetProp(node, "services");
  if (propvalue) {
    DEBUG("maxservices = %d", atoi(propvalue));
    mwdSetIPCparam(MAXSERVICES, atoi(propvalue));
  };

  propvalue = xmlGetProp(node, "gateways");
  if (propvalue) {
    DEBUG("maxgateways = %d", atoi(propvalue));
    mwdSetIPCparam(MAXGATEWAYS, atoi(propvalue));
  };

  propvalue = xmlGetProp(node, "ipckey");
  if (propvalue) {
    DEBUG("ipckey = %d", atoi(propvalue));
    mwdSetIPCparam(MASTERIPCKEY, atoi(propvalue));
  };

  propvalue = xmlGetProp(node, "basebuffersize");
  if (propvalue) {
    DEBUG("basebuffersize = %d", atoi(propvalue));
    mwdSetIPCparam(BUFFERBASESIZE, atoi(propvalue));
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

    if (strcasecmp(cur->name, "Servers") == 0) ParseServersTag(cur);
    else {
      Warning("ignoring unexpected tag \"%s\" under node \"%s\" in config %s", 
	    cur->name, cur->parent->name, configfilename);
    };    
  };
  return 0;
};

static int ParseGatewaysTag(xmlNodePtr node)
{
  xmlNodePtr cur;
  char name[128];
  char args[256];
  char * extraopts;

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

    smgrBeginGroup("MWGATEWAYS");      

    if (strcasecmp(cur->name, "env") == 0) {
      ParseEnvTag(cur, 1);

    } else if (strcasecmp(cur->name, "clients") == 0) {
      char * port;
      int p;

      port = xmlGetProp(cur, "port");
      if (port) {
	p = atoi(port);
      } else {
	Error("Missing port property in config tree: MidWay->mwgwd->clients");
	continue;
      }

      extraopts = xmlGetProp(cur, "options");
      if (extraopts) {
	 strcat(args, extraopts);
      } else {
	 extraopts = "";
      };


      sprintf(name, "mwgwd-clients-%d", p);
      smgrBeginServer(name);

      smgrSetServerExec("mwgwd");
      smgrSetServerAutoBoot(TRUE);
      sprintf(args, "-p %d %s", p, extraopts);

      smgrSetServerArgs(args);
      smgrSetServerMinMax(1, 1);

      DEBUG("Added gateway  %s", name);

      smgrEndServer();
      
    } else if (strcasecmp(cur->name, "domain") == 0) {
      char * domain;

      domain = xmlGetProp(cur, "name");
      if (domain == NULL) {
	Error("Missing name property in config tree: MidWay->mwgwd->domain");
	continue;
      }

      extraopts = xmlGetProp(cur, "options");
      if (extraopts) {
	 strcat(args, extraopts);
      } else {
	 extraopts = "";
      };

      sprintf(name, "mwgwd-domain-%s", domain);
      smgrBeginServer(name);

      smgrSetServerExec("mwgwd");
      smgrSetServerAutoBoot(TRUE);
      sprintf(args, "%s %s", extraopts, domain);

      smgrSetServerArgs(args);
      smgrSetServerMinMax(1, 1);

      DEBUG("Added gateway  %s", name);

      smgrEndServer();

    } else {
      Warning("ignoring unexpected tag \"%s\" under node \"%s\" in config %s", 
	    cur->name, cur->parent->name, configfilename);
    };    

    smgrEndGroup();


  };
  
  return 0;
};

static int ParseMidWayTag(xmlNodePtr node)
{
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

    if (strcasecmp(cur->name, "mwd") == 0) ParseMWDTag(cur) ;
    else if (strcasecmp(cur->name, "mwgwd") == 0) ParseGatewaysTag(cur);
    else {
      Warning("ignoring unexpected tag \"%s\" under node \"%s\" in config %s", 
	    cur->name, cur->parent->name, configfilename);
    };    
  };
  return 0;
};

int xmlConfigParseTree(void)
{
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

    DEBUG("found top level tag %s", cur->name);
    if (strcasecmp(cur->name, "midway") == 0) ParseMidWayTag(cur);
    else {
      Warning("ignoring unexpected tag \"%s\" in config %s", 
	    cur->name, configfilename);
    };    
  };
  DEBUG("***** completed  traversing tree");
  return 0;
};


