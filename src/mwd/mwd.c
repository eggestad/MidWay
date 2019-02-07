/*
  MidWay
  Copyright (C) 2000 Terje Eggestad

  MidWay is free software; you can redistribute it and/or
  modify it under the terms of the GNU  General Public License as
  published by the Free Software Foundation; either version 2 of the
  License, or (at your option) any later version.
  
  MidWay is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.
  
  You should have received a copy of the GNU General Public License 
  along with the MidWay distribution; see the file COPYING.  If not,
  write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA. 
*/


#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <string.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>

/* needed by set_instancename to get ip address of first eth device */
#include <sys/ioctl.h> 
#include <net/if.h>
#include <sys/socket.h>
#include <arpa/inet.h>

/* NB: see below */
#include <signal.h>

#include <MidWay.h>
#include <ipcmessages.h>
#include <ipctables.h>
#include <mwserverapi.h>
#include <address.h>
#include <version.h>
#include "mwd.h"
#include "tables.h"
#include "servermgr.h"
#include "watchdog.h"
#include "xmlconfig.h"
#include "shmallocfmt.h"
#include "requestparse.h"
#include "events.h"

#include "utils.h"

/* undocumented func in lib/mwlog.c */
void _mw_copy_on_stdout(int flag);

static int maxclients =        100;
static int maxservers =         50;
static int maxservices =       100;
static int maxgateways =        20;
static int maxconversations =   20;
static int numbuffers =         -1;
static int bufferbasesize =   1024;
static key_t masteripckey =     -1;

char * uri = NULL;
static char * mwhome = NULL;
static char * mwdatadir = NULL;
char * instancename = NULL;

mode_t mw_umask = 0xffff;

static struct passwd * mepw = NULL;

static Flags flags = { 0, 0, 0, 0, 0};


// prototypes for this module
int term_maininfo(void);


void usage(void)
{
  fprintf (stderr,"\nmwd [options...] [instancename]\n\n");
  fprintf (stderr,"  where options is one of the following:\n");
  fprintf (stderr,"    -l loglevel : (error|warning|info|debug|debug1|debug2|debug3|debug4)\n");  
  fprintf (stderr,"    -A uri : uri is the address of the MidWay instance e.g. ipc://12345\n");  
  fprintf (stderr,"    -f : Start mwd in foreground not as a daemon.\n");  
  fprintf (stderr,"    -H MidWayHome : Defaults to ~/MidWay.\n");  
  fprintf (stderr,"    -D Datadir : Defaults to <MidWayHome><instancename>/data.\n");  
  fprintf (stderr,"    -c maxclients\n");  
  fprintf (stderr,"    -s maxservers\n");  
  fprintf (stderr,"    -S maxservices\n");  
  fprintf (stderr,"    -g maxgateways\n");  
  fprintf (stderr,"    -B heapsize\n");  
  fprintf (stderr,"    -C maxconversations\n");  
  fprintf (stderr,"\n");
  fprintf (stderr,"  Instancename is the name of the instance. It looks for config etc\n");
  fprintf (stderr,"    under MidWayHome/instancename/ defaults to username.\n");
  fprintf (stderr,"    MidWayHome defaults ~/MidWay unless overridden by -H \n");
  fprintf (stderr,"    or envirnoment variable MWHOME\n");
  fprintf (stderr,"\n");
  exit(1);
};

#define GLOBALDEF
#define _MWD_C

int mwdSetIPCparam(IPCPARAM param, int value)
{
  
  switch(param) {

  case MAXCLIENTS: 
    if ( (value > 0) && (value < 100000) )
      maxclients = value;
    break;

  case MAXSERVERS: 
    if ( (value > 0) && (value < 100000) )
      maxservers = value;
    break;

  case MAXSERVICES: 
    if ( (value > 0) && (value < 100000) )
      maxservices = value;
    break;

  case MAXGATEWAYS: 
    if ( (value > 0) && (value < 100000) )
      maxgateways = value;
    break;

  case MAXCONVS: 
    if ( (value > 0) && (value < 100000) )
      maxconversations = value;
    break;

  case NUMBUFFERS: 
    if ( (value > 0) && (value < 100000) )
      numbuffers = value;
    break;

  case BUFFERBASESIZE: 
    if ( (value > 0) && (value < 100000) )
      bufferbasesize = value;
    break;

  case MASTERIPCKEY:
    if ( (value > 0) && (value < 100000) )
      masteripckey = (key_t) value;
    break;

  case UMASK:
     if ( (value >= 0) && (value <= 066) ) {
	umask(value);
	mw_umask = value;
     } else if (value > 066) {
	Error("illegal umask %#o, owner mask must be 0 for MidWay to work, umask must be between 0 and 066", value);
     };
	
  default:
    return -1;
  };
  return 0;
};

int mwdGetIPCparam(IPCPARAM param)
{
  int value;

  switch(param) {

  case MAXCLIENTS: 
    value = maxclients;
    break;
  case MAXSERVERS: 
    value = maxservers;
    break;
  case MAXSERVICES: 
    value = maxservices;
    break;
  case MAXGATEWAYS: 
    value = maxgateways;
    break;
  case MAXCONVS: 
    value = maxconversations;
    break;
  case BUFFERBASESIZE: 
    value = bufferbasesize;
    break;
  case MASTERIPCKEY: 
    value = masteripckey;
    break;
  case NUMBUFFERS:
    value = numbuffers;
    break;
  case UMASK:
     if (mw_umask == 0xffff) {
	mw_umask = umask(0);
	umask(mw_umask);
     };
     value = mw_umask;
  default:
    return -1;
  };
  return value;
};

