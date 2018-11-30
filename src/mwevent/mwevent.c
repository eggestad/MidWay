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


#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <errno.h>

#include <MidWay.h>

char * inputfile = NULL;
char * outputfile = NULL;
FILE * fp = NULL;

char * logfile = NULL;

#define PROG "mwevent"
char * prog = PROG;

// for attach
char * myclientname = PROG;
char * username = NULL;
char * password = NULL;

// for event
char * url = NULL;
char * user = NULL;
char * client = NULL;

int mode = 0;

/* undocumented func in lib/mwlog.c */
void _mw_copy_on_stdout(int flag);
void _mw_copy_on_stderr(int flag);

int subflag = MWEVSTRING;

void eventhdl(const char * eventname, const char * data, size_t datalen) 
{
   fprintf (fp, "%s %*.*s\n", eventname, (int) datalen, (int) datalen, data);
};

int watchevents(int argc, char ** argv) 
{
   int i;

   for (i = 0; i < argc; i++) {
      mwsubscribeCB(argv[i], subflag, eventhdl);
   };

   do {
      sleep(1);
      mwrecvevents();
   } while(1);
};




int postevent(int argc, char ** argv) 
{
   size_t len = 0;
   int rc = 0; 
  char * data = NULL; 

  if (argv[0] == NULL) {
    return -EINVAL;
  }

  /* if data on commandline */
  if (argv[1] != NULL) {
    if (strcmp(argv[1], "-") == 0) {
      DEBUG("about to read input data from stdin");
      exit(9);
    } else {
      DEBUG("about to read input data from commandline");
      len = strlen(argv[1]);
      data = (char *) malloc(len+1);
      strcpy(data, argv[1]);
    }
  } 

  if ( (data == NULL) && (inputfile)) {
    struct stat if_stat;
    int toread, readed = 0;
    int fd;

    DEBUG("about to read input data from %s", inputfile);

    rc = stat(inputfile, &if_stat);
    if (rc != 0) {
      perror("Failed to stat inputfile:");
      exit(-1);
    };
    fd = open(inputfile, O_RDONLY);
    if (fd != 0) {
      perror("Failed to open inputfile:");
      exit(-1);
    };
    
    toread = if_stat.st_size;
    data = (char *) malloc(if_stat.st_size+1);

    // this generate an "comparison is always false due to limited
    // range of data type" while compilingon 64bit platforms, which is ok.
    while(toread > SSIZE_MAX) {
      readed += read (fd, data + readed, SSIZE_MAX);
      toread -= toread;
    };
    readed += read (fd, data + readed, toread);
    
    close(fd);
    len = readed;
  };
  

  DEBUG("about to send event (%s, %s, %zu, ...)", argv[0], data, len);

  rc = mwevent(argv[0], data, len, user, client);
  
  DEBUG("send event => %d", rc);

  return 0;
};


