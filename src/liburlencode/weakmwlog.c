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

/*
 * $Log$
 * Revision 1.2  2005/10/11 22:28:11  eggestad
 * Added URLENCODEVERBOSE for setting loglevel in the lib
 *
 * Revision 1.1  2003/06/12 07:17:08  eggestad
 * added weak syms for mwlog and timepegs, allowing for intergrated logging and profileing with rest of MW
 *
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

/************************************************************************
 this is a dummy module just to be able to get debugging in the midway
log output from the url lib while linked with libMidWay.a, if used
standalone we get the functions below, which uses stderr.
************************************************************************/

static int loglevel = -1;

void mwlog(int level, char * format, ...);
#pragma weak mwlog

void mwlog(int level, char * format, ...)
{
  va_list ap;
  char c;
  
  if (loglevel == -1) {
     char * env = getenv("URLENCODEVERBOSE");
     if (env) {
	loglevel = atoi(env);
     } else {
	loglevel = 0;
     };
  };
  if (loglevel == 0) return;

  va_start(ap, format);

  
  switch(level) {
  case 0:
     c = 'F';
     break;

  case 1:
     c = 'E';
     break;

  case 2:
     c = 'W';
     break;

  case 3:
     c = 'A';
     break;

  case 4:
     c = 'I';
     break;

  case 5:
     c = 'D';
     break;

  case 6:
     c = '1';
     break;

  case 7:
     c = '2';
     break;

  case 8:
     c = '3';
     break;

  case 9:
     c = '4';
     break;

  default:
     c = '.';
  };
  fprintf(stderr, "[%c] ", c);
  vfprintf(stderr, format, ap);
  fprintf (stderr, "\n");
  va_end(ap);

  return ;
};


int mwsetloglevel(int level);
#pragma weak mwsetloglevel

int mwsetloglevel(int level)
{
   int old;
   old = loglevel;
   loglevel = level;
   return old;
};