ipcmaininfo * ipcmain = NULL;

ipcmaininfo * getipcmaintable()
{
  DEBUG("lookup of ipcmain address: %p", ipcmain);
  return ipcmain;
};

  
/****************************************************************************************
 * Signal handling
 ****************************************************************************************/

static void sighandler(int sig)
{
   switch(sig) {
      
   case SIGALRM:      
      flags.alarm++;
      return;
      
   case SIGTERM:
   case SIGINT:
      mwaddtaskdelayed(do_shutdowntrigger, -1, 1.0);
      DEBUG("normal shutdown sig=%d", sig);
      return;

   case SIGHUP:
   case SIGQUIT:

      DEBUG("fast track shutdown");
      kill(sig,ipcmain->mwwdpid);
      /* fast track down */
      shm_destroy();
      term_tables();
      term_maininfo();
      exit(-1);
      
  case SIGCHLD:
    flags.childdied++;
    return;

  case SIGUSR1:
    flags.user1++;
    return;

  case SIGUSR2:
    flags.user2++;
    return;
  };
};

static void inst_sighandlers(void)
{
  struct sigaction action;
  int failed = 0;

  action.sa_flags = 0;
  action.sa_handler = sighandler;
  sigfillset(&action.sa_mask);
  
  if (sigaction(SIGTERM, &action, NULL)) failed++;
  if (sigaction(SIGHUP, &action, NULL)) failed++;
  if (sigaction(SIGINT, &action, NULL)) failed++;
  if (sigaction(SIGQUIT, &action, NULL)) failed++;
  if (sigaction(SIGCHLD, &action, NULL)) failed++;
  if (sigaction(SIGUSR1, &action, NULL)) failed++;
  if (sigaction(SIGUSR2, &action, NULL)) failed++;

  if (failed != 0) {
    Error("failed to install %d signals handlers\n", failed);
    exit(-19);
  }
};

conv_entry * getconv_entry(int i)
{
  i &= MWINDEXMASK;
  return & convtbl[i];
};

int mymqid(void)
{
  if (ipcmain == NULL) return -EUCLEAN;
  return ipcmain->mwd_mqid;
};

/* since only mwd creates and destroys shared memory and semaphores
 * we have placed this function here, and not in ipctables.c
 */

static int tablemode = 0644, /* shm tables mode is 0644 & mask */
  heapmode           = 0666, /* shm heap and semaphore mode is 0666 & mask */
  queuemode          = 0622; /* mwd message queue mode is 0622 & mask */

int mwdheapmode(void)
{
   return heapmode;
};

