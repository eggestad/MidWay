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
  License along with the MidWay distribution; see the file COPYING. If not,
  write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA. 
*/


#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <MidWay.h>

/* MAGIC are used to tag the ipcmain shm segment. */
#define MAGIC "MW10"

// TODO: set version some other way
static const char * Name = "$Name$";

/* RCS Name are to be on the format Name:  TYPE_M_N_P , 
   where TYPE is either RELEASE, ALPHA, BETA, or EXPERIMENTAL (default).
   M, N, P are the version sumber, Major, minor, patchlevel.
*/

char * major = NULL, * minor = NULL, * patch = NULL;
const char * mwversion(void) 
{
  /*  static char *  version = NULL; */
  static char version[1024];

  const char rcsname[] = { '$', 'N', 'a', 'm', 'e', '$', '\0' };
  const char rcsname2[] = "$Name$" ;
  char * buf, * t, * m, * n, * p;

  /*
  if (version != NULL) return version;
  */
  if (strcmp(Name , rcsname) == 0) 
    return "EXPERIMENTAL";

  if (strcmp(Name , rcsname2) == 0) 
    return "EXPERIMENTAL";

  buf = malloc(strlen(Name)+4);
  strcpy(buf, Name);
  t = strchr(buf, ' ');
  t++;
  m = strchr(t, '_');
  *m = '\0';
  m++;

  n = strchr(m, '_');
  *n = '\0';
  n++;
  
  p = strchr(n, '_');
  *p = '\0';
  p++;

  sprintf (version, "MidWay %s release version %s.%s.%s", t,m,n,p);
 
  major = m;
  minor = n;
  patch = p;   
  free (buf);
  buf = NULL;
  return version;
};
 
int _mwgetversion(int * vmaj, int * vmin, int * ptcl)
{
  if (major == NULL) {
    * vmaj = 0;
    * vmin = 0;
    * ptcl = 0;
    return -1;
  }
  * vmaj = atoi(major);
  * vmin = atoi(minor);
  * ptcl = atoi(patch);
  return 0;  
};

const char * _mwgetmagic(void)
{
  return MAGIC;
};
