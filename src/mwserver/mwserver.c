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


#include <MidWay.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <libgen.h>
#include <fcntl.h>

#include "shared_lib_services.h"
#include <ipctables.h>


#ifndef PATH_MAX
#define PATH_MAX  4096
#endif

void cleanup(void)
{
  mwdetach();
};

void sighandler(int sig)
{
  exit(-sig);
};

void usage(void) 
{
  fprintf(stderr, "mwserver [-l loglevel] [-A uri] [-L logprefix] [-n name] [-r rundir] [-s service[:function]] dynamiclibraries...\n");
  fprintf(stderr, "    loglevel is one of error, warning, info, debug, debug1, debug2\n");
  fprintf(stderr, "    uri is the address of the MidWay instance e.g. ipc:12345\n");
  fprintf(stderr, "    logprefix is the path inclusive the beginning of the file name the logfile shall have.\n");
  fprintf(stderr, "    service:function ties a service to a function in the given libs.\n");
  fprintf(stderr, "      if :function is omitted, the function name is assumed to be service.\n");
  fprintf(stderr, "    dynamiclibraries are the .so files containing your service functions..\n");
  fprintf(stderr, "    name is the name the server will use for it self when attaching.\n");
  exit(-1);
};

extern char *optarg;
extern int optind, opterr, optopt;
char * uri = NULL;
char * servername = NULL;
char * libdir = NULL , * rundir = NULL;

static char * checklibrarypath(char * soname, char * directory){
   if (soname == NULL) return NULL;
   if (directory == NULL) directory = "";
   int solen = strlen(soname);
   int dirlen = strlen(directory);
   char * fullpath = malloc(solen + dirlen+ 10);
   if (dirlen > 0) {
      strcpy(fullpath, directory);
      if (directory[dirlen-1] != '/')
	 strcat(fullpath, "/");
   }
   strcat(fullpath, soname);
   DEBUG("Checking of abs path for lib %s %s, %s", soname, directory, fullpath);
   if (faccessat (AT_FDCWD, fullpath, X_OK, 0) == 0) {
      return fullpath;
   } else {
      DEBUG("access failed reason %d", errno);
      free(fullpath);
      return NULL;
   }
}
   
static char * search_for_shared_library(char * soname) {
   if (soname == NULL || strlen(soname) == 0) return NULL;
   if (soname[0] == '/') {
      DEBUG("given library have absolute path %s, skipping search paths", soname);
      return checklibrarypath(soname, NULL);
   }

   char * path = checklibrarypath(soname, NULL);
   if (path != NULL) return path;
   path = checklibrarypath(soname, NULL);
   if (path != NULL) return path;
   path = checklibrarypath(soname, libdir);
   if (path != NULL) return path;

   char * ldlp = getenv ("LD_LIBRARY_PATH");
   if (ldlp != NULL && strlen(ldlp) > 0) {
      ldlp = strdup(ldlp);
      DEBUG("looking thru LD_LIBARAY_PATH");
      char * rest = ldlp;
      char * thisdir;
      do {	 
	 thisdir = strsep(&rest, ":");
	 path = checklibrarypath(soname, thisdir);
	 if (path != NULL) break;
	 // if rest is NULL we're at the last entry in the search path
	 if (rest == NULL) {
	    path = checklibrarypath(soname, thisdir);
	    break;
	 }
      } while (rest != NULL);
      free (ldlp);
      if (path != NULL) return path;
   }
   return NULL;
}
   
   
int main(int argc, char ** argv)
{
  char option;
  int rc, i, loglevel;
  char * svcname, * funcname, * tmpp, * logprefix = "mwserver";
  char  * mwhome, * instance;
  ipcmaininfo * ipcmain;

  char startdir[PATH_MAX];
  getcwd(startdir, PATH_MAX);  

#ifdef DEBUGGING
  loglevel = MWLOG_DEBUG2;
#else 
  loglevel = MWLOG_INFO;
#endif

  while ((option = getopt(argc, argv, "l:L:s:A:r:")) != EOF) {
    
    switch (option) {
      
    case 'l':
       i = _mwstr2loglevel(optarg);
       if (i == -1) 
	  usage();
       loglevel = i;
       DEBUG("mwserver client starting");
       break;

    case 'A':
      uri = strdup(optarg);
      break;

    case 'L':
      logprefix = optarg;
      break;

    case 'n':
      servername = optarg;
      break;

    case 'r':
      rundir = optarg;
      break;

    case 's':
      svcname = optarg;
      funcname = NULL;
      tmpp = strchr(optarg,':');
      if (tmpp != NULL) {
	funcname = tmpp+1;
	*tmpp = '\0';
      };
      if (add_service(svcname, funcname)) {
	fprintf(stderr, "Duplicate service %s\n", svcname);
	exit(-EEXIST);
      };
      break;

    case '?':
      usage();
    }
  }

  mwopenlog(argv[0], logprefix, loglevel);


  rc = mwattach(uri, servername, MWSERVER);  
  DEBUG("mwattached on uri=\"%s\" returned %d\n", uri, rc);
  if (rc < 0) {
    Error("attached failed");
    exit(rc);
  };



  mwhome = getenv ("MWHOME");
  instance = getenv ("MWINSTANCE");
  
  DEBUG("MWHOME = %s", mwhome);
  DEBUG("MWINSTANCE = %s", instance);
  
  ipcmain = _mw_ipcmaininfo();
  
  if (mwhome == NULL) mwhome = ipcmain->mw_homedir;
  if (instance == NULL) instance = ipcmain->mw_instance_name;
  
  
  DEBUG("home = %s", mwhome);
  DEBUG("instance = %s", instance);

  i = strlen(mwhome)+strlen(instance)+10;
  libdir = malloc(i);
  
  sprintf(libdir, "%s/%s/lib", mwhome, instance);
  if (rundir == NULL) {
     rundir = malloc(i);
     sprintf(rundir, "%s/%s/run", mwhome, instance);
  }
  
  DEBUG("libdir = %s", libdir);
  DEBUG("rundir = %s", rundir);

  rc = access(libdir, R_OK|X_OK);
  if (rc != 0) Fatal("rx access failed of lib directory %s", libdir);

  rc = access(rundir, R_OK|W_OK|X_OK);
  if (rc != 0) Fatal("rx access failed of run directory %s", rundir); 

  rc = chdir (rundir);
  if (rc != 0) Error("failed to change dir to %s", rundir);
  else Info ("changed dir to %s", rundir);
  /* all the options are parsed, whar now remain on the comamnd line is the libraries.
     If there are none complain. */
  if (optind >= argc) {
    Error("No libraries specified.");
    usage();
  };

  for (i = optind; i < argc; i++ ) {
     printf("arg %s\n",argv[i]);
     
     char * abspath = search_for_shared_library(argv[i]) ;
  
     if (abspath == NULL) {
	Error("%s: failed to locate shared library %s", 
	      argv[0],argv[i]);
	exit(rc);
     };
     if ((rc = add_library(abspath)) != 0) {
	Error("%s: failed to load shared library %s reason %s", 
	      argv[0],argv[i], strerror(-rc));
	exit(rc);
     };
  };
  
  signal(SIGTERM, sighandler);
  signal(SIGQUIT, sighandler);
  signal(SIGINT, sighandler);

  atexit(cleanup);

  if (servername == NULL) {
    servername = (char *) malloc(20);
    sprintf(servername, "([%d])", getpid());
  };

  provide_services();
  mwMainLoop(0);
  
  mwdetach();
  DEBUG("detached\n");
}