static int
create_ipc(int mode)
{
  int mainid;
  pid_t pid;

  /* 
     If mode is 0 we base access on umask().
     On mode we test only for read, for umask we test for read and execute. 

     We generate here a mask: 
     mask | attachability
     -------------------------
     0000 | group and other
     0060 | only other
     0006 | only group
     0066 | only owner 

  */

  mode_t um = 0, mask = 0;
  
  um =mwdGetIPCparam(UMASK);

  if (mode == 0) {
    /* just to make the arithmetic more readable we convert the mask to mode: */
    mode = 0777& ~um;
  } else {
    /* in order to fake the fact that we only test fro read on arg */
    mode |= S_IXGRP | S_IXOTH;
  };

  mask = S_IRUSR | S_IWUSR;
  printf("mode = %o umask = %o\n", mode, um); 

  /* test for group access */
  if ( ((mode&S_IRGRP) && (mode&S_IXGRP)) )
    mask |= S_IRGRP | S_IWGRP;
  /* and for other access */
  if ( ((mode&S_IROTH) && (mode&S_IXOTH)) )
    mask |= S_IROTH | S_IWOTH;
  
  if (mode > 0777) {
    Error("This can't happen, create_ipc was called with mode = 0%op",mode);
    return -EINVAL;
  };

  /* we apply the mask */
  tablemode = tablemode & mask;
  heapmode  = heapmode  & mask;
  queuemode = queuemode & mask;
  printf("tablemode = %o heapmode = %o queuemode = %o mask = %o\n",tablemode, heapmode,queuemode,mask); 

  DEBUG("shared memory for main info has ipckey = 0x%x mode=0%o",
	masteripckey, tablemode|IPC_CREAT | IPC_EXCL);


  /************************************************************************
   *  IPC parameter fixup 
   *  here we check that all values has a resonable number 
   ************************************************************************/
  if (numbuffers < 0) {
    /* this is actually an important formula. Here we we try to
       estimate the number of shm buffer chunks needed. Currently this
       is just guess work.  */
    numbuffers = (maxclients + maxservers + maxgateways) * 2 ;
  };

  if (maxservices < maxservers) maxservices = maxservers;
  if (maxservices < maxgateways) maxservices = maxgateways;

  Info("  max clients %d, servers %d, services %d, gateways %d, conversations %d\n",
	maxclients, maxservers, maxservices, maxgateways, maxconversations);

  /* main table are always only writeable by owner of mwd. */
  mainid = shmget(masteripckey, sizeof(struct ipcmaininfo), tablemode |IPC_CREAT | IPC_EXCL);
  if (mainid == -1) {
    if (errno == EEXIST) {
      /* if we got EEXIST, this really can't fail */
      mainid = shmget(masteripckey, 0, 0);
      ipcmain = (ipcmaininfo *) shmat(mainid, (void *) 0, 0);
      if ((ipcmain == NULL) || (ipcmain == (void *) -1)) {
	Error("A shared memory segment with key %#x exists, id=%d, but failet to attach it!",masteripckey, mainid);
	return -EEXIST;
      };
      pid = ipcmain->mwdpid;
      if ((pid > 1) && (kill(pid, 0) == 0)) {
	Error("The instance is already running, mwd has pid %d",pid);
	shmdt(ipcmain);
	return -EEXIST;
      }
      /* the previous died without cleaning up. */
      return -EDEADLK;
    }
    /* unknown error */
    Error("Failed to create shared memory for main info for ipckey = 0x%x reason: %s: flags 0%o",
	  masteripckey, strerror(errno),mode );
    return -errno;
  };
  DEBUG("main shm info table has id %d",mainid);

  ipcmain = (ipcmaininfo *) shmat(mainid, (void *) 0, 0);
  if (ipcmain == (void *) -1) {
    ipcmain = NULL;
    Error("Failed to attach shared memory with id = %d reason %s",
	  mainid, strerror(errno));
    return -errno;
  };
  DEBUG("main shm info table attached at %p",ipcmain);

  /* 
   * attaching all the other tables 
   */

  /*** CLIENT TABLE ***/
  ipcmain->clttbl_ipcid = shmget(IPC_PRIVATE, sizeof(struct cliententry) * maxclients,tablemode);
  if (ipcmain->clttbl_ipcid == -1) {
    Error("Failed to attach client table with id = %d reason %s",
	  ipcmain->clttbl_ipcid, strerror(errno));
    return -errno;
  };
  clttbl = (cliententry *) shmat(ipcmain->clttbl_ipcid, NULL, 0);
  DEBUG("client table id=%d attached at %p", 
	ipcmain->clttbl_ipcid,clttbl);
  
  /*** SERVER TABLE ***/
  ipcmain->srvtbl_ipcid = shmget(IPC_PRIVATE, sizeof(struct serverentry) * maxservers,
				 tablemode);
  if (ipcmain->srvtbl_ipcid == -1) {
    Error("Failed to attach Server table with id = %d reason %s",
	  ipcmain->srvtbl_ipcid, strerror(errno));
    return -errno;
  };
  srvtbl = (serverentry *) shmat(ipcmain->srvtbl_ipcid, (void *) 0, 0);
  DEBUG("Server table id=%d attached at %p",
	ipcmain->srvtbl_ipcid ,srvtbl);

  /*** SERVICE TABLE ***/
  ipcmain->svctbl_ipcid = shmget(IPC_PRIVATE, sizeof(struct serviceentry) * maxservices,
				 tablemode);
  if (ipcmain->svctbl_ipcid == -1) {
    Error("Failed to attach service table with id = %d reason %s",
	  ipcmain->svctbl_ipcid, strerror(errno));
    return -errno;
  };
  svctbl = (serviceentry *) shmat(ipcmain->svctbl_ipcid, (void *) 0, 0);
  DEBUG("service table id=%d attached at %p",
	ipcmain->svctbl_ipcid, svctbl);

  /*** GATEWAYS TABLE ***/
  ipcmain->gwtbl_ipcid = shmget(IPC_PRIVATE, sizeof(struct gatewayentry) * maxgateways,
				tablemode);
  if (ipcmain->gwtbl_ipcid == -1) {
    Error("Failed to attach gateway table with id = %d reason %s",
	  ipcmain->gwtbl_ipcid, strerror(errno));
    return -errno;
  };
  gwtbl = (gatewayentry *) shmat(ipcmain->gwtbl_ipcid, (void *) 0, 0);
  DEBUG("gateway table id=%d attached at %p",
	ipcmain->gwtbl_ipcid, gwtbl);

  /*** CONVERSATIONS TABLE ***/
  ipcmain->convtbl_ipcid = shmget(IPC_PRIVATE, sizeof(struct conv_entry) * maxconversations,
				  tablemode);
  if (ipcmain->convtbl_ipcid == -1) {
    Error("Failed to attach convserver table with id = %d reason %s",
	  ipcmain->convtbl_ipcid, strerror(errno));
    return -errno;
  };
  convtbl = (conv_entry *) shmat(ipcmain->convtbl_ipcid, (void *) 0, 0);
  DEBUG("convserver table attached at %p",convtbl);

  /*** SHARED MEMORY DATA BUFFERS ***/
  ipcmain->heap_ipcid = shmb_format(heapmode, bufferbasesize, numbuffers);
  if (ipcmain->heap_ipcid < 0) {
    Error("Failed to creat heap reason %s",
	  strerror(errno));
    return -errno;
  };
  DEBUG("buffer heap id=%d attached ",
	ipcmain->heap_ipcid);
  return 0;

}

