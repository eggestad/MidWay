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
 * $Id$
 * $Name$
 * 
 * $Log$
 * Revision 1.4  2004/05/31 19:48:33  eggestad
 * main API changes
 *
 * Revision 1.3  2000/09/21 18:45:56  eggestad
 * bug fix: url for IPC no not contain //
 *
 * Revision 1.2  2000/07/20 19:37:10  eggestad
 * new test service test
 *
 * Revision 1.1  2000/03/21 21:29:01  eggestad
 * Still some MiddleWay references that I had not cleaned up.
 *
 * Revision 1.1.1.1  2000/03/21 21:04:17  eggestad
 * Initial Release
 *
 * Revision 1.1.1.1  2000/01/16 23:20:12  terje
 * MidWay
 *
 */

static char * RCSId = "$Id$";
static char * RCSName = "$Name$"; /* CVS TAG */


#include <MidWay.h>
#include <signal.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
 #include <unistd.h>
#include <sys/types.h>

int serverhandler1(mwsvcinfo * si)
{
  char buffer [1024];
  struct timeval tv;
  printf (" server handler 1 called\n");
  printf ("  Clientid %d\n", si->cltid);
  printf ("  Serverid %d\n", si->srvid);
  printf ("  Service %s\n", si->service);
  printf ("  Data buffer (%d) : \"%.*s\" \n", si->datalen, si->datalen, si->data);
  printf ("  flags %#x\n", si->flags);

  sprintf(buffer, "%.*s",  si->datalen, si->data);
  if (strcmp(buffer, "ok") == 0) {
    mwreply ("Kapla", 0, TRUE, 666, 0);
    return MWSUCCESS;
  };
  if (strcmp(buffer, "nack") == 0) {
    mwreply ("Kapla", 0, FALSE, 666, 0);
    return MWSUCCESS;
  };
  if (strcmp(buffer, "forward") == 0) {
    mwforward ("test1", "ok", 0, 0);
    return MWSUCCESS;
  };
  if (strcmp(buffer, "date") == 0) {
    gettimeofday(&tv, NULL);
    
    sprintf (buffer, "%s - %d.%6.6d", ctime(&tv.tv_sec), tv.tv_sec, tv.tv_usec);
    mwreply (buffer, 0, TRUE, 666, 0);
    return MWSUCCESS;
  };
  
  return MWFAIL;
};

int serverhandler0(mwsvcinfo * si)
{
  char buffer [1024];
  time_t t;
  time(&t);
  sprintf (buffer, 
	   "test called Clientid %d Serverid %d Service %s flags %#x time=%s\n", 
	   si->cltid & MWINDEXMASK, 
	   si->srvid & MWINDEXMASK, 
	   si->service, 
	   si->flags, ctime(&t));

  mwreply (buffer, 0, TRUE, 666, 0);
  return MWSUCCESS;
};

void cleanup()
{
  mwdetach();
};

void sighandler(int sig)
{
  exit(-sig);
};

int main()
{
  
  int rc ;
  int ipckey;
  char url [1024];
  mwsetlogprefix("testmidway");
  
  mwsetloglevel(MWLOG_DEBUG2);

  signal(SIGTERM, sighandler);
  signal(SIGQUIT, sighandler);
  signal(SIGINT, sighandler);

  /*  signal(SIGSEGV, sighandler);
 signal(SIGBUS, sighandler);
  */

  atexit(cleanup);

  ipckey = getuid();
  sprintf (url, "ipc:%d", ipckey);
  rc = mwattach(url, "firstserver", MWSERVER);
  printf("mwattached on url=\"%s\" returned %d\n", url, rc);
  if (rc != 0) exit (rc);

  rc = mwprovide ("test1", serverhandler1,0L);
  printf("mwprovide \"test1\" returned %d\n", rc);
  
  rc = mwprovide ("test", serverhandler0,0L);
  printf("mwprovide \"test\" returned %d\n", rc);
  

  mwMainLoop(0);

  rc = mwunprovide ("test1");
  printf("mwunprovide \"test1\" returned %d\n", rc);

  rc = mwunprovide ("test");
  printf("mwunprovide \"test\" returned %d\n", rc);

  rc = mwdetach();
  printf("detached rc = %d\n", rc);
  



  rc = mwattach(url, "firstserver", MWSERVER);
  printf("mwattached returned %d\n", rc);
  
  mwdetach();
  printf("detached\n");

  exit(0);

  rc = mwattach(url, "firstserver", MWSERVER);

  printf("mwattached returned %d\n", rc);
  
  mwdetach();
  printf("detached\n");



  rc = mwattach(url, "firstserver", MWSERVER);

  printf("mwattached returned %d\n", rc);
  
  mwdetach();
  printf("detached\n");



  rc = mwattach(url, "firstserver", MWSERVER);

  printf("mwattached returned %d\n", rc);
  
  mwdetach();
  printf("detached\n");



  rc = mwattach(url, "firstserver", MWSERVER);

  printf("mwattached returned %d\n", rc);
  
  mwdetach();
  printf("detached\n");


  rc = mwattach(url, "firstserver", MWSERVER);

  printf("mwattached returned %d\n", rc);
  
  mwdetach();
  printf("detached\n");


}
