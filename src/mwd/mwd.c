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

/*
 * $Id$
 * $Name$
 * 
 * $Log$
 * Revision 1.9  2002/08/09 20:50:16  eggestad
 * A Major update for implemetation of events and Task API
 *
 * Revision 1.8  2002/07/07 22:45:48  eggestad
 * *** empty log message ***
 *
 * Revision 1.7  2002/02/17 14:43:26  eggestad
 * - default ipc runtime params are now static
 * - added mwdSetIPCparam()/mwdGetIPCparam()
 * - all accesses of IPC params now thru API
 * - added missing includes
 * - IPC parameter fixup, sanity check
 * - added ~/.midwaytab and function checktab() to handle it.
 * - mkdir_asneeded() to help creating missing dirs during boot
 * - moved mainloop() from main to its own function mainloop()
 * - we now implicit add mwd to the server table for implicit services
 *
 * Revision 1.6  2001/10/03 22:40:39  eggestad
 * cpp debugging setting
 *
 * Revision 1.5  2001/09/15 23:44:13  eggestad
 * fix for changing ipcmain systemname to instance name
 * added func for instancename generation
 *
 * Revision 1.4  2000/11/15 21:20:42  eggestad
 * Failed to boot with -D, wrong pid in shm
 *
 * Revision 1.3  2000/09/21 18:54:05  eggestad
 * Bug fixes around the URL, and added debug3 and 4 on -l option
 *
 * Revision 1.2  2000/07/20 19:47:39  eggestad
 * - A semaphore for mwgwd is added. (mwgwd changes IPC tables directly,
 *   but they may be more than one.
 * - fix for home dir for instance.
 * - prototype fixup.
 *
 * Revision 1.1.1.1  2000/03/21 21:04:24  eggestad
 * Initial Release
 *
 * Revision 1.1.1.1  2000/01/16 23:20:12  terje
 * MidWay
 *
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

static char * RCSId UNUSED = "$Id$";


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
char * mwhome = NULL;
char * instancename = NULL;

struct passwd * mepw = NULL;

Flags flags = { 0, 0, 0, 0, 0};

void usage(void)
{
  fprintf (stderr,"\nmwd [options...] [instancename]\n\n");
  fprintf (stderr,"  where options is one of the following:\n");
  fprintf (stderr,"    -l loglevel : (error|warning|info|debug|debug1|debug2|debug3|debug4)\n");  
  fprintf (stderr,"    -A uri : uri is the address of the MidWay instance e.g. ipc://12345\n");  
  fprintf (stderr,"    -D : Start mwd as a daemon.\n");  
  fprintf (stderr,"    -H MidWayHome : Defaults to ~/MidWay.\n");  
  fprintf (stderr,"    -c maxclients\n");  
  fprintf (stderr,"    -s maxservers\n");  
  fprintf (stderr,"    -S maxservices\n");  
  fprintf (stderr,"    -g maxgateways\n");  
  fprintf (stderr,"    -B heapsize\n");  
  fprintf (stderr,"    -C maxconversations\n");  
  fprintf (stderr,"\n");
  fprintf (stderr,"  Instancename is the name of the instance. It looks for config etc\n");
  fprintf (stderr,"    under MidWayHome/instancename/ defaults to userid.\n");
  fprintf (stderr,"    MidWayHome defaults ~/MidWay unless overridden by -D\n");
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

  default:
    return -1;
  };
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
  default:
    return -1;
  };
  return value;
};

ipcmaininfo * ipcmain = NULL;

ipcmaininfo * getipcmaintable()
{
  DEBUG("lookup of ipcmain address: 0x%x", ipcmain);
  return ipcmain;
};
cliententry * getcliententry(int i)
{
  return & clttbl[i];
};
serverentry * getserverentry(int i)
{
  return & srvtbl[i];
};
serviceentry * getserviceentry(int i)
{
  return & svctbl[i];
};
gatewayentry * getgatewayentry(int i)
{
  return & gwtbl[i];
};
conv_entry * getconv_entry(int i)
{
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

static int tablemode = 0644, /* shm tables mode is 0666 & mask */
  heapmode           = 0666, /* shm heap and semaphore mode is 0666 & mask */
  queuemode          = 0622; /* mwd message queue mode is 0622 & mask */

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
  
  if (mode == 0) {
    um = umask(0);
    umask(um);

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
  DEBUG("main shm info table attached at 0x%x",ipcmain);

  /* 
   * attaching all the other tables 
   */

  /*** CLIENT TABLE ***/
  ipcmain->clttbl_ipcid = shmget(IPC_PRIVATE, sizeof(struct cliententry) * maxclients,tablemode);
  if (ipcmain->clttbl_ipcid == -1) {
    Error("Failed to attach client table with id = %d reason %s",
	  clttbl, strerror(errno));
    return -errno;
  };
  clttbl = (cliententry *) shmat(ipcmain->clttbl_ipcid, NULL, 0);
  DEBUG("client table id=%d attached at 0x%x", 
	ipcmain->clttbl_ipcid,clttbl);
  
  /*** SERVER TABLE ***/
  ipcmain->srvtbl_ipcid = shmget(IPC_PRIVATE, sizeof(struct serverentry) * maxservers,
				 tablemode);
  if (ipcmain->srvtbl_ipcid == -1) {
    Error("Failed to attach Server table with id = %d reason %s",
	  srvtbl, strerror(errno));
    return -errno;
  };
  srvtbl = (serverentry *) shmat(ipcmain->srvtbl_ipcid, (void *) 0, 0);
  DEBUG("Server table id=%d attached at 0x%x",
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
  DEBUG("service table id=%d attached at 0x%x",
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
  DEBUG("gateway table id=%d attached at 0x%x",
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
  DEBUG("convserver table attached at 0x%x",convtbl);

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

void set_instanceid(ipcmaininfo * ipcmain)
{
  char * pt, buffer [256] = {0};
  int s, rc, idx, n;
  struct ifreq ifdat = {0};
  struct sockaddr_in * sin;
  struct sockaddr hwaddr = { 0 };
  int hwaddrfound = 0;
  
  s = socket (AF_INET, SOCK_DGRAM, 0);

  if (s == -1) goto nonet;

  for (n = 0, idx=1; n == 0; idx++) {
    
    DEBUG("checking interface with index %d for unique address", idx);

    ifdat.ifr_ifindex = idx;
    n = ioctl(s, SIOCGIFNAME, &ifdat);
    if (n == -1) break;

    DEBUG("ioctl(SIOCGIFNAME) returned %d errno %d ifname = %s", 
	  n, errno, ifdat.ifr_name);
    
    rc = ioctl(s, SIOCGIFADDR, &ifdat);
    DEBUG("ioctl(SIOCGIFADDR) returned %d errno %d af = %d", 
	  rc, errno, ifdat.ifr_addr.sa_family);
    
    if (ifdat.ifr_addr.sa_family == AF_INET) {
      sin = (struct sockaddr_in*) &ifdat.ifr_addr;

      pt = (char *) inet_ntop(AF_INET, &(sin->sin_addr), buffer, 256);
      if (pt == NULL) {
	Warning("failed to parse ip address in interface %s", ifdat.ifr_name);
	continue;
      };
      DEBUG("ip addr = %s", buffer);
      
    } else {
      rc = ioctl(s, SIOCGIFHWADDR , &ifdat);
      DEBUG("ioctl returned %d errno %d hw_af = %d", 
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
    }

    if (ntohl(sin->sin_addr.s_addr) == INADDR_LOOPBACK) {
       DEBUG(" this is the loopback device, ignoring");	
      continue;
    }

  /* now we actually have a legal ip address as a string in
     buffer,and it's not the loopback.*/
    
    sprintf(ipcmain->mw_instance_id, "%s/%d", buffer, masteripckey);
    DEBUG("instanceid = %s", ipcmain->mw_instance_id);
    return;
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
    return;
  };

 nonet:
  Warning("No network connection found, assuming that host is standalone");
  sprintf(ipcmain->mw_instance_id, "localhost/%d", masteripckey);
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
  rc = kill_all_servers();

   /* 4 and 5 deleting */
  shm_destroy();
  term_tables();
  term_maininfo();
};

/* we're now going to make an instance name of netaddr/ipckey we go
   thru the network devices and look for our primary ip address.  if
   that fail, we use teh MAC addr, if the fail, we don't have a
   network connected,and we use the hostname. */
  
/****************************************************************************************
 * Signal handling
 ****************************************************************************************/

void sighandler(int sig)
{
  switch(sig) {
    
  case SIGALRM:
    flags.alarm++;
    return;

  case SIGTERM:
  case SIGINT:
    flags.terminate = sig;
    return;
  case SIGHUP:
  case SIGQUIT:
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

void inst_sighandlers()
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

/* lets go thru the ~/.midwaytab and see if there is a home or ipc set. */
static void checktab(char * instancename, char ** home, int * key)
{
  char tabpath[PATH_MAX];
  char line[LINE_MAX];
  char arg[5][65];
  FILE * fp;
  int n, i;

  /* can't happen, mepw is fetche at the very beginning of main */
  if (mepw == NULL) return; 
  
  strcpy(tabpath, mepw->pw_dir);
  strcat(tabpath, "/.midwaytab");

  fp = fopen(tabpath, "r");
  if (fp == NULL) goto errout;

  while ( !feof(fp)) {

    if (fgets(line, LINE_MAX, fp) == NULL) continue;

    arg[0][0] = arg[1][0] = arg[2][0] = arg[3][0] = arg[4][0] = '\0';
    n = sscanf(line, "%64s %64s %64s %64s %64s", 
	       &arg[0], &arg[1], &arg[2], &arg[3], &arg[4]);

    DEBUG2("read %d items from %s", n, tabpath);
    if (n == 0) continue;
     
    DEBUG2("read %d items \"%s\" \"%s\" \"%s\" \"%s\" \"%s\"", 
	  n, arg[0], arg[1], arg[2], arg[3], arg[4]);
    
    if (strcmp(arg[0], instancename) != 0) continue;
    
    for (i = 0; i < 5; i++) {
      if ( (home != NULL) && (strncmp("home=", arg[i], 5) == 0) ) {
	DEBUG2("home = %s", &arg[i][5] );
	*home = strdup(&arg[i][5]);
      };
      if ( (key != NULL) && (strncmp("ipc:", arg[i], 4) == 0) ) {
	DEBUG2("ipckey = %s", &arg[i][4] );
	*key = (atoi(&arg[i][4]));
      };
    };
  };

  fclose(fp);
  return;

 errout:
      
  DEBUG2("checktab fails");
  if  (home != NULL) *home = NULL;
  if  (key != NULL) *key = -1;
  if (fp != NULL) fclose (fp);
  return;
};

static int mkdir_asneeded(char * path)
{
  struct stat st;
  char * tmp; 
  int rc;

  errno = 0;

  DEBUG2("attempting to mkdir(%s)", path);
  rc = stat(path, &st);

  if (rc == 0) {
    if (S_ISDIR(st.st_mode)) {
      DEBUG2("%s is a dir", path);
      return 0;
    } else {
      Error("%s is not a dir", path);
      errno = ENOTDIR;
      return -1;
    };
  };
  
  /* stat failed := path don't exists, recursivly try the .. dir */
  tmp = strrchr(path, '/');
  if (tmp == NULL) {
    DEBUG2("unable to find a .. dir in %s, this is the top", path);
  } else {    
    *tmp = '\0';
    
    if (strlen(path) == 0) {
      errno = ENOTDIR;
      return -1;
    };
    
    rc = mkdir_asneeded(path);
    if (rc == -1) return -1;
  
    *tmp = '/';
  };

  Info("creating directory %s", path);
  rc = mkdir(path, 0777);
  
  return rc;
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

  srvmgrtask = mwaddtask(smgrTask, 5000);
  eventtask = mwaddtask(do_events, 5000);

  mwwaketask(srvmgrtask);
  
  /* finally the loop */
  DEBUG("mainloop starts");
  while(1) {
    nonblock = mwdotasks();
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
      Info("MidWay daemon initiated shutdown on signal %d", 
	    flags.terminate);
      ipcmain->shutdowntime = time(NULL);
      kill(SIGTERM, ipcmain->mwwdpid);
    };
  };
  
  /* clean up provided services, completly redundant, but clean code
     is clean code! */
  _mw_popservice(serviceid_srvmgr);
  delservice(serviceid_srvmgr, _mw_get_my_serverid());

};


/**********************************************************************
 * main
 *
 * this main has grow to be way to large 
 **********************************************************************/

int main(int argc, char ** argv)
{
  char c;
  int rc, loglevel, daemon = 0, n;
  char wd[PATH_MAX]; 
  char * name, * tabhome = NULL;
  int tabkey = -1;
  mwaddress_t * mwaddress;

  int opt_clients =       -1;
  int opt_servers =       -1;
  int opt_services =      -1;
  int opt_gateways =      -1;
  int opt_conversations = -1;
  int opt_numbuffers =       -1;
  int opt_bufferbasesize =   -1;

  /* obtaining info about who I am, must be fixed for suid, check masterd*/
  mepw = getpwuid(getuid());

  if (mepw == NULL) {
    Fatal("Unable to get my own passwd entry, aborting");
    exit(1);
  };

  Info("MidWay daemon starting with userid %s (uid=%d) proccessid=%d", 
	mepw->pw_name, getuid(), getpid());
  Info("Version $Name$");
  Info("Version %s", mwversion());

  /* doing options */
  while((c = getopt(argc,argv, "A:DH:l:c:C:s:S:b:B:g:")) != EOF ){
    switch (c) {
    case 'l':
      if      (strcmp(optarg, "error")   == 0) loglevel=MWLOG_ERROR;
      else if (strcmp(optarg, "warning") == 0) loglevel=MWLOG_WARNING;
      else if (strcmp(optarg, "info")    == 0) loglevel=MWLOG_INFO;
      else if (strcmp(optarg, "debug")   == 0) loglevel=MWLOG_DEBUG;
      else if (strcmp(optarg, "debug1")  == 0) loglevel=MWLOG_DEBUG1;
      else if (strcmp(optarg, "debug2")  == 0) loglevel=MWLOG_DEBUG2;
      else if (strcmp(optarg, "debug3")  == 0) loglevel=MWLOG_DEBUG3;
      else if (strcmp(optarg, "debug4")  == 0) loglevel=MWLOG_DEBUG4;
      else usage();
      mwsetloglevel(loglevel);
      break;
    case 'A':
      uri = strdup(optarg);
      break;
    case 'D':
      daemon = 1;
      break;
    case 'H':
      mwhome = strdup(optarg);
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
  
  Info("MidWay instance name is %s", instancename); 

  /* lets check the ~/.midwaytab and see if there is a home or ipc set. */
  checktab(instancename, &tabhome, &tabkey);

  /* now we're going to set MWHOME, instancename and try to load the config */
  
  /* Figure out the instance home. If not given by -H or thru env var MWHOME,
     Default to ~/MidWay */

  if (mwhome == NULL) {
    if (tabhome != NULL) 
      mwhome = tabhome;
    else 
      mwhome = getenv("MWHOME");
  };

  /* if still NULL default */
  if (mwhome == NULL) {
    mwhome = malloc(strlen(mepw->pw_dir)+10);
    sprintf(mwhome,"%s/%s", mepw->pw_dir, "MidWay");
  };

  /* We apply PATH_MAX == 255 here, just to make life easy. */
  if (strlen(mwhome) > 250) {
    Error("Path to MidWayHome is to long. %d is longer than max 250", strlen(mwhome));
    exit(-1);
  };

  /* now do chdir to mwhome, if fail, go thru and create directories as needed.
     If we fail to create them, abort. */
  Info("MWHOME = %s", mwhome);

  rc = mkdir_asneeded(mwhome);
  if (rc != 0) {
    Error("Failed to create the directory %s reason %s",
	  mwhome, strerror(errno));
    exit(-1);
  };

  rc = chdir(mwhome);
  if (rc != 0) {
    Error("failed to chdir to MWHOME=%s reason %s", 
	  mwhome, strerror(errno));
    exit(-1);
  };
  
  rc = mkdir_asneeded(instancename);
  if (rc != 0) {
    Error("Failed to create the instance directory MWHOME/%s reason %s",
	  instancename, strerror(errno));
    exit(-1);
  };

  rc = chdir(instancename);
  if (rc != 0) {
    Error("failed to chdir to MWHOME/%s reason %s", 
	  instancename, strerror(errno));
    exit(-1);
  };

  /* when we get here, CWD is mwhome/instancename and it exists.*/


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
  rc = mkdir_asneeded("log");
  if (rc != 0) {
    Error("Failed to create the instance directory MWHOME/%s reason %s",
	  "log", strerror(errno));
    exit(-1);
  };

  strcpy(wd, mwhome);
  strcat(wd, "/");
  strcat(wd, instancename);
  strcat(wd, "/");
  strcat(wd, "log/");

  strcat(wd,"SYSTEM");

  name = strrchr(argv[0], '/');
  if (name == NULL) name = argv[0];
  else name++;
  mwopenlog(name, wd, loglevel);
  
  Info("Logging to %s.YYYYMMDD", wd);

  /* more subdirs to follow. */
  mkdir_asneeded("bin");
  mkdir_asneeded("lib");
  mkdir_asneeded("run");


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

    /* if tabkey != -1 the ipckey was in ~/.midwaytab */
    if (tabkey > 0) {
      sprintf(uri, "ipc:%d", tabkey);
    } else {
      /* if in config it's alreay set */
      if (mwdGetIPCparam(MASTERIPCKEY) > 0) {
	sprintf(uri, "ipc:%d", mwdGetIPCparam(MASTERIPCKEY));
      } else {
	/* else default to uid */
	sprintf(uri, "ipc:%d", getuid());
      };
    }
  };
  
  Info("MidWay instance URI is %s", uri); 
  errno = 0;
  mwaddress = _mwdecode_url(uri);
  
  if (mwaddress == NULL) {
    Error("Unable to parse URI %s, expected ipc:12345 " 
	  "where 12345 is a unique IPC key", uri);
    exit(-1);
  };

  if ( (mwaddress != NULL) && (mwaddress->protocol != MWSYSVIPC) ) {
    Error("url prefix must be ipc for mwd, url=%s errno=%d", uri, errno);
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
    rc = fork();
    if (rc == -1) {
      Error("fork failed reason: %s", strerror(errno));
      shm_destroy();
      term_tables();
      term_maininfo();
      exit (-1);
    };
    /* parent just die */
    if (rc > 0) {
      usleep(100000);
      exit(0);
    }
  };

  init_maininfo();
  init_tables();

  /* place MWHOME in ipcmain */
  strncpy(ipcmain->mw_homedir, mwhome, 256);

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

  /* we copy all log messages to stdout iff in debugmode. */
  if (loglevel < MWLOG_DEBUG) {
    _mw_copy_on_stdout(FALSE);
    fclose (stdin);
    fclose (stdout);
    fclose (stderr);
  };

  ipcmain->status = MWREADY;
  time(&ipcmain->boottime);

  /* now start all autobott servers */
  smgrAutoBoot();

  mainloop();

  cleanup_ipc();

  Info("MidWay daemon shutdown complete", rc);
};