//
// listing all the interfaces is a messy affair. The man page is obsolete. 
//
void set_instanceid(ipcmaininfo * ipcmain)
{
  
#define USE_UUID
#ifdef USE_UUID
#include <uuid/uuid.h>
  uuid_t uuid;
  uuid_generate(uuid);
#ifdef __APPLE__
  uuid_string_t uuid_str;
#else
  char uuid_str[64];
#endif
  uuid_unparse(uuid, uuid_str);
  sprintf(ipcmain->mw_instance_id, "%s", uuid_str);
  return;
#endif

  char * pt, buffer [256] = {0};
  int s, rc, idx, N, len;
  struct ifconf ifc = {0};
  struct ifreq * ifr, ifdat;
  struct sockaddr_in * sin;
  struct sockaddr hwaddr = { 0 };
  int hwaddrfound = 0;
  
  s = socket (AF_INET, SOCK_DGRAM, 0);

  if (s == -1) goto nonet;

  N = 0;
  do {
    N += 1;
    ifc.ifc_len = len = (sizeof(struct ifreq))*N+4 ;

    DEBUG ("alloc len = %d", ifc.ifc_len);

    ifc.ifc_ifcu.ifcu_buf = realloc (ifc.ifc_ifcu.ifcu_buf, ifc.ifc_len);

    /* I wonder exactly how portable this is, but ifconfig does this,
       and Stevens says this is portable */
    rc = ioctl(s, SIOCGIFCONF, &ifc);
    if (rc == -1) {
      goto out;
    };
    DEBUG (" len = %d & %d", ifc.ifc_len, len);

  } while (ifc.ifc_len >= (len-sizeof(struct ifreq)));

  
  for (idx=0; idx < N; idx++) {
    
    ifr = &ifc.ifc_ifcu.ifcu_req[idx];
    
    DEBUG("checking interface %d %s for unique address", idx, ifr->ifr_name);
    errno = 0;
    rc = ioctl(s, SIOCGIFADDR, ifr);
    DEBUG("ioctl(SIOCGIFADDR) returned %d errno %s af = %d", 
	  rc, strerror(errno), ifr->ifr_addr.sa_family);

    if (rc == -1) {
      continue;
    }
    
    if (ifr->ifr_addr.sa_family == AF_INET) {
      sin = (struct sockaddr_in*) &ifr->ifr_addr;

      pt = (char *) inet_ntop(AF_INET, &(sin->sin_addr), buffer, 256);
      if (pt == NULL) {
	Warning("failed to parse ip address in interface %s", ifr->ifr_name);
	continue;
      };
      DEBUG("ip addr = %s", buffer);

#ifdef SIOCGIFHWADDR
    } else {
      rc = ioctl(s, SIOCGIFHWADDR , &ifdat);
      DEBUG("ioctl (SIOCGIFHWADDR) returned %d errno %d hw_af = %d", 
	     rc, errno, ifdat.ifr_addr.sa_family );
      
      /* is this a portable way to check for ethernet */
      if ((ifdat.ifr_addr.sa_family == 1) || (ifdat.ifr_addr.sa_family == 2)){
	DEBUG(" MAC addr: %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
	       ifdat.ifr_addr.sa_data[0], 
	       ifdat.ifr_addr.sa_data[1], 
	       ifdat.ifr_addr.sa_data[2], 
	       ifdat.ifr_addr.sa_data[3], 
	       ifdat.ifr_addr.sa_data[4], 
	       ifdat.ifr_addr.sa_data[5]);

	memcpy (&hwaddr, &ifdat.ifr_addr, sizeof(struct sockaddr));
	hwaddrfound = 1;
      };
#endif
    }

    if (ntohl(sin->sin_addr.s_addr) == INADDR_LOOPBACK) {
       DEBUG(" this is the loopback device, ignoring");	
      continue;
    }

  /* now we actually have a legal ip address as a string in
     buffer,and it's not the loopback.*/
    
    sprintf(ipcmain->mw_instance_id, "%s/0x%x", buffer, masteripckey);
    DEBUG("instanceid = %s", ipcmain->mw_instance_id);
    goto out;
  };
  
  if (hwaddrfound) {
    Warning("using MAC address as instance net address");
    sprintf(ipcmain->mw_instance_id, "[%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx]/%d",
	    hwaddr.sa_data[0], 
	    hwaddr.sa_data[1], 
	    hwaddr.sa_data[2], 
	    hwaddr.sa_data[3], 
	    hwaddr.sa_data[4], 
	    hwaddr.sa_data[5], 
	    masteripckey);    
    goto out;
  };

 out:
  free(ifc.ifc_ifcu.ifcu_buf);
  close(s);
  return;
  

 nonet:
  Warning("No network connection found, assuming that host is standalone");
  sprintf(ipcmain->mw_instance_id, "localhost/%u", masteripckey);
  return;
}

