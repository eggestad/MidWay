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
 * Revision 1.8  2003/04/25 13:03:12  eggestad
 * - fix for new task API
 * - new shutdown procedure, now using a task
 *
 * Revision 1.7  2002/11/19 12:43:55  eggestad
 * added attribute printf to mwlog, and fixed all wrong args to mwlog and *printf
 *
 * Revision 1.6  2002/09/04 07:19:12  eggestad
 * mwd now sends an event on service (un)provide
 *
 * Revision 1.5  2002/08/09 20:50:16  eggestad
 * A Major update for implemetation of events and Task API
 *
 * Revision 1.4  2002/07/07 22:45:48  eggestad
 * *** empty log message ***
 *
 * Revision 1.3  2002/02/17 18:02:14  eggestad
 * - added missing includes
 *
 * Revision 1.2  2000/07/20 19:54:52  eggestad
 * prototype fixup.
 *
 * Revision 1.1.1.1  2000/03/21 21:04:30  eggestad
 * Initial Release
 *
 * 
 */

#include <errno.h>
#include <stdio.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>

#include <MidWay.h>
#include <ipctables.h>
#include "mwd.h"
#include "tables.h"
#include "watchdog.h"

static char * RCSId UNUSED = "$Id$";
static char * RCSName UNUSED = "$Name$"; /* CVS TAG */

/* undocumented func in lib/mwlog.c */
void _mw_copy_on_stdout(int flag);


/* The periode in seconds  between every time the dog shall go on patrol */
#ifndef INTERVAL
#define INTERVAL 120
#endif

static int go_on_patrol(void)
{
  struct timeval tm;

  gettimeofday(&tm, NULL);

  /* check to see if some members have disappeared without letting us know.*/
  DEBUG("Watchdog: checking tables");
  check_tables();
  /* check for buffer leaks */
  /*  check_heap(); */

  ipcmain->lastactive = tm.tv_sec;
  DEBUG("Watchdog: all checks complete");
  return 0;
};

static int run_watchdog(void)
{
  int rc;
  int remaining, sleeping; 
  time_t now;

  remaining = 0;
  sleeping = INTERVAL;

  now = time(NULL);

  while(1) {
    if (remaining == 0) sleeping = INTERVAL;
    else sleeping = remaining;
    
    /* shutdown time is within the next interval */
    if ( (ipcmain->shutdowntime > ipcmain->boottime) && 
	 (ipcmain->shutdowntime < remaining + now) ) 
      sleeping = ipcmain->shutdowntime - now;
    
    /***************************************************
     * SLEEPING 
     ***************************************************/
    remaining = sleep(sleeping);     
    now = time(NULL);

    DEBUG("watchdog awoke after going to sleep for %d, with %d remaining", 
	  sleeping, remaining);

    /* basic sanity checks */
    if (ipcmain->mwdpid != getppid()) {
      Error("Watchdog: mwd who was my parent died unexpectantly, This Can't happen. %d != %d", 
	    ipcmain->mwdpid, getppid());
      Error("Watchdog: attempting to clean up everything.");
      kill_all_servers(SIGKILL);

      cleanup_ipc();

      exit(0);
    };

    /* if sleep returned before end of INTERVAL, 
     * this is casued by a signal, it shall be normally a signal from the parent 
     * mwd (requester) by coudl be strays from som other process.
     * of course shutdown(8) will give it too.
     */
    if (remaining > 0) {
      DEBUG("sleep(%d) ended prematurely with %d second to go", 
	    sleeping, remaining);
      if (ipcmain->status == MWSHUTDOWN) {
	Info("MWD Watchdog: ordered to take everything down");
	kill_all_servers(SIGTERM);
	ipcmain->status = MWDEAD;
      };
    } else {
      rc = go_on_patrol();
      if (rc != 0) break;
    };
  };
  return 0;
};
/* I'm not sure if the watchdog should be a thread or separate process.
    Its a process until futher.
 */
int start_watchdog(void)
{
  int rc;
  
   /* I can't start the dog with out the shm seg.*/
  if (ipcmain == NULL) return -ENOMEM;

  if (clttbl == NULL)  return -EUCLEAN;
  if (srvtbl == NULL)  return -EUCLEAN;
  if (svctbl == NULL)  return -EUCLEAN;
  if (gwtbl == NULL)   return -EUCLEAN;
  if (convtbl == NULL) return -EUCLEAN;

   /* Check heap */
  rc = fork();
  if (rc == -1) {
    return -errno;
  };

  /* parent */
  if (rc > 0) {
    ipcmain->mwwdpid = rc;
    return rc;
  }

  /* Bark, I'm the dog.*/
  _mw_copy_on_stdout(FALSE);

  fclose (stdin);
  fclose (stdout);
  fclose (stderr);

   /* we nice us down a bit, just in case. */
  nice(10);
  ipcmain->lastactive = time(NULL);
  inst_sighandlers();
  Info("MidWay WatchDog daemon startup  complete");
  rc = run_watchdog();
  Info("MidWay WatchDog daemon shutdown complete");
  exit(0);
};
