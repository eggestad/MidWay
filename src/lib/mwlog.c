
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

static char * RCSId = "$Id$";
static char * RCSName = "$Name$"; /* CVS TAG */


#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>
#include <pthread.h>

#include <MidWay.h>

/*                     hours minutes seconds */
static int DAYLENGTH =  24  *  60   *   60;

char levelprefix[] = { 'F', 'E', 'W', 'A', 'I', 'D', '1', '2', '3', '4' };
char *levelheader[] = { "FATAL: ", "ERROR: ", "Warning: ", "ALERT:", "info: ", 
			"Debug: ", "Debug1: ", "Debug2: ", "Debug3: ", "Debug4: ", 
			NULL };

static FILE *log = NULL;
static loglevel = MWLOG_INFO;
static time_t switchtime = 0;
static char * logprefix = NULL;
static char * progname = NULL;
static int copy_on_stdout = FALSE;

pthread_mutex_t logmutex = PTHREAD_MUTEX_INITIALIZER;

/* this is meant to be undocumented. Needed my mwd to print on stdout before 
   becoming a daemon.*/

void _mw_copy_on_stdout(int flag)
{
  if (flag) copy_on_stdout = TRUE;
  else copy_on_stdout = FALSE;
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
  struct tm * now;
  static char timesuffix[100] = ""; 
  char newsuffix[100];
  char filename[256];
  int l;

  if (logprefix == NULL) return ;

  gettimeofday(&tv,NULL);
  strftime(newsuffix, 100, "%Y%m%d", localtime(&tv.tv_sec));
  if (strcmp(newsuffix, timesuffix) == 0) return;

  strftime(timesuffix, 100, "%Y%m%d", localtime(&tv.tv_sec));
  if (log != NULL) fclose(log);

  if (logprefix != NULL) {
    strcpy (filename, logprefix);
  } else {
    strcpy (filename, "userlog");
  };
  strcat (filename,".");
  strcat (filename,timesuffix);
  log = fopen(filename,"a");

  mwlog(MWLOG_INFO, "Switched to New Log", timesuffix);
  return;
}
  
void 
mwlog(int level, char * format, ...)
{
  va_list ap;

  if (level > loglevel) return;
  switchlog();

  va_start(ap, format);

  pthread_mutex_lock(&logmutex);

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

  /* copy to stdout if so has been desired */
  if ((log == NULL) || copy_on_stdout) {
    fprintf(stdout,"%s", levelheader[level]);
    vfprintf(stdout, format, ap);
    fprintf(stdout,"\n");
    fflush(stdout);
  };

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
  if (level >= MWLOG_DEBUG) copy_on_stdout = TRUE;
  else copy_on_stdout = FALSE;
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
