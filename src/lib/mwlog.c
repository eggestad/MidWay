
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

#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>

#include <MidWay.h>
#include <ipctables.h>

static char * RCSId UNUSED = "$Id$";

/** @file
This module implement the provided logging API see man page for mwlog(3).
*/

/* vt100'ish color codes. See ctlseq.ms that float around the net,
   which describe xterms vt100 control sequences.  */
#ifndef NOCOLOR

#define COLORNORMAL 		"\e[m"

#define COLOR_RED_ON_YELLOW	"\e[37;1;43m"
#define COLOR_RED_BOLD		"\e[31;1m"
#define COLOR_YELLOW_BOLD	"\e[33;1m"
#define COLOR_MAGNETA_BOLD	"\e[35;1m"
#define COLOR_GREEN_BOLD	"\e[32;1m"
#define COLOR_BLUE		"\e[34m"
#define COLOR_GREY		"\e[37m"

#else 

#define COLORNORMAL 		

#define COLOR_RED_ON_YELLOW	
#define COLOR_RED_BOLD		
#define COLOR_YELLOW_BOLD	
#define COLOR_MAGNETA_BOLD	
#define COLOR_GREEN_BOLD	
#define COLOR_BLUE		
#define COLOR_GREY		

#endif 

static char levelprefix[] = { 'F', 'E', 'W', 'A', 'I', 'D', '1', '2', '3', '4' };

static char * ttylevelheader[] = { COLOR_RED_ON_YELLOW "FATAL: "  COLORNORMAL, 
				   COLOR_RED_BOLD      "ERROR: "  COLORNORMAL, 
				   COLOR_YELLOW_BOLD   "Warning: " COLORNORMAL, 
				   COLOR_MAGNETA_BOLD  "ALERT: "  COLORNORMAL, 
				   COLOR_GREEN_BOLD    "info: "   COLORNORMAL, 
				   COLOR_BLUE          "Debug: "  COLORNORMAL, 
				   COLOR_GREY          "Debug1: " COLORNORMAL, 
				   COLOR_GREY          "Debug2: " COLORNORMAL, 
				   COLOR_GREY          "Debug3: " COLORNORMAL, 
				   COLOR_GREY          "Debug4: " COLORNORMAL, 
				   NULL };

static char *levelheader[] = { "FATAL: ", 
			       "ERROR: ", 
			       "Warning: ", 
			       "ALERT: ", 
			       "info: ", 
			       "Debug: ", 
			       "Debug1: ", 
			       "Debug2: ", 
			       "Debug3: ", 
			       "Debug4: ", 
			       NULL };

static FILE *log = NULL;
static int loglevel = MWLOG_INFO;
static int _force_switchlog = 0;
static char * logprefix = NULL;
static char * progname = NULL;
static FILE * copy_on_FILE = NULL;
static int loginited = 0;

//#define DEBUGGING_X
// we can use DEBUG here, since we an error here will likely stop all logging 
#ifdef DEBUGGING_X
DECLAREMUTEX(logmutex);
#define _fprintf(m...) fprintf(m)
#else 
#define _fprintf(m...)
#endif



/**
   Indicate that logging shall be copied on stdout. 

   this is meant to be undocumented. Needed my mwd to print on stdout before 
   becoming a daemon.
*/
void _mw_copy_on_stdout(int flag)
{
  if (flag) copy_on_FILE = stdout;
  else copy_on_FILE = NULL;
  return;
};

/**
   Indicate that logging shall be copied on stderr. 

   this is meant to be undocumented. Needed my mwd to print on stdout before 
   becoming a daemon.
*/
void _mw_copy_on_stderr(int flag)
{
  if (flag) copy_on_FILE = stderr;
  else copy_on_FILE = NULL;
  return;
};

static int
sptime(char *b, int max)
{
  int rc;
  struct timeval tv;
  struct tm * now;
  if (b == NULL) return  -1;

  gettimeofday(&tv,NULL);
  now = localtime((time_t *) &tv.tv_sec);
  
  rc = snprintf(b, max, "%4d%2.2d%2.2d %2.2d%2.2d%2.2d.%3.3ld ",
		now->tm_year+1900, now->tm_mon+1, now->tm_mday,
		now->tm_hour, now->tm_min, now->tm_sec, tv.tv_usec/1000);
  return rc;
}

