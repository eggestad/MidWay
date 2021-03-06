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


unsigned long long lltimes(void)
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

/* compataibility wrapper function. setitimer is not POSIX...  */
void setrealtimer(long long usecs)
{
  struct itimerval itv;
  int rc;

  /* the timer shall stop after expire */
  itv.it_interval.tv_sec = 0;
  itv.it_interval.tv_usec = 0;

  /* recalc a 64 bit usecs to two 32/64 bit sec and usec values */
  itv.it_value.tv_sec = usecs / (long) 1e6;
  itv.it_value.tv_usec = usecs % (long) 1e6;

  DEBUG("usecs = %lld tv = { %d . %d }", usecs, 
	itv.it_value.tv_sec, itv.it_value.tv_usec);

  rc = setitimer(ITIMER_REAL,  &itv, NULL);

  if (rc != 0) {
    Fatal("failed to set the real timer %lld usecs into the future, reason, %s", 
	  usecs, strerror(errno));
  };

  return;
};


