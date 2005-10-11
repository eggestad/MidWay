/*
  MidWay
  Copyright (C) 2002 Terje Eggestad

  MidWay is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License as
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
 * $Log$
 * Revision 1.6  2005/10/11 22:25:31  eggestad
 * added an implicit mwdotasks() at the end of addtask
 *
 * Revision 1.5  2004/10/07 22:00:38  eggestad
 * task API updates
 *
 * Revision 1.4  2004/05/31 19:47:09  eggestad
 * tasks interrupted mainloop
 *
 * Revision 1.3  2004/04/12 23:05:24  eggestad
 * debug format fixes (wrong format string and missing args)
 *
 * Revision 1.2  2003/04/25 13:04:20  eggestad
 * - fixes to task API
 *
 * Revision 1.1  2002/08/09 20:50:15  eggestad
 * A Major update for implemetation of events and Task API
 * 
 *
 */

#include <signal.h>
#include <limits.h>
#include <errno.h>

#include <MidWay.h>
#include <utils.h>
#include <tasks.h>

static char * RCSId UNUSED = "$Id$";
static char * RCSName UNUSED = "$Name$";


/* wonder if gtod return us on all platforms... anyway we use it here
   to measure time spend in tasks*/ 

DECLAREMUTEX(tasklock);

struct _task {
  double interval; // in sec. 
  int state;
  
  taskproto_t function;
  char * name;

  long long nextsched; // in uS
  
  int calls;
  long long timespend; // in uS
};

typedef struct _task Task;

static Task * tasklist = NULL;
static int tasks = -1;

static long long outoftasks = 0;
static long long intasks = 0;
static long long insched = 0;
static long long ts = 0, te = 0;


static long long nexttasktime;
static int runnable = 0, nexttask = 0;
static int alarm = 0;

#ifndef INT64_MAX
#define INT64_MAX 9223372036854775807LL
#endif

  
static int schedule(void)
{
  long long now;
  int first = 0, newrunnable = 0;
  int i, idx;
  Task * t;

  now = _mw_llgtod();
  nexttasktime = INT64_MAX;
  alarm = 0;

  newrunnable = 0;  

  DEBUG1("begins tasks = %d", tasks);
  for (i = 0; i < tasks; i++) {
    idx = (nexttask + i) % tasks;
    t = & tasklist[i];
    DEBUG3("scheduling task #%d \"%s\"", i, t->name); 
    switch (t->state) {
      
    case TASK_RUN:
      DEBUG3(" runnable");
      newrunnable++;
      break;
      
    case TASK_WAIT:
      if ((t->nextsched+100) <= now) {
	DEBUG3(" switching to runnable");
	t->state = TASK_RUN; 
	newrunnable++;
      } else {
	DEBUG3(" %s in wait: %lld micro til next schedule", t->name, t->nextsched - now);
	if (t->nextsched < nexttasktime) {
	  nexttasktime = t->nextsched ;
	  first = idx;
	};	
      };
      break;
      
    case TASK_SUSP:
      DEBUG3(" suspended");
      break;

    default:
      Error("task %d has illegal state, placing it in suspend",i);
      t->state = TASK_SUSP;
    };
  };

  nexttask = first;
  if (newrunnable) {
    runnable = newrunnable; 
    _mw_setrealtimer(0);
    DEBUG1("schedule => runnable = %d newrunnable = %d", runnable, newrunnable);
  } else {
    _mw_setrealtimer(nexttasktime);
    DEBUG1("new schedule %lld, in %lld us", nexttasktime, nexttasktime - _mw_llgtod());
    runnable = 0;
  };
  DEBUG1("end");
  return newrunnable;
};

static void (*chain)(int sig) = NULL;

static void _sighandler(int sig)
{
  if (sig !=  SIGALRM) return;
    
  alarm = 1;
  if (chain != NULL) chain(sig);
  return;    
};

static inline void alarmblock(void)
{
  sigset_t sa_mask;
  
  sigemptyset(&sa_mask);
  sigaddset(&sa_mask, SIGALRM);

  DEBUG("blocking ARLM");
  sigprocmask(SIG_BLOCK, &sa_mask, NULL);
};

static inline void alarmunblock(void)
{
  sigset_t sa_mask;
  
  sigemptyset(&sa_mask);
  sigaddset(&sa_mask, SIGALRM);

  DEBUG1("unblocking ARLM");
  sigprocmask(SIG_UNBLOCK, &sa_mask, NULL);
};

int _mw_tasksignalled(void)
{
   DEBUG1("alarm flags is %s", alarm?"set":"clear");
   return alarm;
};

static inline int inittasks(void)
{
  struct sigaction action, oldaction;;

  if (tasks == -1) {
    action.sa_flags = 0;
    action.sa_handler = _sighandler;
    
    sigfillset(&action.sa_mask);
    DEBUG1("init task  ARLM");
    sigaction(SIGALRM, &action, &oldaction);

    /* we daisy chain the signal handler */
    if ((oldaction.sa_handler == SIG_IGN) || (oldaction.sa_handler == SIG_DFL))
      chain = oldaction.sa_handler;    

    tasks = 0;
  };

};

