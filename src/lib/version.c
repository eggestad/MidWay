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

/*
 * $Id$
 * $Name$
 * 
 * $Log$
 * Revision 1.5  2002/07/07 22:35:20  eggestad
 * *** empty log message ***
 *
 * Revision 1.4  2002/02/17 14:24:22  eggestad
 * added missing include
 *
 * Revision 1.3  2001/10/03 22:46:09  eggestad
 * mem corruption fixes
 *
 * Revision 1.2  2000/07/20 19:38:37  eggestad
 * prototype fix up.
 *
 * Revision 1.1.1.1  2000/03/21 21:04:17  eggestad
 * Initial Release
 *
 * Revision 1.1.1.1  2000/01/16 23:20:12  terje
 * MidWay
 *
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <MidWay.h>

/* MAGIC are used to tag the ipcmain shm segment. */
#define MAGIC "MW10"

static char * RCSId = "$Id$";
static const char * Name = "$Name$";

/* RCS Name are to be on the format Name:  TYPE_M_N_P , 
   where TYPE is either RELEASE, ALPHA, BETA, or EXPERIMENTAL (default).
   M, N, P are the version sumber, Major, minor, patchlevel.
*/

char * major = NULL, * minor = NULL, * patch = NULL;
const char * mwversion(void) 
{
  int len;
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
  } else {
    * vmaj = atoi(major);
    * vmin = atoi(minor);
    * ptcl = atoi(patch);
  }
};

const char * _mwgetmagic(void)
{
  return MAGIC;
};
