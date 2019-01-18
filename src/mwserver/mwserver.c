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

#include "shared_lib_services.h"
#include <ipctables.h>


#ifndef PATH_MAX
dddd
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
     char abspath[PATH_MAX];

     char * bname = strdup(argv[i]);
     bname = basename(bname);
     char * dname = strdup(argv[i]);
     dname = dirname(dname);

     printf("arg %s\n",argv[i]);
     printf("bsn %s\n",bname);
     printf("din %s\n",dname);

     if (dirname == NULL) {
	snprintf(abspath, PATH_MAX, "%s/%s", startdir, bname);
	printf("___ %s\n", abspath);

     } else if (dname[0] == '/') {
	strncpy(abspath, argv[i], PATH_MAX);
	printf("abs %s\n", abspath);
	
     } else {
	snprintf(abspath, PATH_MAX, "%s/%s", startdir, argv[i]);
	printf("rel %s\n", abspath);
     }

     printf("%s\n", abspath);
     
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