void mwblocksigalarm(void)
{
   sigset_t sset;

   sigemptyset(&sset);
   sigaddset(&sset, SIGALRM);

   sigprocmask(SIG_BLOCK, &sset, NULL);
   return;
};

void mwunblocksigalarm(void)
{
   sigset_t sset;

   sigemptyset(&sset);
   sigaddset(&sset, SIGALRM);

   sigprocmask(SIG_UNBLOCK, &sset, NULL);
   return;
};

static int _runnable(void) 
{
  int i, n = 0;
  for(i = 0; i < tasks; i++) {
    if (tasklist[i].state == TASK_RUN) n++;
  };
  return n;
};

int mwdotasks(void) 
{  
  long long start, end, tt;
  long long timeintask = 0;
  int i, idx, rc;
  Task * t;

  DEBUG1("begin");
  if (tasks <= 0) return 0;

  LOCKMUTEX(tasklock);

  ts = _mw_llgtod();
  if (te != 0) 
    outoftasks += (ts - te);
  
  runnable = _runnable();

  if (!runnable) {    
    DEBUG1("no task in runable state, calling schedule");
    runnable = schedule();
  } else {
    DEBUG1("dotask begin with runnable = %d of %d tasks", runnable, tasks);
  };

  if (!runnable) {
    te = _mw_llgtod();
    insched += te - ts;
    DEBUG1("end no runnable");
    goto out;
  };


  /* we go thru the tasklist starting with nexttask, and call the next task with state == TASK_RUN */  
  for(i = 0; i < tasks; i++) {
    idx = (nexttask + i) % tasks;
    DEBUG3("checking task %d, nexttask = %d", idx, nexttask);
    if (tasklist[idx].state == TASK_RUN) {
      t = &tasklist[idx];
      DEBUG1("calling task %d \"%s\"", idx, t->name);


      UNLOCKMUTEX(tasklock);
      start = _mw_llgtod();
      rc = t->function(idx+PTASKMAGIC);
      end = _mw_llgtod();
      DEBUG1("task function %d \"%s\" returned %d", idx, t->name, rc);
      LOCKMUTEX(tasklock);

      if (rc == 0)  {
	DEBUG1("task complete");
	runnable--;

	// the task map have called other mw*task*() that has changed state. 

	if (t->interval < 0) {
	   t->state = TASK_SUSP;
	   t->nextsched = 0;
	   DEBUG1("task complete suspending, runnable %d", runnable);
	} else {	   
	  if (t->state == TASK_RUN) {
	    t->state = TASK_WAIT;
	  };
	  tt = _mw_llgtod();
	  t->nextsched = tt + t->interval * 1e6;
	  DEBUG1("task complete nextsched = %lld, runnable %d", t->nextsched, runnable);
	}
      };
      tt = end - start;
      DEBUG1("time spend in his task = %lld timeintask = %lld", tt, timeintask);
      timeintask += tt;
      tasklist[idx].timespend += tt;    
      break;
    };
  };

  DEBUG1("nexttask is now %d", nexttask);
  nexttask = (idx + 1) % tasks;

  te = _mw_llgtod();
  intasks += timeintask;
  start = end = 0;

  start = _mw_llgtod();
  runnable = schedule();
  end = _mw_llgtod();
  
  DEBUG3("schedule() took %lld", end - start);
  DEBUG3("time in %s %lld", __FUNCTION__, (te - ts));
  insched += (end - start);
  DEBUG1("dotasks competes with runnable = %d outoftask = %fs, intasks  = %fs, insched = %fs", 
	 runnable,
	 ((double) outoftasks) / 1e6F, 
	 ((double) intasks) / 1e6F, 
	 ((double) insched) / 1e6F);
  DEBUG1("end runnable = %d", runnable);

 out:
  UNLOCKMUTEX(tasklock);

  return runnable;
};


//int mwchtask(PTask pt, int cmd, void * data)

/* if we for some reason should need to wake a task before it's
   interval, we can call this. */
int mwwaketask(PTask pt)
{
  Task * t;
  int idx;

  idx = pt - PTASKMAGIC;
  if (tasks < 0) return -EILSEQ;
  if (tasks == 0) return -ENOENT;
  if (idx >= tasks) return -ENOENT;

  LOCKMUTEX(tasklock);

  t = &tasklist[idx];

  DEBUG1("waking task %d \"%s\"", idx, t->name);
  t->state = TASK_RUN;
  t->nextsched =  _mw_llgtod();
  runnable++;

  nexttask = idx;
  _mw_setrealtimer(0);

  DEBUG1("waking task done");

  UNLOCKMUTEX(tasklock);
  return 0;
};

