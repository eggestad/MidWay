
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
#include <pthread.h>

#include <MidWay.h>

static char * RCSId UNUSED = "$Id$";

char levelprefix[] = { 'F', 'E', 'W', 'A', 'I', 'D', '1', '2', '3', '4' };
char *levelheader[] = { "FATAL: ", "ERROR: ", "Warning: ", "ALERT:", "info: ", 
			"Debug: ", "Debug1: ", "Debug2: ", "Debug3: ", "Debug4: ", 
			NULL };

static FILE *log = NULL;
static int loglevel = MWLOG_INFO;
static int _force_switchlog = 0;
static char * logprefix = NULL;
static char * progname = NULL;
static FILE * copy_on_FILE = NULL;

DECLAREMUTEX(logmutex);

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

static void
fptime(FILE *fp)
{
  struct timeval tv;
  struct tm * now;
  if (fp == NULL) return ;

  gettimeofday(&tv,NULL);
  now = localtime((time_t *) &tv.tv_sec);
  
  fprintf(fp, "%4d%2.2d%2.2d %2.2d%2.2d%2.2d.%3.3d ",
	  now->tm_year+1900, now->tm_mon+1, now->tm_mday,
	  now->tm_hour, now->tm_min, now->tm_sec, tv.tv_usec/1000);
  return ;
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
  
  if (level > loglevel) return;
  switchlog();

  
  _LOCKMUTEX(logmutex);

  /* print to log file if open */
  if (log != NULL) {
    fptime(log);
    if (progname != 0) 
      fprintf(log,"%8.8s", progname);
    fprintf(log,"[%5d] [%c]: ",getpid(), levelprefix[level]);
    vfprintf(log, format, ap);
    fputc('\n',log);
    
    fflush(log);
  } 

  /* copy to stdout/stderr if so has been desired */
  if (copy_on_FILE) {
    fprintf(copy_on_FILE,"%s", levelheader[level]);
    vfprintf(copy_on_FILE, format, ap);
    fprintf(copy_on_FILE,"\n");
    fflush(copy_on_FILE);
  };

  _UNLOCKMUTEX(logmutex);

  return ;
};

  
void 
mwlog(int level, char * format, ...)
{
  va_list ap;

  if (level > loglevel) return;
  switchlog();

  va_start(ap, format);
  
  _mw_vlogf(level, format, ap);
  
  va_end(ap);

  pthread_mutex_unlock(&logmutex);

  return ;
};

int mwsetloglevel(int level)
{
  int oldlevel;

  if (level == -1) return loglevel;
  if ( (level < MWLOG_FATAL) || (level > MWLOG_DEBUG4) ) return ;
  oldlevel = loglevel;
  loglevel = level;
  if (level >= MWLOG_DEBUG) copy_on_FILE = stderr;
  else copy_on_FILE = NULL;
  return oldlevel;
};

void mwsetlogprefix(char * lfp)
{
  if (lfp == NULL) return;
  if (logprefix != NULL) free (logprefix);
  logprefix = strdup(lfp);
};

void mwopenlog(char * prog, char * lfp, int level)
{
  /* set flag for switchlog() to switch logs even if not at the endof day. */
  _force_switchlog = 1;
  mwsetlogprefix(lfp);
  mwsetloglevel(level);

  if (progname != NULL)
    free (progname);
   
  if (prog == NULL) {
    progname = NULL;
  } else {
    progname = strdup(prog);
  };
};