static char timesuffix[100] = ""; 
static char newsuffix[100];
static char filename[256];

/**
   rotate the logfind is we'ew crossed midnite. 

*/
static void 
switchlog (void)
{
  struct timeval tv;


  if (logprefix == NULL) return ;

  /* if an openlog call has been made in between, we swichlog even if not at the end of day */ 
  gettimeofday(&tv,NULL);
 
  if (_force_switchlog) {
    _force_switchlog = 0;
  } else {
    strftime(newsuffix, 100, "%Y%m%d", localtime(&tv.tv_sec));
    if (strcmp(newsuffix, timesuffix) == 0) {
      return;
    };
  };
  
  {
    int i;
    for (i = 0; i < 256; i++) 
      filename [i] = '\0';
  };

  strftime(timesuffix, 100, "%Y%m%d", localtime(&tv.tv_sec));

  if (logprefix != NULL) {
    strcpy (filename, logprefix);
  } else {
    strcpy (filename, "userlog");
  };
  strcat (filename,".");
  strcat (filename,timesuffix);

  if (log != NULL) {
    //    mwlog(MWLOG_INFO, "Switching to New Log %s", filename);
    fclose(log);
  };
  
  log = fopen(filename,"a");
  
  //mwlog(MWLOG_INFO, "Switched to New Log %s", filename);
  return;
}
#define LOG_MSG_MAX (64*1024)

static   char buffer[LOG_MSG_MAX];
static char mesg[LOG_MSG_MAX];

/**
   This is mwlog() incarnate. 

   mwlog() is really a varargs wrapper that ultimatly call this
   function. It is here is a va_ version for reuse internally.

   @param level loglevel as defined in MidWay.h
   @param format the printf format
   @param ap the va_list see stdard.h
*/
void 
_mw_vlogf(int level, char * format, va_list ap)
{

  int l, e, s, rc;

  if (level > loglevel) return;
  if (format == NULL) return;
  e = errno;

  if (!loginited) mwopenlog(NULL, NULL, MWLOG_INFO);
  switchlog();

#ifdef DEBUGGING_X
  _LOCKMUTEX(logmutex);
#endif 

  rc = vsnprintf(mesg, LOG_MSG_MAX, format, ap);
  if (rc == LOG_MSG_MAX) mesg[LOG_MSG_MAX-1] = '\0';

  /* print to log file if open */
  if (log != NULL) {
    char timestamp[40];

    buffer[0] = '\0';
    s = LOG_MSG_MAX-2; 
    l = 0;

    // we don't check rc on these, since the *must* succed. 
    rc = sptime(timestamp, 40);

    l = snprintf(buffer, s, "%s %8.8s" "[%5d] [%c]: " "%s\n",
		 timestamp, 
		 progname?progname:"", 
		 getpid(), levelprefix[level], 
		 mesg);
    
    if (l >= s) {
      buffer[LOG_MSG_MAX-2] = '\n';
      buffer[LOG_MSG_MAX-1] = '\0';
    };
    
    write (fileno(log), buffer, l);
  } 

  //  copy_on_FILE = 0;

  /* copy to stdout/stderr if so has been desired */
  if (copy_on_FILE) {
     if (ttyname(fileno(copy_on_FILE))) {
	fprintf(copy_on_FILE,"%s%s\n", ttylevelheader[level], mesg);
     } else {
	fprintf(copy_on_FILE,"%s%s\n", levelheader[level], mesg);
     }
    //    fflush(copy_on_FILE);
  };

#ifdef DEBUGGING_X
  _UNLOCKMUTEX(logmutex);
#endif

  errno = e;
  return ;
};