int mwsuspendtask(PTask pt)
{
   Task * t;
   int idx;

   idx = pt - PTASKMAGIC;
   if (tasks < 0) return -EILSEQ;
   if (tasks == 0) return -ENOENT;
   if (idx >= tasks) return -ENOENT;

   LOCKMUTEX(tasklock);
  
   t = &tasklist[idx];

   DEBUG1("suspending task %d", idx);
   
   t->state = TASK_SUSP;
   runnable = schedule();
   DEBUG1("suspending task complete");

   UNLOCKMUTEX(tasklock);  
   return 0;
};

int mwresumetask(PTask pt)
{
   Task * t;
   int idx;

   idx = pt - PTASKMAGIC;
   if (tasks < 0) return -EILSEQ;
   if (tasks == 0) return -ENOENT;
   if (idx >= tasks) return -ENOENT;

   LOCKMUTEX(tasklock);

   t = &tasklist[idx];

   DEBUG1("resuming task %d, interval = %g", idx, t->interval);
   
   if (t->interval > 0) {
     t->state = TASK_WAIT;
     runnable = schedule();
   };
   DEBUG1("resume task complete");

   UNLOCKMUTEX(tasklock);
   return 0;
};

int mwsettaskinterval (PTask pt, double interval) 
{
  Task * t;
  double oldinterval;
  int idx;
  long long now, timetonext;
  
  idx = pt - PTASKMAGIC;
  if (tasks < 0) return -EILSEQ;
  if (tasks == 0) return -ENOENT;
  if (idx >= tasks) return -ENOENT;

  LOCKMUTEX(tasklock);

  t = &tasklist[idx];
  now = _mw_llgtod();

  oldinterval = t->interval;
  DEBUG1("changing interval from %g to %g", t->interval, interval);

  if (interval < 0) 
  {
    interval = -1;
    t->state = TASK_SUSP;
  } else {
    t->interval = interval;
    timetonext = t->nextsched - now;
    t->nextsched += (interval - oldinterval) * 1e6;
    DEBUG1("next schedule at %lld was %lld", t->nextsched, timetonext);
  };

  DEBUG1("task interval = %gs new interval = %gs old interval = %gs", 
	 interval, t->interval, oldinterval);

  runnable = schedule();
  DEBUG1("complete");

  UNLOCKMUTEX(tasklock);
  return oldinterval;
};
   
int _mw_gettaskstate(PTask pt)
{
  Task * t;
  int idx;
  int state;

  idx = pt - PTASKMAGIC;
  if (tasks < 0) return -EILSEQ;
  if (tasks == 0) return -ENOENT;
  if (idx >= tasks) return -ENOENT;

  LOCKMUTEX(tasklock);

  t = &tasklist[idx];
  state = t->state;

  UNLOCKMUTEX(tasklock);
  
  return state;
};

PTask _mwaddtask(taskproto_t function, char * name, double interval)
{
  DEBUG1("interval = %f", interval);
  return _mwaddtaskdelayed(function, name, interval, -1);
};

/* interval & initialdelay is in ms but all times in struct is in
   us. if initdelay is -1, the first call is in inteval. If interval
   if -1 and initialdelay is -1, the task is never scheduled until
   either mwwaketask() or mwchtask() is called. if only initialdelay
   is positive, the task is called once, and is suspended after
   completion. */
PTask _mwaddtaskdelayed(taskproto_t function, char * name, double interval, double initialdelay)
{
  Task * t;
  int firstissue = -1;
  int idx;

  if (function == NULL) return  -EINVAL;

  LOCKMUTEX(tasklock);

  DEBUG1("tasks %d", tasks);
  DEBUG1("interval %g delay", interval, initialdelay);

  if (tasks == -1) inittasks();
  tasklist = realloc(tasklist, (tasks+1)*sizeof(Task));
  DEBUG1("tasklist = %p, size = %d", tasklist, (tasks+1)*sizeof(Task));
  idx = tasks;
  t = &tasklist[tasks];
  tasks++;

  if (initialdelay >= 0) firstissue = initialdelay * 1e6;
  else if (interval >= 0) firstissue = interval * 1e6;
  
  t->function = function;
  t->name = name;

  DEBUG1("adding task %d interval %gs delay %gs", tasks, interval, initialdelay);
  if (interval <= 0) { 
    t->interval = -1;
  } else {    
    t->interval = interval;
  };

  if (firstissue >= 0) {
    t->nextsched = _mw_llgtod() + firstissue;
  } else {
    t->nextsched = -1;
  };

  // if both neg, we go directly to suspend.
  if ( (interval < 0) && (initialdelay < 0))
     t->state = TASK_SUSP;
  else
     t->state = TASK_WAIT;

  t->calls = 0;
  t->timespend = 0;


  DEBUG("complete");

  UNLOCKMUTEX(tasklock);
  runnable = schedule();
  while(runnable) {
    runnable = mwdotasks() ;
  };
  return  idx + PTASKMAGIC;
};

/* Emacs C indention
Local variables:
c-basic-offset: 2
End:
*/
