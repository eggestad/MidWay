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
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>

#include <MidWay.h>

int servicehandler1(mwsvcinfo * si)
{
  char buffer [1024];
  struct timeval tv;
  printf (" service handler 1 called\n");
  printf ("  Clientid %d\n", si->cltid);
  printf ("  Serverid %d\n", si->srvid);
  printf ("  Service %s\n", si->service);
  printf ("  Data buffer (%zu) : \"%.*s\" \n", si->datalen, (int)si->datalen, si->data);
  printf ("  flags %#x\n", si->flags);

  sprintf(buffer, "%.*s",  (int)si->datalen, si->data);
  if (strcmp(buffer, "ok") == 0) {
    mwreply ("Kapla", 0, TRUE, 666, 0);
    return TRUE;
  };
  if (strcmp(buffer, "nack") == 0) {
    mwreply ("Kapla", 0, FALSE, 666, 0);
    return 0;
  };
  if (strcmp(buffer, "date") == 0) {
    gettimeofday(&tv, NULL);
    
    sprintf (buffer, "%s.%ld %ld.%6.6ld", ctime(&tv.tv_sec), tv.tv_usec/1000, tv.tv_sec, tv.tv_usec);
    mwreply (buffer, 0, TRUE, 666, 0);
    return 0;
  };
  
  return FALSE;
};


int sleep1(mwsvcinfo * si)
{
  char rtime[100];
  struct timeval tv;
  struct tm * tnow;;

  gettimeofday(&tv, NULL);
  tnow = localtime(&tv.tv_sec);
  sleep(1);

  strftime(rtime,99,"%Y%m%d %H%M%S Julian:%j Week:%W %Z %z", tnow);
  mwreply(rtime,0,TRUE,0, 0);
  return TRUE;
};