/**
  Get the string representaion of a loglevel.

*/
int _mwstr2loglevel(char * arg) 
{
   int loglevel = -1, l;
   if      (strcasecmp(arg, "fatal")   == 0) loglevel=MWLOG_FATAL;
   else if (strcasecmp(arg, "error")   == 0) loglevel=MWLOG_ERROR;
   else if (strcasecmp(arg, "warning") == 0) loglevel=MWLOG_WARNING;
   else if (strcasecmp(arg, "alert")   == 0) loglevel=MWLOG_ALERT;
   else if (strcasecmp(arg, "info")    == 0) loglevel=MWLOG_INFO;
   else if (strcasecmp(arg, "debug")   == 0) loglevel=MWLOG_DEBUG;
   else if (strcasecmp(arg, "debug1")  == 0) loglevel=MWLOG_DEBUG1;
   else if (strcasecmp(arg, "debug2")  == 0) loglevel=MWLOG_DEBUG2;
   else if (strcasecmp(arg, "debug3")  == 0) loglevel=MWLOG_DEBUG3;
   else {
      l = atoi(arg);
      if ( (l >= MWLOG_FATAL) && (l <= MWLOG_DEBUG3)) {
	 loglevel = l;
      };
   };
   return loglevel;
};

#ifdef  _ISOC99_SOURCE
static __thread char strbuf[64];
#else 
static  char strbuf[64];
#endif
/** 
    get a string version of the MWID.

    This is for debugging purposes, note that unless you're in C99, using NULL for buffer is not threadsafe
*/
char * _mwid2str(MWID id, char * buffer)
{
   int idx;
   if (!buffer) buffer = strbuf;

   if ( (idx=SRVID2IDX(id)) != UNASSIGNED) {
      sprintf(buffer, "SRV:%d", idx);
      return buffer;
   } 

   if ( (idx=GWID2IDX(id)) != UNASSIGNED) {
      sprintf(buffer, "GW:%d", idx);
      return buffer;
   } 

   if ( (idx=SVCID2IDX(id)) != UNASSIGNED) {
      sprintf(buffer, "SVC:%d", idx);
      return buffer;
   } 

   if ( (idx=CLTID2IDX(id)) != UNASSIGNED) {
      sprintf(buffer, "CLT:%d", idx);
      return buffer;
   } 

   if (id == UNASSIGNED) {
      sprintf(buffer, "UNASSIGNED");
      return buffer;
   } 
   
   sprintf(buffer, "ERROR:%#x", id);
   return buffer;
} 

/**
   The main logging function. 

   This is a part of the user library API see man page
*/
void mwlog(int level, char * format, ...)
{
  va_list ap;

  va_start(ap, format);
  
  _mw_vlogf(level, format, ap);
  
  va_end(ap);

  return ;
};

/**
   Set the loglevel. 

   This is a part of the user library API see man page. 
   Sets what log messages that shall be ignored. 
*/
int mwsetloglevel(int level)
{
  int oldlevel;
  char * tmp;

  if (level == -1) return loglevel;

  // env overrides code
  if (tmp = getenv("MWLOG_LEVEL")) {
     level = _mwstr2loglevel(tmp);
  };

  if ( (level < MWLOG_FATAL) || (level > MWLOG_DEBUG4) ) return -EINVAL;
  oldlevel = loglevel;
  loglevel = level;
  DEBUG("loglevel is now %s", levelheader[level]);

  tmp = getenv("MWLOG_STDERR");
  if (tmp) {
     if (atoi(tmp)) copy_on_FILE = stderr;
     else copy_on_FILE = NULL;
  } else {
     if (level >= MWLOG_DEBUG) copy_on_FILE = stderr;
     else copy_on_FILE = NULL;
  };
  loginited = 1;
  return oldlevel;
};

/**
   give us the pointer to the loglevel variable

   for internal use
*/
int * _mwgetloglevel(void)
{
  return &loglevel;
};

static char * logfilename = NULL;
static char * logdir = NULL;

/**
   This is a part of the user library API see man page. 
  
   The if's and but's here  are getting complicated...  

   if the arg lfp (LogFilePrefix) is not NULL, we check if the lfp has
   a leading path it's relative to cwd.  If not we place it in the
   logdir.  However, we may not have the logdir until we actually
   mwattach(), and then only in the case if IPC attach. The logdir is
   default mwhome/instancename/log/.

   if lfp is NULL we use progname as default filename, if progname is
   NULL, we use userlog;
*/