/* after creation, fill in initial data in the tables. */
void
init_maininfo(void)
{
  unsigned short int semval = 1;

  strcpy(ipcmain->magic,_mwgetmagic());
  _mwgetversion(&ipcmain->vermajor,&ipcmain->vermajor,&ipcmain->vermajor);
  DEBUG("magic = %s version = %d.%d.%d", 
	 ipcmain->magic,ipcmain->vermajor,ipcmain->vermajor,ipcmain->vermajor);

  ipcmain->mwdpid = getpid();
  ipcmain->mwwdpid = -1;
  strncpy(ipcmain->mw_instance_name, instancename, MWMAXNAMELEN - 1);
  set_instanceid(ipcmain);
  ipcmain->status = MWBOOTING;
  ipcmain->clttbl_length  = maxclients;
  ipcmain->srvtbl_length  = maxservers;
  ipcmain->svctbl_length  = maxservices;
  ipcmain->gwtbl_length   = maxgateways;
  ipcmain->convtbl_length = maxconversations;

  ipcmain->mwd_mqid = msgget(masteripckey,IPC_CREAT|queuemode);
  DEBUG(" mwd mqid is %d", ipcmain->mwd_mqid);

  ipcmain->boottime = time(NULL);;
  ipcmain->lastactive = 0;
  ipcmain->configlastloaded = 0;
  ipcmain->shutdowntime = 0;

  /* alloc the lock for gateways. */
  ipcmain->gwtbl_lock_sem = semget(IPC_PRIVATE, 1, 0600);
  semctl(ipcmain->gwtbl_lock_sem, 1, SETALL, &semval);
};

/* mark the main segment that mwd has gone down. */
int term_maininfo(void)
{
  int mainid;

  if (ipcmain == NULL) return -ENOTCONN;
  ipcmain->status = MWDEAD;
  msgctl(ipcmain->mwd_mqid, IPC_RMID, NULL);
  ipcmain->mwd_mqid = -1;
  ipcmain->mwdpid = -1;
  semctl(ipcmain->gwtbl_lock_sem, 1, IPC_RMID);
  
  mainid = shmget(masteripckey, 0, 0);
  shmctl(mainid, IPC_RMID, NULL);
  shmdt(ipcmain);
  ipcmain = NULL;
  return 0;
};

/* cleanup_ipc is called if create_ipc has returned -EDEADLK, which indicate that 
   the ipcmain segemnt exists, but there are no process with the mwdpid in ipcmain.
   Or any other reason all traces of the  previous instance must be purged.

   We *could* try to just restart mwd and use tha same shm segments, but servers
   started by mwd are supposed to be restarted by mwd when mwd get SIGCHLD. If
   mwd is just restarted, this begcome a problem. How shall mwd know which the
   prev mwd started and who's standalone? (Maybe we should have a flag or PPID
   in the server table). It would make mwd crashes more gracefull, but this way
   is cleaner, and we're more sure of getting back to a known state.

   Procedure is as follows:
   1 - attach ipcmain and the tables
   2 - mark ipcmain for deletion.
   3 - call kill_all_members()
   4 - delete semaphores.
   5 - mark the rest of the shared memory segments for deletion.
   6 - detach them.  */

int cleanup_ipc(void)
{
  int rc; 
    
  int mainid;
  DEBUG("begin");
  if (ipcmain != NULL) {
    /* 1 attaching ipcmain */
    mainid = shmget(masteripckey, 0, 0);
    if (mainid == -1) {
      /* dissapeared in the mean while. */
      return 0;
    };
    ipcmain = (ipcmaininfo *) shmat(mainid, (void *) 0, 0);
    if (ipcmain == (void *) -1) {
      ipcmain = NULL;
      /* dissapeared in the mean while. */
      shmctl(mainid, IPC_RMID, NULL); /* just to be safe.*/
      return 0;
    };
  };
  if (strcmp(ipcmain->magic,_mwgetmagic()) != 0) {
    Warning(	  "Shared memory segment with key %#x is in use by another application", 
	  masteripckey);
    return -EBUSY;
  };
    
  Info("Beginning cleaning up after previous instance");
  /* 2, marking ipcmain as deleted */
  shmctl(mainid, IPC_RMID, NULL); /* this will allow another mwd to start!*/

  /* 1 attaching the rest of the tables. */
  clttbl = (cliententry *) shmat(ipcmain->clttbl_ipcid, NULL, 0);
  srvtbl = (serverentry *) shmat(ipcmain->srvtbl_ipcid, (void *) 0, 0);
  gwtbl = (gatewayentry *) shmat(ipcmain->gwtbl_ipcid, (void *) 0, 0);

  if (clttbl == (void *) -1) clttbl = NULL;
  if (srvtbl == (void *) -1) srvtbl = NULL;
  if (gwtbl == (void *) -1) gwtbl = NULL;

  /* 3 we have a proc for killing every one. */ 
  rc = kill_all_servers(SIGKILL);

   /* 4 and 5 deleting */
  shm_destroy();
  term_tables();
  term_maininfo();
  return 0;
};

PTask shutdowntask = 0;

int do_shutdowntrigger(PTask pt)
{
   int rc;
   rc = mwwaketask(shutdowntask);
   if (rc != 0) Fatal ("attempted to wake shutdown task %p but got error (%d) %s", 
		       (void *) shutdowntask, rc, strerror(-rc));

   return 0;
};

