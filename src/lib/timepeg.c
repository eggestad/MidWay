
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
 * $Log$
 * Revision 1.1  2002/07/07 22:45:48  eggestad
 * *** empty log message ***
 *
 */


static char * RCSId = "$Id$";
static char * RCSName = "$Name$"; /* CVS TAG */

#ifndef NDEBUG 

struct timepegdata {
  char * function;
  char * file;
  int line;
  char * note;
  long long timestamp;
};

struct timepegdata timepegarray[100];
int timepegidx  = 0;


static inline long long  rt_sample(void)
{
  long long val;
  asm volatile ("rdtsc"  : "=A" (val));
  return val;
};


void __timepeg(char * function, char * file, int line, char * note)
{
  if (timepegidx >= 99) return;

  timepegarray[timepegidx].timestamp = rt_sample();
  timepegarray[timepegidx].file = file;
  timepegarray[timepegidx].line = line;
  timepegidx++;
};


void timepeg_clear()
{
  int i;
  timepegidx = 0;
  for (i = 0; i < 100; i++) {
    timepegarray[i].function = NULL;
    timepegarray[i].file = NULL;
    timepegarray[i].line = 0;
    timepegarray[i].note = NULL;
  };
};

int timepeg_sprint(char * buffer, size_t size)
{
  int i, l = 0;
  timepegidx = 0;
  
  l += sprintf(buffer + l, "\n      Function      :   File   : Line : cycles  : Note\n");
  for (i = 0; i < 100; i++) {
    if (file == NULL) break;
    if (l >= size - 80) break;
    l += sprintf(buffer + l, "%-20.20s:%10.10s:%6d: %-7.7lld : %-30.30s\n",  
		 timepegarray[i].function, 
		 timepegarray[i].file, 
		 timepegarray[i].line, 
		 i=0?0:timepegarray[i].timestamp - timepegarray[i-1].timestamp, 
		 timepegarray[i].note==NULL?"":timepegarray[i].note);
    
  };
  return l;
};

void timepeg_log(void)
{
  char buffer[8192];

  timepeg_sprint(buffer, 8192);
  Info("Timepegs: %s\n",  buffer);
  timepeg_clear();
};

#endif  // ifndef NDEBUG


