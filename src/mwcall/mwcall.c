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
 * Revision 1.1  2000/09/21 18:15:45  eggestad
 * A simple client for testing and shell scripts
 *
 * Revision 1.2  2000/07/20 19:43:31  eggestad
 * CVS keywords were missing
 *
 * Revision 1.1.1.1  2000/03/21 21:04:21  eggestad
 * Initial Release
 *
 * Revision 1.1.1.1  2000/01/16 23:20:12  terje
 * MidWay
 *
 */


static char * RCSId = "$Id";
static char * RCSName = "$Name$"; /* CVS TAG */

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

static char * url = NULL;

static char * inputfile = NULL;
static char * outputfile = NULL;
static char * logfile = NULL;
static int noreply = 0;
static char * prog; 

int call(int argc, char ** argv) 
{
  int i, j, len = 0, apprc, rc;
  char * data = NULL, * rdata = NULL;
  struct timeval start, end;

  if (argv[0] == NULL) {
    return -EINVAL;
  }

  /* if data on commandline */
  if (argv[1] != NULL) {
    if (strcmp(argv[1], "-") == 0) {
      mwlog(MWLOG_DEBUG,"about to read input data from stdin");
      exit(9);
    } else {
      mwlog(MWLOG_DEBUG,"about to read input data from commandline");
      len = strlen(argv[1]);
      data = (char *) malloc(len+1);
      strcpy(data, argv[1]);
    }
  } 

  if ( (data == NULL) && (inputfile)) {
    struct stat if_stat;
    int toread, readed = 0;
    int fd;

    mwlog(MWLOG_DEBUG,"about to read input data from %s", inputfile);

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
    while(toread > SSIZE_MAX) {
      readed += read (fd, data + readed, SSIZE_MAX);
      toread -= toread;
    };
    readed += read (fd, data + readed, toread);

    close(fd);
    len = readed;
  };


  mwlog(MWLOG_DEBUG,"about to call(%s, %s, %d, ...)", argv[0], data, len);
  gettimeofday(&start, NULL); 

#ifdef TESTASYNC
  {
    int hdl1 , hdl2, hdl3;
    int rlen;
    hdl1 = mwacall(argv[0], data, len, 0);
    hdl2 = mwacall(argv[0], data, len, 0);
    hdl3 = mwacall(argv[0], data, len, 0);
    rc = mwfetch(0, &rdata, &rlen, &apprc, 0);
    rc = mwfetch(0, &rdata, &rlen, &apprc, 0);
    rc = mwfetch(0, &rdata, &rlen, &apprc, 0);
    hdl1 = mwacall(argv[0], data, len, 0);
    hdl2 = mwacall(argv[0], data, len, 0);
    hdl3 = mwacall(argv[0], data, len, 0);
    rc = mwfetch(hdl1, &rdata, &rlen, &apprc, 0);
    rc = mwfetch(hdl3, &rdata, &rlen, &apprc, 0);
    rc = mwfetch(hdl2, &rdata, &rlen, &apprc, 0);
  }
#else 
  rc = mwcall(argv[0], data, len, &rdata, &len, &apprc, 0);
#endif

  gettimeofday(&end, NULL); 

  if (rc != 0) {
    mwlog(MWLOG_ERROR,"Call failed with reason %d", rc);
    return rc;
  };
  mwlog(MWLOG_INFO,"call returned in %f", 
	 (float)(end.tv_sec - start.tv_sec) 
	 +(float)(end.tv_usec - start.tv_usec)/1000000); 
  
  if (rdata != NULL) {
    FILE * OF;
    mwlog(MWLOG_INFO,
	  "Call to \"%s\" succeded, returned %d bytes of data with application return code %d",  
	    argv[0], len, apprc);

    if (outputfile) {
      OF = fopen(outputfile, "w");
    } else { 
      OF = stdout;
    };
    fwrite (rdata, len, 1, OF);
    /* implisitt close on exit */
  } else {
    mwlog(MWLOG_INFO,
	  "Call to \"%s\" succeded, returned no data with application return code %d",  
	    argv[0], data, len, apprc);
  };


  mwlog(MWLOG_DEBUG,"call to %s returned %s len %d bytes rc = %d apprc=%d)", 
	argv[1], data, len, rc, apprc);

  
  mwfree(rdata);
  
  return apprc;
};

static char * prog = "mwcall";

void usage(int rc) 
{
  fprintf (stderr, "usage: %s [-l debuglevel] [-L logfile] [-i inputfile] [-o outputfile] [-R] [-A URL] service [data|-]\n", prog);
  fprintf (stderr, "       -l debuglevel : one of: error warning info debug debug1 debug2 debug3 debug4\n");
  fprintf (stderr, "       -L logfile    : all logging is placed in this file, if - stderr\n");
  fprintf (stderr, "       -i inputfile  : The data to be passed the service is read from this file\n");
  fprintf (stderr, "       -o outputfile : the return data is placed in this file \n");
  fprintf (stderr, "       -R            : Noreply \n");
  fprintf (stderr, "       -A URL        : the URL to the MidWay instance (e.g. srbp://host:port or ipc:ipckey\n");
  fprintf (stderr, "       service       : The name of the MidWay service to call\n");
  fprintf (stderr, "       data | -      : either a data string or - to use stdin\n");
  exit(rc);
};

  
main(int argc, char ** argv)
{
  int option, rc, i;
  int loglevel = MWLOG_DEBUG;
  extern char *optarg;
  extern int optind, opterr, optopt;

  prog = argv[0];

  while ((option = getopt(argc, argv, "l:L:o:i:RA:")) != EOF) {
    
    switch (option) {
      
    case 'l':
      if      (strcmp(optarg, "error")   == 0) loglevel=MWLOG_ERROR;
      else if (strcmp(optarg, "warning") == 0) loglevel=MWLOG_WARNING;
      else if (strcmp(optarg, "info")    == 0) loglevel=MWLOG_INFO;
      else if (strcmp(optarg, "debug")   == 0) loglevel=MWLOG_DEBUG;
      else if (strcmp(optarg, "debug1")  == 0) loglevel=MWLOG_DEBUG1;
      else if (strcmp(optarg, "debug2")  == 0) loglevel=MWLOG_DEBUG2;
      else if (strcmp(optarg, "debug3")  == 0) loglevel=MWLOG_DEBUG3;
      else if (strcmp(optarg, "debug4")  == 0) loglevel=MWLOG_DEBUG4;
      else usage(-1);
      mwsetloglevel(loglevel);
      mwlog (MWLOG_DEBUG, "mwcall loglevel set to %s", optarg);
      break;
    case 'R':
      noreply = 1;
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

    case '?':
      usage(-1);
    }
  }
  mwlog (MWLOG_DEBUG, "mwcall client starting");

  if (optind >= argc) usage (-2);

  rc = mwattach(url, "mwcall", NULL, NULL, 0 );
  if (rc != 0) {
    mwlog(MWLOG_ERROR, "mwattach on url %s returned %d", url, rc);
    exit(rc);
  };

  rc = call(argc - optind, argv+optind) ;

  mwlog(MWLOG_DEBUG, "call returned %d", rc);   
  mwlog(MWLOG_DEBUG, "detaching");  

  mwdetach();
  exit(rc);
};