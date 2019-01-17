/*
  MidWay
  Copyright (C) 2001-4 Terje Eggestad

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

/** @file
   This is a collection of utility function to be use through out MidWay. 
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
#include <unistd.h>
#include <errno.h>
//#include <threads.h>

#include <MidWay.h>

/**
   The gettiimeofday in microsecs. We use microsecs and not mlillisecs
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
 
/**
   Get the time in ticks since an arbitrary point. This function is a
   wrsapper to times(2) bit it returns always a 64 bit int, and can't
   wrap-around like times(2).
   @return the number of ticks. 
*/
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

static void printitv(struct itimerval * itv) {
   DEBUG1("itimeval interval: %ld.%06ld value: %ld.%06ld", 
	  itv->it_interval.tv_sec, (long) itv->it_interval.tv_usec, 
	  itv->it_value.tv_sec, (long) itv->it_value.tv_usec);
};
/**
   Set at timer (wall clock). A wrapper to setitimer.  

   @param usecs the time into the future the timer shall strick, in
   micro seconds. 0 disable the timer.
 */
void _mw_setrealtimer(long long usecs)
{
  struct itimerval itv;
  int rc;

  // this is to avoid a race condition. It's posssible that within
  // the mainloop a signal occur just after the enabling of the
  // alarm signal just after mwdotask() to the msgrcv() in
  // parse_request() is called. By resetting the timer here to a
  // long time (1 minute) into the future we ensure that we got a
  // way out of the race.

  DEBUG1("set realtimer to %lldus (%f s)", usecs, (1.0 * usecs) / 1e6);

  itv.it_interval.tv_sec = 60;
  itv.it_interval.tv_usec = 0;

  if (usecs < 0)
    Fatal("attempted to set timer with negative timeout");

  if (usecs == 0) {
     itv.it_value.tv_sec = 0;
     itv.it_value.tv_usec = 0;

     itv.it_interval.tv_sec = 0;
     itv.it_interval.tv_usec = 0;
     
     DEBUG1("Disable timer");

     printitv(&itv);
     rc = setitimer(ITIMER_REAL,  &itv, NULL);
     if (rc != 0) {
	Fatal("failed to disable the real timer, reason, %s", 
	      strerror(errno));
     };
     return;
  };
  
  // convert into usec into the future we shall expire. 
  usecs = usecs - _mw_llgtod();

  DEBUG1("expire in %lld us", usecs);
  // minimun 1ms
  if (usecs < 1000)
     usecs = 1000;

  /* recalc a 64 bit usecs to two 32/64 bit sec and usec values */
  itv.it_value.tv_sec = usecs / (long) 1e6;
  itv.it_value.tv_usec = usecs % (long) 1e6;

  printitv(&itv);
  rc = setitimer(ITIMER_REAL,  &itv, NULL);

  if (rc != 0) {
    Fatal("failed to set the real timer %lld usecs into the future, reason, %s", 
	  usecs, strerror(errno));
  };

  return;
};


/**
   Get an random integer. 
   @param bound the uppper bound. 
   @return a number between 0 and bound-1 
*/
int _mw_irand(int bound)
{
   static int seed = -1;
   double upper = bound;

   if (seed == -1) {
      seed = time(NULL) + getpid(); // good enough
      srand(seed);
   };

   return (int) (upper*rand()/(RAND_MAX+1.0));
};   

/************************************************************************
 * time pegs 
 ************************************************************************/
#ifdef TIMEPEGS

// this works on both ia32 and x86-64.
static inline long long  rt_sample(void)
{
  long long val;
  long msb, lsb;
  asm volatile ("rdtsc"  : "=d" (msb), "=a" (lsb));
  val = msb;
  val <<= 32;
  val += lsb;
  return val;
};

#if 0
// this works on x86, but not x86-64.
static inline long long  rt_sample(void)
{
  long long val;
  asm volatile ("rdtsc"  : "=A" (val));
  return val;
};
#endif

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

/// @todo use gcc __thread instead for gcc that support it

#ifdef USETHREADS
static pthread_key_t data_key;
static int pd_init = 0;
#else 
struct perfdata * pd = NULL;
#endif


/**
   \ingroup timing
   Pause the timing. The cycles between a pause and resume is not included in the timing info. 
   @{
*/
void _perf_pause(void)
{
  _pause = rt_sample();
};

