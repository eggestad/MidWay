#include <MidWay.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <sys/times.h>

int init(void)
{
  printf ("******************************testsuite server booting\n");
  fflush(stdout);
};

int shell(mwsvcinfo * si)
{
  int i, l, rc;
  char buffer[1025];
  FILE * fp = NULL;
  double real, user, sys;
  struct tms tms_start, tms_end;
  struct timeval tv_start, tv_end;
  int jiffies;
  clock_t clks;

  jiffies = sysconf(_SC_CLK_TCK);
  gettimeofday(&tv_start, NULL);

  clks = times (&tms_start);
  printf ("tms_start (%d %d %d %d) %d\n", 
	  tms_start.tms_utime, tms_start.tms_stime, tms_start.tms_cutime, tms_start.tms_cstime, clks);

  if (si->datalen <= 0) {
    mwreturn(NULL, 0, MWFAIL, 0);
  };
  
    printf (">> %s\n", si->data);
  fp = popen(si->data, "r");
  if (fp == NULL) {
    mwreturn(NULL, 0, FALSE, 0);
  };

  do {
    buffer[0] = '\0';
    fgets(buffer, 1024, fp);
    l = strlen(buffer);
    if (l <= 0) break;
    printf (">> %s\n", buffer);
    mwreply(buffer, 0, MWMORE, 0, 0);
  } while (l > 0);

  gettimeofday(&tv_end,NULL);

  rc = pclose(fp);

  clks = times (&tms_end);  
  printf ("tms_end (%d %d %d %d) %d\n", 
	  tms_end.tms_utime, tms_end.tms_stime, tms_end.tms_cutime, tms_end.tms_cstime, clks);


  real = tv_end.tv_usec - tv_start.tv_usec;
  real /= 1000000;
  real += tv_end.tv_sec - tv_start.tv_sec;
  user = tms_end.tms_cutime - tms_start.tms_cutime;
  sys  = tms_end.tms_cstime - tms_start.tms_cstime;
  user /= jiffies;
  sys  /= jiffies;
  sprintf (buffer, "\n\nreal %12.6f user %6.3f system %6.3f status %d\n" , real, user, sys, rc>>8);

  mwreturn(buffer, 0, MWSUCCESS, 0);
};


/*
 * Local variables:
 *  compile-command: "gcc -shared -fpic -o shell.so shell.c -lMidWay"
 * End:
 */