int do_shutdowntask(PTask pt)
{
   static int countdown = 5;
   int n, rc;

   if (countdown == 5) {
      Info ("beginning shutdown");
      mwsettaskinterval(pt, 1.0);
   };

   DEBUG ("shutdown task countdown %d", countdown);

   switch (countdown) {
      
   case 5:
      DEBUG("shutdown phase %d", countdown);
      countdown--;
      Info("MidWay daemon initiated shutdown ");
      ipcmain->shutdowntime = time(NULL);
      ipcmain->status = MWSHUTDOWN;

      n = stop_server(-1); /* give the command to every server */
      DEBUG("Signalled %d servers to stop", n);
      if (n != 0) break;
      
   case 4:  
      DEBUG("shutdown phase %d", countdown);
      rc = kill_all_servers(SIGTERM);
      DEBUG("kill_all_servers() returned %d", rc);
      countdown--;
      

   case 3:
      DEBUG("shutdown phase %d", countdown);
      rc = kill_all_servers(SIGINT);
      DEBUG("kill_all_servers() returned %d", rc);
      countdown--;

   case 2:
      DEBUG("shutdown phase %d", countdown);
      rc = kill_all_servers(SIGHUP);
      DEBUG("kill_all_servers() returned %d", rc);
      countdown--;

   case 1:
      DEBUG("shutdown phase %d", countdown);
      countdown--;
      Info("Watchdog: termination, kill all servers and connecting members.");
      kill(SIGTERM, ipcmain->mwwdpid);

      rc = kill_all_servers(SIGKILL);
      DEBUG("kill_all_servers() returned %d", rc);
      ipcmain->status = MWDEAD;
      hard_disconnect_ipc();
      break;
      
   case 0: 
      DEBUG("shutdown phase %d", countdown);
      flags.terminate = 1;      
   };
   
   countdown--;
   return 0;   
}; 



/*********************************************************************
 * the main program loop
 *********************************************************************/

static void mainloop(void)
{
  int rc, nonblock;
  int serviceid_srvmgr; 
  serviceentry * svcent;
  int provided = 0;
  PTask srvmgrtask, eventtask;
  double srvmgrtaskinterval = 5.0, eventtaskinterval = 5.0;
  double d;
  char * penv;

  /* simplified mwprovide() since we're not going to send a provide
     req to our self. */
  serviceid_srvmgr = addlocalservice(_mw_get_my_serverid(),MWSRVMGR, MWCALLSVC);
  if (serviceid_srvmgr < 0) abort();
  svcent = _mw_get_service_byid(serviceid_srvmgr);
  Info("providing service \"%s\" with serviceid = %x",  
	  MWSRVMGR, serviceid_srvmgr);
  svcent->svcfunc = 
    (struct ServiceFuncEntry  *) _mw_pushservice (serviceid_srvmgr, smgrCall);
  provided++;
  _mw_set_my_status("");

  if ((penv = getenv ("MWD_SRVMGR_TASK_INTERVAL"))) {
     d = atof(penv);
     if (d > 0.0) srvmgrtaskinterval = d;
  };

  if ((penv = getenv ("MWD_EVENT_TASK_INTERVAL"))) {
     d = atof(penv);
     if (d > 0.0) eventtaskinterval = d;
  };

  DEBUG("adding server manager task with interval %lg", srvmgrtaskinterval);
  srvmgrtask = mwaddtask(smgrTask, srvmgrtaskinterval);
  
  DEBUG("adding event task with interval %lg", eventtaskinterval);
  eventtask = mwaddtask(do_events, eventtaskinterval);

  shutdowntask = mwaddtask(do_shutdowntask, -1);

  mwwaketask(srvmgrtask);
  
  /* finally the loop */
  DEBUG("mainloop starts");
  while(1) {
    mwblocksigalarm();
    flags.alarm = 0;
    nonblock = mwdotasks();
    mwunblocksigalarm();
    DEBUG("mwdotask returned %d task ready to run", nonblock);


    // there is a potential race here if a timer signal should occur
    // after alarm counter is tested and the beginning of mgrcv() in
    // request_parse(). The race is avoided by having a 60 second
    // timeout fallback timer interval in _mw_serrealtimer().

    if ((!nonblock) && flags.alarm) nonblock = 1;
    DEBUG("mwdotask returned %s", nonblock?"nonblock":"block");
    
    rc = parse_request(nonblock);
    if (rc == -ESHUTDOWN) break;
    if (ipcmain->status == MWDEAD) exit(-1);

    if (flags.childdied > 0) {
      DEBUG("%d children has died furneral", flags.childdied);
      smgrDoWaitPid();
      flags.childdied = 0;
    };

    if (flags.terminate) {
       break;
    };

  };
  
  /* clean up provided services, completly redundant, but clean code
     is clean code! */
  _mw_popservice(serviceid_srvmgr);
  delservice(serviceid_srvmgr);

};

int daemonize(void)
{
  pid_t p;

  
  p = fork();

  if (p == -1) {
    Error("fork failed, reason %s", strerror(errno));
    return p;
  };

  if (p > 0) {
     return p;
  };
	
  fclose(stdin);
  fclose(stderr);
  fclose(stdout);

  stdout = fopen("/dev/null", "w");
  stderr = fopen("/dev/null", "w");
  
  setsid();

  return p;
};

/**********************************************************************
 * main
 *
 * this main has grow to be way to large 
 **********************************************************************/