/**
   Resume the timing. The cycles between a pause and resume is not included in the timing info. 
*/
void _perf_resume(void)
{
  debug_subtract += rt_sample() - _pause;
};

/**
   Clear the timepeg table. Used after printing the table to start another timeing trace. 
*/
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

/**
   Records a timepeg for the gived function, file, line, and
   note. This is not inteded for direct use. Use the #TIMEPEG, or
   #TIMEPEGNOTE instead, they are wrappers.
*/
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

/**
   Print the timepeg trace table to the given buffer. 
   @param buffer a buffer to write the table to. 
   @param size the size of buffer
   @return the number of bytes actually written to buffer, if returning size, the print was crpped. 
*/
int timepeg_sprint(char * buffer, size_t size)
{
  int i, l;
  long long t, sum = 0;
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

  if (pd->perfidx <= 0)  {
    buffer[0] = '\0';
    return;
  };

  l = 0;
  l += snprintf(buffer + l, size - l, "\n     Entry:         File         :      Function       (line)   delta    note");

  for (i = 0; i < pd->perfidx; i++) {    
    t = pd->perfarray[i].timediff;
    if ((size - l) < 100) return l;

    l += snprintf(buffer + l, size - l, "\n     %5d:%22s:%22s(%d) + %8lld  \"%s\"", 
		  i,
		  pd->perfarray[i].file?pd->perfarray[i].file:"------", 
		  pd->perfarray[i].function?pd->perfarray[i].function:"------", 
		  pd->perfarray[i].line, 
		  t,
		  pd->perfarray[i].note?pd->perfarray[i].note:"");
    sum += t;
  };
  l += snprintf(buffer + l, size - l, "\n                                                        = %8lld", sum);

  return l;
};
  
static char tpbuffer [(MAXPEGS * 256) + 1024];
/** Log the timepeg trace table with a Info(). */
void timepeg_log(void)
{
   int l, n;
  l = (MAXPEGS * 256) + 1024;

  n = timepeg_sprint(tpbuffer, l);
  Info ("TIMEPEGS: (%d/%d) %s", n, l, tpbuffer);
  timepeg_clear();
};
/* @} */
#endif

#ifdef DEBUG_MALLOC

struct alloc {
   void * addr;
   size_t size;
   char * file;
   int line;
};

void * debug_malloc(char * file, int line, size_t size)
{
  void * nptr;
  nptr =  x_malloc(size);
  mwlog(MWLOG_DEBUG2, "@@@@@@@@@ %s:%d @@@@@@@@@    malloc(%d) => %p\n", file, line, size, nptr);
  return nptr;
};

void * debug_realloc(char * file, int line, void *ptr, size_t size)
{
  void * nptr;
  nptr = x_realloc(ptr, size);
  mwlog(MWLOG_DEBUG2, "@@@@@@@@@ %s:%d @@@@@@@@@    realloc (%p, %d) => %p\n", file, line, ptr, size, nptr);
  return nptr;
};

void debug_free(char * file, int line, void *ptr)
{
  mwlog(MWLOG_DEBUG2, "@@@@@@@@@ %s:%d @@@@@@@@@    freeing %p\n", file, line, ptr);  
  free(ptr);
};

char * debug_strdup(char * file, int line, char * ptr)
{
   ptr = strdup(ptr);
   mwlog(MWLOG_DEBUG2, "@@@@@@@@@ %s:%d @@@@@@@@@    strdup => %p\n", file, line, ptr);  
   return ptr;
};

#endif

char errmsgbuffer[256] = { 0 }; 
char * _mw_errno2str() {
   printf("xxxx");
   char * label = "E?";

   printf("%s(%d) %s\n", "", 0, "");
   printf("%s(%d) %s\n", label, errno, "");
   switch (errno) {
      
#include "errnos.h"
   }
   printf("%s(%d) %s\n", label, errno, "");
      printf("%s(%d) %s\n", label, errno, strerror(errno));

   sprintf(errmsgbuffer, "%s(%d) %s", label, errno, strerror(errno));
   printf("%s(%d) %s\n", label, errno, errmsgbuffer);
   printf("%s(%d) %p\n", label, errno, errmsgbuffer);
   return &errmsgbuffer[0];
}
