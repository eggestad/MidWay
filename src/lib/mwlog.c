
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

/*
 * $Id$
 * $Name$
 * 
 * $Log$
 * Revision 1.12  2002/11/18 00:10:55  eggestad
 * - Made vt100 colors more readable, replaceable, and potentially configurable
 *
 * Revision 1.11  2002/11/13 16:30:18  eggestad
 * rewamped _mw_vlogf to format the whole message in a buffer, and writeit out at once. Better performance, and we got rid of the mutex locks
 *
 * Revision 1.10  2002/11/08 00:12:58  eggestad
 * Major fixup on default logfile
 *
 * Revision 1.9  2002/10/22 21:58:20  eggestad
 * Performace fix, the connection peer address, is now set when establised, we did a getnamebyaddr() which does a DNS lookup several times when processing a single message in the gateway (Can't believe I actually did that...)
 *
 * Revision 1.8  2002/10/03 21:10:42  eggestad
 * - switchlog() didn't switch log on mwsetlogprefix()
 *
 * (still not quite happy on default logfiles)
 *
 * Revision 1.7  2002/08/09 20:50:15  eggestad
 * A Major update for implemetation of events and Task API
 *
 * Revision 1.6  2002/08/07 23:54:06  eggestad
 * fixup for DEBUGs so we're C99 compliant
 *
 * Revision 1.5  2002/07/07 22:35:20  eggestad
 * *** empty log message ***
 *
 * Revision 1.4  2002/02/17 14:14:22  eggestad
 * missing include added
 *
 * Revision 1.3  2000/09/21 18:42:39  eggestad
 * Changed a bit the copy_on_stdout to be either stderr or stdout.
 *
 * Revision 1.2  2000/07/20 19:26:59  eggestad
 * - progname added til logline.
 * - mwlog() now threadsafe.
 * - diffrent unspecified log file name.
 * - New loglevels.
 * - new function mwopenlog().
 *
 * Revision 1.1.1.1  2000/03/21 21:04:12  eggestad
 * Initial Release
 *
 * Revision 1.1.1.1  2000/01/16 23:20:12  terje
 * MidWay
 *
 */


#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#include <MidWay.h>
#include <ipctables.h>

static char * RCSId UNUSED = "$Id$";


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

char levelprefix[] = { 'F', 'E', 'W', 'A', 'I', 'D', '1', '2', '3', '4' };
char *levelheader[] = { COLOR_RED_ON_YELLOW "FATAL: "  COLORNORMAL, 
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

static FILE *log = NULL;
static int loglevel = MWLOG_INFO;
static int _force_switchlog = 0;
static char * logprefix = NULL;
static char * progname = NULL;
static FILE * copy_on_FILE = NULL;


// we can use DEBUG here, since we an error here will likely stop all logging 
#ifdef DEBUGGING_X
DECLAREMUTEX(logmutex);
#define _fprintf(m...) fprintf(m)
#else 
#define _fprintf(m...)
#endif



/* this is meant to be undocumented. Needed my mwd to print on stdout before 
   becoming a daemon.*/

void _mw_copy_on_stdout(int flag)
{
  if (flag) copy_on_FILE = stdout;
  else copy_on_FILE = NULL;
  return;
};

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
  
  rc = snprintf(b, max, "%4d%2.2d%2.2d %2.2d%2.2d%2.2d.%3.3d ",
		now->tm_year+1900, now->tm_mon+1, now->tm_mday,
		now->tm_hour, now->tm_min, now->tm_sec, tv.tv_usec/1000);
  return rc;
}

