/*
  MidWay
  Copyright (C) 2001 Terje Eggestad

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
 * $Log$
 * Revision 1.3  2002/10/29 23:54:50  eggestad
 * attempted to subtract debugging from timepegs
 *
 * Revision 1.2  2002/10/22 21:45:49  eggestad
 * added timepegs for measuring timeing in code, based on x86 instr rdtsc
 *
 * Revision 1.1  2002/08/09 20:50:15  eggestad
 * A Major update for implemetation of events and Task API
 *
 *
 */

/* linux return uptime in clock ticks via times(2).
        normally a clock tick is 1/100'th of a second. 
        uptime as reported by times() then roll over 2^31 every ~250 days.
        Thus on 32 bit machines we must have a 32 bit carry, 64 bit is OK.

        NB: times() even if returns an signed int, in the linux kernel, it is 
        an unsigned int, and is returned directly.
        We carry over after 31 bits.

	NB: an unsigned int is still 32 bit even on a 64 bit system,
	we're really counting on clock_t to guide us!
*/

#include <sys/time.h>
#include <sys/times.h>
#include <errno.h>
#include <string.h>

#include <MidWay.h>

static char * RCSId UNUSED = "$Id$";
static char * RCSName UNUSED = "$Name$"; /* CVS TAG */

/* the gettiimeofday in microsecs. We use microsecs and not mlillisecs
   since setrealtimer takes microsecs. */
long long _mw_llgtod(void)
{
  long long res;
  struct timeval tv;

  gettimeofday(&tv, NULL);
  res = tv.tv_sec; 
  res *= 1e6;
  res += tv.tv_usec;
  return res;
};
 
unsigned long long _mw_lltimes(void)
{
  clock_t jiff;
  static unsigned long long last = 0;
  static unsigned long long now;

  jiff = times(NULL);
        
  /* if 64 (or larger) bit platform,  */
  if (sizeof(clock_t) >= 8) {
    return jiff;
    /* if 32 bit */
  } else if (sizeof(clock_t) == 4) {
    now = last;
    jiff &= 0x7fffffff; /* lower 31 bits */
    now &= ~0x7fffffff;
    now |= jiff;
    /* inc "carry" */
    while(now < last) now += 0x80000000;
  }
  last = now;
  return now;
};

/* compatibility wrapper function. setitimer is not POSIX...  */
void _mw_setrealtimer(long long usecs)
{
  struct itimerval itv;
  int rc;

  if (usecs > 0)
    usecs = usecs - _mw_llgtod();
  else if (usecs < 0)
    Fatal("attemot to set timer with negative timeout");
  // implicit else usecs = 0; 0 to setitimer means turn it of

  /* just in case the timer expirsed before we manage to set it, we
     set it just 10us into the future */
  if (usecs <= 0) usecs = 10;

  /* the timer shall stop after expire */
  itv.it_interval.tv_sec = 0;
  itv.it_interval.tv_usec = 0;

  /* recalc a 64 bit usecs to two 32/64 bit sec and usec values */
  itv.it_value.tv_sec = usecs / (long) 1e6;
  itv.it_value.tv_usec = usecs % (long) 1e6;

  DEBUG1("usecs = %lld tv = { %d . %d }", usecs, 
	itv.it_value.tv_sec, itv.it_value.tv_usec);

  rc = setitimer(ITIMER_REAL,  &itv, NULL);

  if (rc != 0) {
    Fatal("failed to set the real timer %lld usecs into the future, reason, %s", 
	  usecs, strerror(errno));
  };

  return;
};

/************************************************************************
 * time pegs 
 ************************************************************************/
#ifdef TIMEPEGS

static inline long long  rt_sample(void)
{
  long long val;
  asm volatile ("rdtsc"  : "=A" (val));
  return val;
};

static long long last, debug_subtract, _pause;

struct perfentry {
  char * file;
  char * function;
  int line;
  char * note; 
  long long timediff;
};

#define MAXPEGS 100

struct perfdata {
  struct perfentry perfarray[MAXPEGS];
  int perfidx;
};


#ifdef USETHREADS
static pthread_key_t data_key;
static int pd_init = 0;
#else 
struct perfdata * pd = NULL;
#endif

void _perf_pause(void)
{
  _pause = rt_sample();
};

void _perf_resume(void)
{
  debug_subtract += rt_sample() - _pause;
};

void timepeg_clear(void)
{
  int i;
#ifdef USETHREADS
  struct perfdata * pd;
  
  if (!pd_init++) pthread_key_create(&data_key, NULL);

  pd = pthread_getspecific(data_key);
  if (pd == NULL) {
    pd = malloc(sizeof(struct perfdata));
    memset(pd, '\0', sizeof(struct perfdata));
    pthread_setspecific(data_key, pd);
  };
#endif

  last = debug_subtract = 0;

  pd->perfidx = 0;
  for (i = 0; i < MAXPEGS; i++) {
    pd->perfarray[i].file = NULL;
    pd->perfarray[i].function = NULL;
    pd->perfarray[i].line = 0;
    pd->perfarray[i].note = NULL;
  };
};


void  __timepeg(char * function, char * file, int line, char * note)
{
  struct perfentry * pe;
  long long now;
#ifdef USETHREADS
  struct perfdata * pd;
  
  if (!pd_init++) pthread_key_create(&data_key, NULL);

  pd = pthread_getspecific(data_key);
  if (pd == NULL) {
    pd = malloc(sizeof(struct perfdata));
    memset(pd, '\0', sizeof(struct perfdata));
    pthread_setspecific(data_key, pd);
  };
#endif

  if (pd->perfidx >= MAXPEGS-1) return;
  now = rt_sample();
  if (pd->perfidx == 0) 
    last = now;
  

  pe = & pd->perfarray[pd->perfidx];

  pe->timediff = now - last;// - debug_subtract;; 
  last = now;
  debug_subtract = 0;

  pe->file = file;
  pe->function = function;
  pe->line = line;
  pe->note = note;
  //  fprintf (stderr, "set timepeg %d\n", perfidx);
  pd->perfidx++;
};

int timepeg_sprint(char * buffer, size_t size)
{
  int i, l;
  long long t;
#ifdef USETHREADS
  struct perfdata * pd;
  
  if (!pd_init++) pthread_key_create(&data_key, NULL);

  pd = pthread_getspecific(data_key);
  if (pd == NULL) {
    pd = malloc(sizeof(struct perfdata));
    memset(pd, '\0', sizeof(struct perfdata));
    pthread_setspecific(data_key, pd);
  };
#endif


  l = 0;
  l += snprintf(buffer + l, size - l, 
		"\n     Entry:         File         :      Function       (line)   delta    note");
  for (i = 0; i < pd->perfidx; i++) {    
    t = pd->perfarray[i].timediff;
    if ((size - l) < 100) return l;
    l += snprintf(buffer + l, size - l, "\n     %5d:%22s:%22s(%d) + %8lld  \"%s\"", 
		  i,
		  pd->perfarray[i].file, 
		  pd->perfarray[i].function, 
		  pd->perfarray[i].line, 
		  t,
		pd->perfarray[i].note?pd->perfarray[i].note:"");
  };
  
  return l;
};
  
void timepeg_log(void)
{
  char buffer[(MAXPEGS * 256) + 1024];
  timepeg_sprint(buffer, (MAXPEGS * 128) + 1024);
  Info("TIMEPEGS: %s", buffer);
  timepeg_clear();
};

#endif
