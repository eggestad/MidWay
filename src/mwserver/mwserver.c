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
 * Revision 1.2  2000/07/20 19:55:49  eggestad
 * mwMainLoop() now has an argument
 *
 * Revision 1.1.1.1  2000/03/21 21:04:31  eggestad
 * Initial Release
 *
 * Revision 1.1.1.1  2000/01/16 23:20:12  terje
 * MidWay
 *
 */

#include <MidWay.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "shared_lib_services.h"

void cleanup()
{
  mwdetach();
};

void sighandler(int sig)
{
  exit(-sig);
};

usage() 
{
  fprintf(stderr, "mwserver [-d loglevel] [-A uri] [-l logprefix] [-n name] {-s service[:function]} dynamiclibraries...\n");
  fprintf(stderr, "    loglevel is one of error, warning, info, debug, debug1, debug2\n");
  fprintf(stderr, "    uri is the address of the MidWay instance e.g. ipc://12345\n");
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

int main(int argc, char ** argv)
{
  char option;
  int rc, i ;
  char * svcname, * funcname, * tmpp;

  mwsetlogprefix("mwserver");
  
  mwsetloglevel(MWLOG_DEBUG2);

  while ((option = getopt(argc, argv, "d:l:s:A:")) != EOF) {
    
    switch (option) {
      
    case 'd':
      if      (strcmp(optarg, "error")   == 0) mwsetloglevel(MWLOG_ERROR);
      else if (strcmp(optarg, "warning") == 0) mwsetloglevel(MWLOG_WARNING);
      else if (strcmp(optarg, "info")    == 0) mwsetloglevel(MWLOG_INFO);
      else if (strcmp(optarg, "debug")   == 0) mwsetloglevel(MWLOG_DEBUG);
      else if (strcmp(optarg, "debug1")  == 0) mwsetloglevel(MWLOG_DEBUG1);
      else if (strcmp(optarg, "debug2")  == 0) mwsetloglevel(MWLOG_DEBUG2);
      else usage();

      mwlog (MWLOG_DEBUG, "mwadm client starting");
      break;

    case 'A':
      uri = strdup(optarg);
      break;

    case 'l':
      mwsetlogprefix(optarg);
      break;

    case 'n':
      servername = optarg;
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
      usage;
    }
  }

  /* all the options are parsed, whar now remain on the comamnd line is the libraries.
     If there are none complain. */
  if (optind >= argc) {
    fprintf(stderr, "No libraries specified.\n\n");
    usage();
  };
  for (i = optind; i < argc; i++ ) {
    if ((rc = add_library(argv[i])) != 0) {
      fprintf(stderr,"%s: failed to load shared library %s reason ", 
	      argv[0],argv[i], rc);
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
  rc = mwattach(uri, servername, NULL, NULL, MWSERVER);  
  printf("mwattached on uri=\"%s\" returned %d\n", uri, rc);
  
  provide_services();
  mwMainLoop(0);
  
  mwdetach();
  printf("detached\n");
}









