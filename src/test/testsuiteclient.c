#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>

#include <MidWay.h>

#include "testsuite.h"



int main(int argc, char ** argv)
{
  int rc;
  struct testdata td, *rd;
  int i, appcode, len;
  int count;
  char * rbuffer = NULL;
  struct timeval btv, etv;

  float resptime, proctime;
  float respmin = 1000, respmax = 0, respavg = 0; 
  float procmin = 1000, procmax = 0, procavg = 0; 

  rbuffer = (char *) &rd;

  rc = mwattach(NULL, argv[0], 0);
  if (rc != 0) {
    printf("mwattach() failed with rc = %d\n", rc);
    exit(rc);
  }
  

  td.msleep = 0;
  /* write & fsync. If we write in chunks of td->writeamount / td->fsyncs
   and fdatasync() betwwen every chunk */

  /* spinn */
  td.mspinn = 0;
  
  /* random read */

  

  /* do the call */
  len = sizeof(struct testdata);
  for (count = 0; count < 100; count++) {
    
    gettimeofday(&btv,NULL);
    rc = mwcall("testsvc1", (char *) &td, len , 
	      &rbuffer, &len, 
	      &appcode, 0);
    gettimeofday(&etv,NULL);
    rd = (struct testdata*) rbuffer;
    
    resptime = (etv.tv_sec - btv.tv_sec) + 
      (float) etv.tv_usec/1000000 - (float) btv.tv_usec/1000000;
    proctime = (rd->endtv.tv_sec - rd->starttv.tv_sec) + 
      (float) rd->endtv.tv_usec/1000000 - (float) rd->starttv.tv_usec/1000000;
    printf ("call completed with rc=%d appcode=%d, responstime=%f procesingtime=%f\n",
	    rc, appcode, resptime, proctime);

    if (resptime < respmin) respmin = resptime;
    if (resptime > respmax) respmax = resptime;
    if (proctime < procmin) procmin = proctime;
    if (proctime > procmax) procmax = proctime;
    respavg += resptime;;
    procavg += proctime;
  };
  respavg /= count;
  procavg /= count;
  printf ("Response   times: min=%f max=%f average=%f\n", respmin, respmax, respavg);
  printf ("Processing times: min=%f max=%f average=%f\n", procmin, procmax, procavg);
  mwdetach();
};

