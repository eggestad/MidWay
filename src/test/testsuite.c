#include <MidWay.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>

int servicehandler1(mwsvcinfo * si)
{
  char buffer [1024];
  struct timeval tv;
  printf (" service handler 1 called\n");
  printf ("  Clientid %d\n", si->cltid);
  printf ("  Serverid %d\n", si->srvid);
  printf ("  Service %s\n", si->service);
  printf ("  Data buffer (%d) : \"%.*s\" \n", si->datalen, si->datalen, si->data);
  printf ("  flags %#x\n", si->flags);

  sprintf(buffer, "%.*s",  si->datalen, si->data);
  if (strcmp(buffer, "ok") == 0) {
    mwreply ("Kapla", 0, TRUE, 666);
    return TRUE;
  };
  if (strcmp(buffer, "nack") == 0) {
    mwreply ("Kapla", 0, FALSE, 666);
    return 0;
  };
  if (strcmp(buffer, "date") == 0) {
    gettimeofday(&tv, NULL);
    
    sprintf (buffer, "%s.%d %d.%6.6d", ctime(&tv.tv_sec), tv.tv_usec/1000, tv.tv_sec, tv.tv_usec);
    mwreply (buffer, 0, TRUE, 666);
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
  mwreply(rtime,0,TRUE,0);
  return TRUE;
};