void usage(int rc) 
{
   if (rc != 2) {
      fprintf (stderr, "usage: %s [-l debuglevel] [-L logfile] [-i inputfile] [-o outputfile] [-u user] [-c client] [-A URL] event [data|-]\n", prog);
      fprintf (stderr, "       -A URL        : the URL to the MidWay instance (e.g. srbp://host:port or ipc:ipckey\n");
      fprintf (stderr, "       -l debuglevel : one of: error warning info debug debug1 debug2 debug3 debug4\n");
      fprintf (stderr, "       -L logfile    : all logging is placed in this file, if - stderr\n");
      fprintf (stderr, "       -i inputfile  : The data to be passed the service is read from this file\n");
      fprintf (stderr, "       -U username   : username to login to MidWay with\n");
      fprintf (stderr, "       -P password   : password for username \n");
      fprintf (stderr, "       -u user       : username to whom to send events\n");
      fprintf (stderr, "       -c client     : clientname to whom to send events \n");
      fprintf (stderr, "       event         : The name of the MidWay event\n");
      fprintf (stderr, "       data | -      : either a data string or - to use stdin\n");
   } else if (rc != 1) {
      fprintf (stderr, "usage: -S %s [-l debuglevel] [-L logfile] [-i inputfile] [-o outputfile] [-u user] [-c client] [-A URL] event [data|-]\n", prog);
      fprintf (stderr, "       -l debuglevel : one of: error warning info debug debug1 debug2 debug3 debug4\n");
      fprintf (stderr, "       -L logfile    : all logging is placed in this file, if - stderr\n");
      fprintf (stderr, "       -o outputfile : the return data is placed in this file \n");
      fprintf (stderr, "       -U username   : username to login to MidWay with\n");
      fprintf (stderr, "       -P password   : password for username \n");
      fprintf (stderr, "       -A URL        : the URL to the MidWay instance (e.g. srbp://host:port or ipc:ipckey\n");
      fprintf (stderr, "       -r            : pattern is a regular expression \n");
      fprintf (stderr, "       -E            : pattern is an extended regular expression \n");
      fprintf (stderr, "       -g            : pattern is a glob wildcard expression \n");
      fprintf (stderr, "       pattern...    : The pattern(s) to subscribe to\n");
   };
   exit(rc);
};
  
int main(int argc, char ** argv)
{
  int option, rc;
  int loglevel = MWLOG_DEBUG;
  extern char *optarg;
  extern int optind, opterr, optopt;

  prog = argv[0];

  while ((option = getopt(argc, argv, "l:L:A:So:i:u:c:U:P:rgE")) != EOF) {
    
    switch (option) {
      
    case 'l':
       rc = _mwstr2loglevel(optarg);
       if (rc == -1) usage(-1);
       loglevel = rc;
       mwsetloglevel(loglevel);
       DEBUG("mwevent loglevel set to %s", optarg);
       break;

    case 'U':
       username = optarg;
       break;

    case 'P':
       password = optarg;
       break;

    case 'u':
       user = optarg;
       break;

    case 'c':
       client = optarg;
       break;

    case 'A':
      url = optarg;
      break;

    case 'L':
      logfile = optarg;
      if (strcmp(logfile, "-") == 0) {
	/* internal flag in lib/mwlog.c */
	_mw_copy_on_stderr(TRUE);
      } else {
	mwopenlog(prog, logfile, loglevel);
      };
      break;

    case 'i':
      inputfile = optarg;
      break;

    case 'o':
      outputfile = optarg;
      break;

    case 'r':
       subflag = MWEVREGEXP;
       break;

    case 'E':
       subflag = MWEVEREGEXP;
       break;

    case 'g':
       subflag = MWEVGLOB;
       break;

    case 'S':
       mode = 1;
       break;

    case '?':
      usage(9);
    }
  }
  if (optind >= argc) usage (3);

  if (username) {
     mwsetcred(MWAUTH_PASSWORD, username, password);
  };

  if (mode == 0) {
     DEBUG("mwevent client starting");
     
     if (outputfile) usage(1);
     if (subflag != MWEVSTRING) usage(1);
     
     rc = mwattach(url, myclientname, 0);
     if (rc != 0) {
	Error("mwattach on url %s returned %d", url, rc);
	exit(rc);
     };

     rc = postevent(argc - optind, argv+optind) ;
     
     DEBUG("event returned %d", rc);   
     DEBUG("detaching");  
     
     mwdetach();
     exit(rc);
  } else {
     
     DEBUG("mwevent watcher starting");
  
     if (inputfile) usage(2);
     if (user) usage(2);
     
     if (outputfile) {
	fp = fopen(outputfile, "w");
	if (!fp) {
	   Error("could not open %s for writing, reason: %s", outputfile, strerror(errno));
	   exit(8);
	};
     } else {
	fp = stdout;
     };

     rc = mwattach(url, myclientname, 0);
     if (rc != 0) {
	Error("mwattach on url %s returned %d", url, rc);
	exit(rc);
     };

     watchevents(argc - optind, argv+optind);
  };
};
