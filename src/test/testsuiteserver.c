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

#include <MidWay.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>

#include "testsuite.h"

int testdataservice(mwsvcinfo * si);

 
int testdataservice(mwsvcinfo * si)
{
  struct testdata * td;
  int i;

  mwlog(MWLOG_INFO, "Startying %s", si->service);

  mwlog(MWLOG_INFO, "data %s(%d)", si->data, si->datalen);

  if (si->datalen < sizeof(struct testdata)) {
     mwlog(MWLOG_INFO, "data (%d)", sizeof(struct testdata));
     mwreturn(NULL, 0, FALSE, 0);
  };
  td = (struct testdata *) si->data;
  /* timestamp when we started to process request */
  gettimeofday(&td->starttv,NULL);
  /* if we're told to sleep do so (millisecs) */
  if (td->msleep != 0) usleep(td->msleep*1000);
  /* write & fsync. If we write in chunks of td->writeamount / td->fsyncs
   and fdatasync() betwwen every chunk */

  /* spinn */
  for (i=0; i < (td->mspinn*1000); i++); /* no op */
  
  /* random read */

  /* timestamp when we end processing request */
  gettimeofday(&td->endtv,NULL);

  mwlog(MWLOG_INFO, "Ending %s", si->service);
  mwreturn(si->data, sizeof(struct testdata), TRUE, 0);
};

int test_svc_time(mwsvcinfo * si) {
   
   char * buf = mwalloc(100);
   time_t t;
   time(&t);
   ctime_r(&t, buf);

   mwreturn (buf, 0, MWSUCCESS, 0);
}

int test_svc_chargen(mwsvcinfo * si) {
   int lines = 50;
   
   if (si->datalen > 0) {
      int i = atoi(si->data);
      if (i > 0) lines = i;
   }

   int buflen = lines * 75+10;
   char * buf = mwalloc(buflen);

   size_t replylen = chargen(buf, buflen);

   mwreply (buf, replylen, MWSUCCESS, 0, 0);   
}

int test_svc_echo(mwsvcinfo * si) {

   
   if (si->datalen == 0) {
      mwreply ("", 0, MWSUCCESS, 0, 0);
      return 0;
   }

   char * buf = mwalloc(si->datalen);
   memcpy(buf, si->data, si->datalen);

   mwreply (buf, si->datalen, MWSUCCESS, 0, 0);   
}
     
int test_svc_sleep(mwsvcinfo * si) {
   int i = 0;
     
   if (si->datalen > 0) {
      i = atoi(si->data);
      if (i < 0) i = 0;
   }
   sleep(i);
   char * buf = mwalloc(80);
   sprintf(buf, "sleept for %d seconds", i);
  
   mwreply (buf, 0, MWSUCCESS, 0, 0);   
}
   

   
__attribute__((constructor))  int xinit(void)  
{
  printf ("******************************testsuite server booting\n");
  fflush(stdout);

  mwprovide("testsvc1", testdataservice, 0);
  mwprovide("testdate", testdataservice, 0);
  mwprovide("testtime", test_svc_time, 0);
  mwprovide("testsleep", test_svc_sleep, 0);
  mwprovide("testecho", test_svc_echo, 0);
  
  mwprovide("testchargen", test_svc_chargen, 0);
  
  return 0;
 };

