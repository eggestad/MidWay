#include <MidWay.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/utsname.h>

#define Debug(fmt, ...)    mwlog(MWLOG_DEBUG, "%s(%d) " fmt, __FUNCTION__, __LINE__, ## __VA_ARGS__ )
#define Info(fmt, ...)    mwlog(MWLOG_INFO, fmt, ## __VA_ARGS__ )
#define Warning(fmt, ...) mwlog(MWLOG_WARNING, fmt, ## __VA_ARGS__ )
#define Error(fmt, ...)   mwlog(MWLOG_ERROR, fmt, ## __VA_ARGS__ )

#define Fatal(fmt, ...)   do { mwlog(MWLOG_FATAL, fmt, ## __VA_ARGS__ ); abort(); } while (0)

char hostname[1024];
time_t boottime = 0;
struct utsname uts;

int ping(mwsvcinfo * si)
{
   char buffer[1024];
   time_t uptime;
   int days;
   int hours, mins, secs;
   uptime = time(NULL) - boottime;

   Debug ("uptime = %d boot time %d", uptime, boottime);

   days = uptime / (24*3600);
   uptime -= days *  24*3600;
   Debug ("days %d uptime = %d", days, uptime);
   hours = uptime / 3600;
   uptime -= hours * 3600;
   Debug ("hours %d uptime = %d", hours, uptime);
   mins = uptime / 60;
   uptime -= mins * 60;
   Debug ("mins %d uptime = %d", mins, uptime);
   secs = uptime;
   Debug ("secs %d uptime = %d", secs, uptime);

   snprintf (buffer, 1024, "%s %s %s %s %s %s up %d days %2d:%02d:%02d", 
	     hostname, uts.nodename, uts.sysname, uts.release, uts.version, uts.machine, 
	     days, hours, mins, secs);

   mwreturn(buffer, 0, MWSUCCESS, 0);
}

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
  Debug ("tms_start (%d %d %d %d) %d\n", 
	  tms_start.tms_utime, tms_start.tms_stime, tms_start.tms_cutime, tms_start.tms_cstime, clks);

  if (si->datalen <= 0) {
    mwreturn(NULL, 0, MWFAIL, 0);
  };
  
  Debug (">> %s\n", si->data);
  fp = popen(si->data, "r");
  if (fp == NULL) {
    mwreturn(NULL, 0, FALSE, 0);
  };

  do {
    buffer[0] = '\0';
    fgets(buffer, 1024, fp);
    l = strlen(buffer);
    if (l <= 0) break;
    Debug (">> %s\n", buffer);
    mwreply(buffer, 0, MWMORE, 0, 0);
  } while (l > 0);

  gettimeofday(&tv_end,NULL);

  rc = pclose(fp);

  clks = times (&tms_end);  
  Debug ("tms_end (%d %d %d %d) %d\n", 
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

int _init(void)
{
   char svcname[1024];
   int rc;
   FILE * fp = NULL;
   struct timeval now;
   double uptime = 0, idletime, dnow;
   char * stmp;

   Info ("Shell server starting");

   fp = fopen("/proc/uptime", "r");
   if (fp != NULL) {
     rc = fscanf(fp, "%lf %lf", &uptime, &idletime);
     Debug ("parsing uptime => %d elmn (%f %f)", rc, uptime, idletime);
     fclose(fp);
   } else {
      Debug ("failed to open /proc/uptime errno = %d", errno);
   }

   Debug ("uptime = %f", uptime);
   
   if (uptime <= 0) uptime = time(NULL);

   gettimeofday(&now, NULL);
   dnow = now.tv_usec;
   dnow /= 1e6;
   dnow += now.tv_sec;
   boottime = dnow - uptime;
   
   Debug("boot time %s", ctime(&boottime));

   rc = gethostname (hostname, 1024);
   stmp = strchr(hostname, '.');
   if (stmp != NULL) *stmp = '\0';
   Info ("hostname = \"%s\"", hostname);

   if (rc == -1) Fatal("Failed to get hostname!");
  
   uname(&uts);

   mwprovide("shell", shell, 0);
   sprintf(svcname, "shell-%s", hostname);
   mwprovide(svcname, shell, 0);


   mwprovide("ping", ping, 0);
   sprintf(svcname, "ping-%s", hostname);
   mwprovide(svcname, ping, 0);

};


int _fini(void)
{
   char svcname[1024];
   int rc;

   Info ("Shell server stopping");
   mwunprovide("shell");
   sprintf(svcname, "shell-%s", hostname);
   mwunprovide(svcname);


   mwunprovide("ping");
   sprintf(svcname, "ping-%s", hostname);
   mwunprovide(svcname);
   
};


/*
 * Local variables:
 *  compile-command: "gcc -I../../include -nostdlib -shared -fpic -o shell.so shell.c -L../lib -lMidWay"
 * End:
 */
