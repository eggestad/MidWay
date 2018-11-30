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


/* Our primary need is to find out if a process exists. 
   We can't use kill(pid, 0) since it only works for procsesses of same uid.
   
   THis is fairly OS dependant, on linux we depend on /proc.
   if /proc is not mounted we return -ENODEV, 0 on no proc, 1 if proc exists.
*/


#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>

#include <MidWay.h>

int _mw_procowner(pid_t pid, uid_t * uid) 
{ 
  struct stat pstat;
  char path[5+10];
  int rc;

  sprintf (path, "/proc/%d", pid);

  rc = stat(path, &pstat);

  if (rc == 0) {
    if (uid != NULL) *uid = pstat.st_uid;
    return 1;
  };

  if (errno == ENOENT) return 0;
  return -ENODEV;
};
