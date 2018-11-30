/*
  MidWay
  Copyright (C) 2000,2001,2002 Terje Eggestad

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

#include <stdio.h>
#include <stdarg.h>

/************************************************************************
 this is a dummy module just to be able to do TIMEPEG profiling along
with libMidWay.a. If linked with it we get the timepeg funs in there,
else we get these.
************************************************************************/

void  __timepeg(char * function, char * file, int line, char * note);
void timepeg_clear(void);
int timepeg_sprint(char * buffer, size_t size);
void timepeg_log(void);

void _perf_pause(void);
void _perf_resume(void);

#pragma weak __timepeg
#pragma weak timepeg_clear
#pragma weak timepeg_sprint
#pragma weak timepeg_log

#pragma weak _perf_pause
#pragma weak _perf_resume


/* these are all dymmy in stand alone */
void  __timepeg(char * function, char * file, int line, char * note)
{
};
void timepeg_clear(void)
{
};
int timepeg_sprint(char * buffer, size_t size)
{
   return 0;
};
void timepeg_log(void)
{
};

void _perf_pause(void)
{
};
void _perf_resume(void)
{
};