int main(int argc, char ** argv)
{
  char c;
  int rc, loglevel, daemon = 1, n;
  char wd[PATH_MAX]; 
  char * name;
  mwaddress_t * mwaddress;

  int opt_clients =       -1;
  int opt_servers =       -1;
  int opt_services =      -1;
  int opt_gateways =      -1;
  int opt_conversations = -1;
  int opt_bufferbasesize =   -1;
  char * penv;
  mode_t m;
  
  /* { */
  /*   _mw_copy_on_stdout(TRUE); */
  /*   loglevel = _mwstr2loglevel("debug3");  */

  /*   mwopenlog("x", ".", loglevel); */
  /*   _mw_copy_on_stdout(TRUE); */

  /*   struct ipcmaininfo ipcmain = {0}; */
  /*   set_instanceid(&ipcmain); */
  /*   exit(0); */
  /* } */
  m = umask(0);
  umask(m);
  mwdSetIPCparam(UMASK, m);


  /* obtaining info about who I am, must be fixed for suid, check masterd*/
  mepw = getpwuid(getuid());

  loglevel = MWLOG_INFO;
  penv = getenv ("MWD_LOGLEVEL");
  if (penv != NULL) {
     rc = _mwstr2loglevel(penv);
     if (rc != -1) {
	loglevel = rc;
	mwsetloglevel(loglevel);
     };
  };

  if (mepw == NULL) {
    Fatal("Unable to get my own passwd entry, aborting");
    exit(1);
  };

  Info("MidWay daemon starting with userid %s (uid=%d) proccessid=%d", 
	mepw->pw_name, getuid(), getpid());
  Info("Version $Name$");
  Info("Version %s", mwversion());

  /* doing options */
  while((c = getopt(argc,argv, "A:fD:H:l:c:C:s:S:b:B:g:")) != EOF ){
    switch (c) {
    case 'l':
      loglevel = _mwstr2loglevel(optarg); 
      mwsetloglevel(loglevel);
      break;
    case 'A':
      uri = strdup(optarg);
      break;
    case 'f':
      daemon = 0;
      break;
    case 'H':
      mwhome = strdup(optarg);
      break;
    case 'D':
      mwdatadir = strdup(optarg);
      break;
    case 'c':
      opt_clients = atoi(optarg);
      break;
    case 'C':
      opt_conversations = atoi(optarg);
      break;
    case 's':
      opt_servers = atoi(optarg);
      break;
    case 'S':
      opt_services = atoi(optarg);
      break;
    case 'b':
      opt_bufferbasesize = atoi(optarg);
      break;
    case 'B':
      break;
    case 'g':
      opt_gateways = atoi(optarg);
      break;
    default:
      usage();
      break;
    }
  }

  _mw_copy_on_stdout(TRUE);
#ifdef DEBUGGING
  mwsetloglevel(MWLOG_DEBUG);
#endif 

  /* Now brfore we can create the IPC tables, we must have the 
     configuration file, MWHOME, and ipckey for the instance. 

     It's located in $(MWHOME)/instancename/config.xml

     The instance is either given as an arg, or defaults to userid.

     MWHOME is set by the -H option. If not, we check ~/.midwaytab, if
     no there we check the environment, if not set there either we
     default to ~/MidWay/

     The ipckey is set by the -A option, if not, we check
     ~/.midwaytab, if not there we check the config, if not set there
     we default to uid.
  */

  
  DEBUG("optind = %d", optind);
  /* the only argument allowed is the instance name (or a config). */
  if (optind +1  == argc) {
    instancename = argv[optind];
  } else if (optind == argc) {
    instancename = strdup(mepw->pw_name);
  } else {
    usage();
  };
  uri = alloca(strlen(instancename) + 8);
  sprintf(uri, "ipc:/%s", instancename);
  
  Info("MidWay instance name is %s url=%s", instancename, uri); 
  mwhome = _mw_makeMidWayHomePath(mwhome) ;
  Info("MidWay home is %s", mwhome); 
  char * instancehomepath = _mw_makeInstanceHomePath(mwhome, instancename);
  masteripckey = _mw_make_instance_home_and_ipckey(instancehomepath);
  chdir (instancehomepath);
  /* init smgr, it sets default path which must be set before all
     setenvs in config but after mwhome is set. */
  smgrInit();
       
  /* now if the config exists load it */
  getcwd(wd,PATH_MAX);
  strcat(wd,"/config");

  Info("loading config %s", wd);
  rc = xmlConfigLoadFile(wd);
  if (rc == 0) {
    Info("config loaded");    
    rc = xmlConfigParseTree();
    Info("config parses with rc = %d errno %d", rc, errno);
    smgrDumpTree();
  } else {
    if (errno == ENOENT) 
      Info("no config using defaults");
    else {
      Error("while loading config reason %s", strerror(errno));
      exit(-1);
    };
  };


  /**********************************************************************
   *
   * load of config complete, now w do the rest of the setup.
   *
   **********************************************************************/

  /* TODO: the log dir may be set in the config */
  rc = _mw_mkdir_asneeded("log");
  if (rc != 0) {
    Error("Failed to create the instance directory MWHOME/%s reason %s",
	  "log", strerror(errno));
    exit(-1);
  };

  getcwd(wd,PATH_MAX);
  strcat(wd, "/");
  strcat(wd, "log/");

  strcat(wd,"SYSTEM");

  name = strrchr(argv[0], '/');
  if (name == NULL) name = argv[0];
  else name++;

  Info("Logging to %s.YYYYMMDD", wd);
  mwopenlog(name, wd, loglevel);
  
  Info("Logging to %s.YYYYMMDD", wd);

  /* more subdirs to follow. */
  _mw_mkdir_asneeded("bin");
  _mw_mkdir_asneeded("lib");
  _mw_mkdir_asneeded("run");


  inst_sighandlers();

  /********************************************************************************
   *
   * Now after given options we calculate the size of the shared memory segments.
   * We have separate segments for the tables and for the shm buffers.
   * The max* options are used to control these sizes. 
   * In the future we will get defaults from config file.
   * see ipctables.h and shmalloc.h 
   *
   *******************************************************************************/

  /* first, do sanity checks on optional parameters. */
  if (opt_clients == -1)       mwdSetIPCparam(MAXCLIENTS,  opt_clients);
  if (opt_servers == -1)       mwdSetIPCparam(MAXSERVERS,  opt_servers);
  if (opt_services == -1)      mwdSetIPCparam(MAXSERVICES, opt_services);
  if (opt_gateways == -1)      mwdSetIPCparam(MAXGATEWAYS, opt_gateways);
  if (opt_conversations == -1) mwdSetIPCparam(MAXCONVS,    opt_conversations);
    

  /* figring out ipckey , it may already be set by -A or config, we
     overturn the config but honor -A */

  /* if -A was set uri is not NULL, else it came form config, and
     we're going to create it now. */
  if (uri == NULL) {
    uri = malloc(16);
    key_t tabkey = 0;
    /* if tabkey != -1 the ipckey was in ~/.midwaytab */
    if (tabkey > 0) {
      sprintf(uri, "ipc:%d", tabkey);
    } else {
      /* if in config it's alreay set */
      if (mwdGetIPCparam(MASTERIPCKEY) > 0) {
	sprintf(uri, "ipc:%u", mwdGetIPCparam(MASTERIPCKEY));
      } else {
	/* else default to uid */
	sprintf(uri, "ipc:%u", getuid());
      };
    }
  };
  
  Info("MidWay instance URI is %s", uri); 
  errno = 0;

  mwaddress = _mw_get_mwaddress();
  rc = _mwdecode_url(uri, mwaddress);
  
  if (rc != 0) {
    Error("Unable to parse URI %s, expected ipc:12345 " 
	  "where 12345 is a unique IPC key", uri);
    exit(-1);
  };

  if (mwaddress->protocol != MWSYSVIPC) {
    Error("url protocol must be ipc for mwd, url=%s %s",
	  uri, _mw_errno2str());
    exit(-1);
  };
  
  mwdSetIPCparam(MASTERIPCKEY, mwaddress->sysvipckey);

  /* We have a protection mode = 0 here. We should use umask.
   * but then again we should alse allow this to be set by cmdline 
   */
  rc = create_ipc(0);
  n = 0;
  while (rc < 0) {
    if (n > 5) {
      Error("This can't happen clean up of old ipc fails in %s line %d", 
	    __FILE__, __LINE__);
      exit (-6);
    };
    switch (rc) {
    case -EDEADLK:
      rc = cleanup_ipc();
      if (rc == -EBUSY) {
	exit(-1);
      };
      break;;
    case -EEXIST:
      exit(0);
    default:
      exit(9);
    };
    n++;
    rc = create_ipc(0);
  };
  DEBUG("Shm data segments created.");

  if (daemon) {
    rc = daemonize();
    if (rc == -1) {
      Error("daemonize failed reason: %s", strerror(errno));
      shm_destroy();
      term_tables();
      term_maininfo();
      exit (-1);
    };
    /* parent just die */
    if (rc > 0) {
      exit(0);
    }
  };
  DEBUG("formating shm tables");
  init_maininfo();
  init_tables();

  /* place MWHOME in ipcmain */
  strncpy(ipcmain->mw_homedir, mwhome, 256);
  
  if (mwdatadir == NULL) {
     strncpy(ipcmain->mw_bufferdir, mwhome, 256);
     strcat(ipcmain->mw_bufferdir, "/");
     strcat(ipcmain->mw_bufferdir, instancename);
     strcat(ipcmain->mw_bufferdir, "/data");
  } else {
     strncpy(ipcmain->mw_bufferdir, mwdatadir, 256);
  };
  rc = _mw_mkdir_asneeded(ipcmain->mw_bufferdir);
  if (rc != 0) {
     Error("Failed to create the directory %s reason %s",
	   ipcmain->mw_bufferdir, strerror(errno));
     exit(-1);
  };
  
  /* module ipctables.c has a static defined ipcmain, we must set it. */
  _mw_set_shmadr (ipcmain, clttbl, srvtbl, svctbl, gwtbl, convtbl);

  /* we add us selfs as a server since we're going to provide .mwsrvmgr etc */

  _mw_set_my_serverid(addserver("mwd",ipcmain->mwd_mqid, getpid()));

  /*
   * main loop of mwd, apart from occational timers, alle we do is
   * to waut for administrative request in message queue.
   *
   * Note that child processes will be TCP client servers and Gateway servers
   * they will have a diffrent main loop 
   */
  DEBUG("starting watchdog");
  rc = start_watchdog();
  if (rc <= 0) {
    Error("MidWay WatchDog daemon failed to start. reason %d", rc);
    shm_destroy();
    term_tables();
    term_maininfo();
    exit(-1);
  };
  Info("MidWay WatchDog daemon started pid=%d", rc); 
  Info("MidWay daemon boot complete instancename is %s", 
	ipcmain->mw_instance_name); 

  ipcmain->status = MWREADY;
  ipcmain->boottime = (int64_t) time(NULL);

  /* now start all autobott servers */
  smgrAutoBoot();

  mainloop();

  cleanup_ipc();

  Info("MidWay daemon shutdown complete");
};

/* Emacs C indention
Local variables:
c-basic-offset: 2
End:
*/