void mwsetlogprefix(char * lfp)
{
  char * mwhome, * instancename, *tmp;
  ipcmaininfo * ipcmain;

  if (tmp = getenv("MWLOG_PREFIX")) lfp = tmp;

  _fprintf(stderr, "logprefix arg = %s at %s:%d\n", lfp, __FUNCTION__, __LINE__);

  // if arg is null and we've set logprefix, we no not override with default. 
  if ( (logprefix != NULL) && (lfp == NULL) ) return;
  loginited = 1;

  if (logfilename) free(logfilename);

  if (lfp != NULL) {
    char * slash;
    tmp = strdup(lfp); 
    slash = strrchr(tmp, '/'); 
    if (slash == NULL) 
      logfilename = tmp;
    else {
      *slash = '\0';
      slash++;
      _fprintf(stderr, "logfilename := %s (%d)\n", slash, strlen(slash));
      if (strlen(slash) > 0) 
	logfilename = strdup(slash);
      else 
	logfilename = strdup(progname);

      logdir = malloc(PATH_MAX);
      logdir[0] = '\0';
      _fprintf(stderr, "logdirsuffix := %s\n", tmp);
      if (tmp[0] != '/') { // leading / means absolute 
	getcwd(logdir, PATH_MAX);
	strncat(logdir, "/", PATH_MAX-strlen(logdir));
      } 
      strncat(logdir, tmp, PATH_MAX-strlen(logdir));
      logdir[PATH_MAX-1] = '\0';
      free(tmp);

    };
  } else {
    // ok called with NULL, attempting to set default
    if ((logfilename == NULL) && (progname != NULL)) {
      logfilename = strdup(progname);
    };
  };

  _fprintf(stderr, "logfilename = %s logdir = %s at %s:%d\n", 
	   logfilename?logfilename:"(null)", 
	   logdir?logdir:"(null)", 
	   __FUNCTION__, __LINE__);
  

  /* MWHOME and MWINSTANCE is set by mwd, if their're not set we assume
     start by  commandline and log to  stderr until we  get the ipcmain
     info */

  if (logdir == NULL) {
    mwhome = getenv ("MWHOME");
    instancename = getenv ("MWINSTANCE");
    
    if (!(mwhome && instancename)) {
      // last try is shm
      ipcmain = _mw_ipcmaininfo();
      if (ipcmain != NULL) {
	
	if  (!instancename) 
	  instancename = ipcmain->mw_instance_name;
	
	if  (!mwhome) 
	  mwhome = ipcmain->mw_homedir;
      };
    } 

    if (mwhome && instancename) {
      logdir = malloc(strlen(mwhome) + strlen(instancename) +10);
      sprintf(logdir, "%s/%s/log", mwhome, instancename);
    };

    _fprintf(stderr, "logfilename = %s logdir = %s at %s:%d\n", 
	     logfilename?logfilename:"(null)", 
	     logdir?logdir:"(null)", 
	     __FUNCTION__, __LINE__);
  };

  if ( (logdir != NULL) && (logfilename != NULL) ) {
    if (logprefix != NULL) free (logprefix);
    logprefix = malloc(strlen(logdir) + strlen(logfilename) + 10);
    sprintf(logprefix, "%s/%s", logdir, logfilename);
    _force_switchlog = 1;
    _fprintf(stderr, "logprefix = %s at %s:%d\n", logprefix, __FUNCTION__, __LINE__);
  };
  return;
};

/**
   replaces both mwsetloglevel and mwsetlogprefix.

   This is a part of the user library API see man page. 
*/
void mwopenlog(char * prog, char * lfp, int level)
{
  _fprintf(stderr, "openlog(prog=%s, logprefix=%s, level=%d\n", 
	   prog?prog:"(null)", 
	   lfp?lfp:"(null)", 
	   level);

  if (progname != NULL)
    free (progname);
   
  if (prog == NULL) {
    progname = NULL;
  } else {
    progname = strdup(prog);
  };

  mwsetlogprefix(lfp);
  mwsetloglevel(level);
  loginited = 1;

};
