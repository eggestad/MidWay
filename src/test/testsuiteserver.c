#include <MidWay.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>

#include "testsuite.h"

int init(void)
{
  printf ("******************************testsuite server booting\n");
  fflush(stdout);
};

int testdataservice(mwsvcinfo * si)
{
  struct testdata * td;
  int i;
  char buffer [1024];
  struct timeval tv;

  if (si->datalen < sizeof(struct testdata)) {
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
  
  mwreturn(si->data, sizeof(struct testdata), TRUE, 0);
};

