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
 * Revision 1.2  2001/10/04 19:18:10  eggestad
 * CVS tags fixes
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <string.h>
#include <errno.h>

#include <MidWay.h>

struct service {
  char * svcname;
  char * funcname;
  void * libhandle;
  int (*func) (mwsvcinfo *);
  struct service * next;
};
static struct service * services = NULL;

struct library {
  char * libname;
  void * handle;
  struct library * next;
};
static struct library * libraries = NULL;

int add_service(char * svcname, char * funcname)
{
  struct service * tmpsvc, * psvc;
  

  if (svcname == NULL) return -EINVAL;

  /* test for duplicate service names.
     in the end psvc is pointing to the last elemnt in the list.
     is the list is empty psvc is NULL. */
  psvc = NULL;
  tmpsvc = services;
  while(tmpsvc != NULL) {
    if (strcmp(tmpsvc->svcname, svcname) == 0) return -EEXIST;
    psvc = tmpsvc;
    tmpsvc = tmpsvc->next;
  };

  /* create a new element, init it. */
  tmpsvc = malloc(sizeof(struct service));
  tmpsvc->svcname = svcname;
  tmpsvc->funcname = funcname;
  tmpsvc->libhandle = NULL;
  tmpsvc->func = NULL;
  tmpsvc->next = NULL;
  
  /* add it to the list. */
  if (psvc == NULL) 
    services = tmpsvc;
  else
    psvc->next = tmpsvc;
  return 0;
};

char * wd;

int add_library(char * libpath)
{
  struct library * tmplib, * plib;

  if (wd == NULL ) {
    wd = malloc(4096);
    getcwd(wd, 4090);
  };

  if (libpath == NULL) return -EINVAL;
  
  /* test for duplicate libraries.
     in the end plib is pointing to the last element in the list.
     is the list is empty plib is NULL. */
  plib = NULL;
  tmplib = libraries;
  while(tmplib != NULL) {
    if (strcmp(tmplib->libname, libpath) == 0) return -EEXIST;
    plib = tmplib;
    tmplib = tmplib->next;
  };

  /* create a new element, init it. */
  tmplib = malloc(sizeof(struct library));
  tmplib->libname = malloc(4100);
  sprintf(tmplib->libname, "%s/%s", wd, libpath);
  tmplib->handle = dlopen(tmplib->libname,RTLD_NOW);
  if (tmplib->handle == NULL) {
    fprintf(stderr, "failed to open library %s, reason %s", tmplib->libname, dlerror());
    return -ELIBACC;
  };
  tmplib->next = NULL;
  
  /* add it to the list. */
  if (plib == NULL) 
    libraries = tmplib;
  else
    plib->next = tmplib;
  return 0;
};

int provide_services()
{
  struct service * tmpsvc;   
  struct library * tmplib;;

  tmpsvc = services;
  
  /* now we resolve all service function references and 
     call mwprovide() on all of them.*/

  while(tmpsvc != NULL) {
    
    if (tmpsvc->funcname == NULL) tmpsvc->funcname = tmpsvc->svcname;
    mwlog(MWLOG_DEBUG, "For service %s we are looking for symbol %s", 
	  tmpsvc->svcname, tmpsvc->funcname);
	  
    tmplib = libraries;
    while (tmplib != NULL) {
      tmpsvc->func = dlsym(tmplib->handle, tmpsvc->funcname);
      if (tmpsvc->func != NULL) {
	mwlog(MWLOG_INFO, "For service %s we are using symbol %s in library %s", 
	       tmpsvc->svcname, tmpsvc->funcname, tmplib->libname);
	tmpsvc->libhandle = tmplib->handle;
	break;
      } else 
	mwlog(MWLOG_DEBUG, "For service %s we didn't find symbol %s in library %s", 
	      tmpsvc->svcname, tmpsvc->funcname, tmplib->libname);	
      tmplib = tmplib->next;
    };

    if (tmpsvc->func == NULL) {
      mwlog(MWLOG_ERROR, "Failed to resolve the symbol %s for service %s", 
	    tmpsvc->funcname, tmpsvc->svcname);
    } else {
      mwprovide (tmpsvc->svcname, tmpsvc->func, 0);
    }

    tmpsvc = tmpsvc->next;
  };
  
  return 0;
};