static void 
switchlog (void)
{
  struct timeval tv;
  static char timesuffix[100] = ""; 
  char newsuffix[100];
  char filename[256];


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
  
  strftime(timesuffix, 100, "%Y%m%d", localtime(&tv.tv_sec));

  if (logprefix != NULL) {
    strcpy (filename, logprefix);
  } else {
    strcpy (filename, "userlog");
  };
  strcat (filename,".");
  strcat (filename,timesuffix);

  if (log != NULL) {
    mwlog(MWLOG_INFO, "Switching to New Log %s", filename);
    fclose(log);
  };
  
  log = fopen(filename,"a");
  
  mwlog(MWLOG_INFO, "Switched to New Log %s", filename);
  return;
}

  
void 
_mw_vlogf(int level, char * format, va_list ap)
{
  char buffer[LINE_MAX];
  int l, s, rc;

  if (level > loglevel) return;

  if (logprefix == NULL) mwsetlogprefix(NULL);
  switchlog();

#ifdef DEBUGGING_X
  _LOCKMUTEX(logmutex);
#endif 

  s = LINE_MAX-2; 
  l = 0;
  /* print to log file if open */
  if (log != NULL) {

    // we don't check rc on these, since the *must* succed. 
    rc = sptime(buffer+l, s);
    s -= rc;
    l += rc;

    _fprintf (stderr, "%d (l=%d s=%d) %s\n", __LINE__, l, s, buffer);
  
    if (progname != 0) {
      rc = snprintf(buffer+l, s, "%8.8s", progname);
      s -= rc;
      l += rc;
    };

    _fprintf (stderr, "%d (l=%d s=%d) %s\n", __LINE__, l, s, buffer);
          
    rc = snprintf(buffer+l, s, "[%5d] [%c]: ",getpid(), levelprefix[level]);
    s -= rc;
    l += rc;

    _fprintf (stderr, "%d (l=%d s=%d) %s\n", __LINE__, l, s, buffer);

    // Here we might get an overflow...
    rc = vsnprintf(buffer+l, s, format, ap);
    l += rc;

    _fprintf (stderr, "%d (l=%d s=%d) %s\n", __LINE__, l, s, buffer);

    if (rc > s) {
      s = 0;
      buffer[l-1] = '\n';
    } else {
      buffer[l] = '\n';
      buffer[l+1] = '\0';
      l++;
    };
    _fprintf (stderr, "%d %s", __LINE__, buffer);

    fwrite (buffer, 1, l, log);
  } 

  /* copy to stdout/stderr if so has been desired */
  if (copy_on_FILE) {
    fprintf(copy_on_FILE,"%s", levelheader[level]);
    vfprintf(copy_on_FILE, format, ap);
    fprintf(copy_on_FILE,"\n");
    fflush(copy_on_FILE);
  };

#ifdef DEBUGGING_X
  _UNLOCKMUTEX(logmutex);
#endif

  return ;
};

  
void 
mwlog(int level, char * format, ...)
{
  va_list ap;

  va_start(ap, format);
  
  _mw_vlogf(level, format, ap);
  
  va_end(ap);

  return ;
};

int mwsetloglevel(int level)
{
  int oldlevel;

  if (level == -1) return loglevel;
  if ( (level < MWLOG_FATAL) || (level > MWLOG_DEBUG4) ) return ;
  oldlevel = loglevel;
  loglevel = level;
  DEBUG("loglevel is now %s", levelheader[level]);
  if (level >= MWLOG_DEBUG) copy_on_FILE = stderr;
  else copy_on_FILE = NULL;
  return oldlevel;
};

/* the if's and but's here  are getting complicated...  

if the arg lfp (LogFilePrefix) is not NULL, we check if the lfp has
a leading  path it's relative  to cwd.  If  not we place it  in the
logdir.   However, we  may not  have the  logdir until  we actually
mwattach(), and then only in the  case if IPC attach. The logdir is
default mwhome/instancename/log/. 

if lfp is NULL we use  progname as default filename, if progname is
NULL, we use userlog;
*/

static char * logfilename = NULL;
static char * logdir = NULL;


void mwsetlogprefix(char * lfp)
{
  char * mwhome, * instancename;
  ipcmaininfo * ipcmain;

  _fprintf(stderr, "logprefix arg = %s at %s:%d\n", lfp, __FUNCTION__, __LINE__);

  // if arg is null and we've set lofprefix, we no not override with default. 
  if ( (logprefix != NULL) && (lfp == NULL) ) return;

  if (lfp != NULL) {
    char * tmp = strdup(lfp); 
    char * slash;
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
      };
      strncat(logdir, tmp, PATH_MAX-strlen(logdir));
      logdir[PATH_MAX] = '\0';
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

  if (logdir != NULL) {
    if (logprefix != NULL) free (logprefix);
    logprefix = malloc(strlen(logdir) + strlen(logfilename) +1);
    sprintf(logprefix, "%s/%s", logdir, logfilename);
    _force_switchlog = 1;
    _fprintf(stderr, "logprefix = %s at %s:%d\n", logprefix, __FUNCTION__, __LINE__);
  };
  return;
};


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

};
