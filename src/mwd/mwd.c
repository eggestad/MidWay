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
 * Revision 1.1  2000/03/21 21:04:24  eggestad
 * Initial revision
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
#include <string.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>
/* NB: see below */
#include <signal.h>

#include <MidWay.h>
#include <ipcmessages.h>
#include <ipctables.h>
#include <address.h>
#include <version.h>
#include "mwd.h"
#include "tables.h"

int maxclients =        100;
int maxservers =         50;
int maxservices =       100;
int maxgateways =        20;
int maxconversations =   20;
int numbuffers =         -1;
int bufferbasesize =   1024;
key_t masteripckey =     -1;

char * uri = NULL;
char * mwhome = NULL;
char * instancename = NULL;

struct passwd * mepw = NULL;

Flags flags = { 0, 0, 0, 0, 0};

void usage()
{
  fprintf (stderr,"\nmwd [options...] [instancename]\n\n");
  fprintf (stderr,"  where options is one of the following:\n");
  fprintf (stderr,"    -l loglevel : (error|warning|info|debug|debug1|debug2)\n");  
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
ipcmaininfo  * ipcmain = NULL;

ipcmaininfo * getipcmaintable()
{
  mwlog(MWLOG_DEBUG, "lookup of ipcmain address: 0x%x", ipcmain);
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

int mymqid()
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
  int a;
  void * adr;
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
    mwlog(MWLOG_ERROR,"This can't happen, create_ipc was called with mode = 0%op",mode);
    return -EINVAL;
  };

  /* we apply the mask */
  tablemode = tablemode & mask;
  heapmode  = heapmode  & mask;
  queuemode = queuemode & mask;
  printf("tablemode = %o heapmode = %o queuemode = %o mask = %o\n",tablemode, heapmode,queuemode,mask); 

  mwlog(MWLOG_DEBUG,"shared memory for main info has ipckey = 0x%x mode=0%o",
	masteripckey, tablemode|IPC_CREAT | IPC_EXCL);

  /* main table are always only writeable by owner of mwd. */
  mainid = shmget(masteripckey, sizeof(struct ipcmaininfo), tablemode |IPC_CREAT | IPC_EXCL);
  if (mainid == -1) {
    if (errno == EEXIST) {
      /* if we got EEXIST, this really can't fail */
      mainid = shmget(masteripckey, 0, 0);
      ipcmain = (ipcmaininfo *) shmat(mainid, (void *) 0, 0);
      if ((ipcmain == NULL) || (ipcmain == (void *) -1)) {
	mwlog(MWLOG_ERROR,"A shared memory segment with key %#x exists, id=%d, but failet to attach it!",masteripckey, mainid);
	return -EEXIST;
      };
      pid = ipcmain->mwdpid;
      if ((pid > 1) && (kill(pid, 0) == 0)) {
	mwlog(MWLOG_ERROR,"The instance is already running, mwd has pid %d",pid);
	shmdt(ipcmain);
	return -EEXIST;
      }
      /* the previous died without cleaning up. */
      return -EDEADLK;
    }
    /* unknown error */
    mwlog(MWLOG_ERROR,"Failed to create shared memory for main info for ipckey = 0x%x reason: %s: flags 0%o",
	  masteripckey, strerror(errno),mode );
    return -errno;
  };
  mwlog(MWLOG_DEBUG,"main shm info table has id %d",mainid);

  ipcmain = (ipcmaininfo *) shmat(mainid, (void *) 0, 0);
  if (ipcmain == (void *) -1) {
    ipcmain = NULL;
    mwlog(MWLOG_ERROR,"Failed to attach shared memory with id = %d reason %s",
	  mainid, strerror(errno));
    return -errno;
  };
  mwlog(MWLOG_DEBUG,"main shm info table attached at 0x%x",ipcmain);

  /* 
   * attaching all the other tables 
   */

  /*** CLIENT TABLE ***/
  ipcmain->clttbl_ipcid = shmget(IPC_PRIVATE, sizeof(struct cliententry) * maxclients,tablemode);
  if (ipcmain->clttbl_ipcid == -1) {
    mwlog(MWLOG_ERROR,"Failed to attach client table with id = %d reason %s",
	  clttbl, strerror(errno));
    return -errno;
  };
  clttbl = (cliententry *) shmat(ipcmain->clttbl_ipcid, NULL, 0);
  mwlog(MWLOG_DEBUG,"client table id=%d attached at 0x%x", 
	ipcmain->clttbl_ipcid,clttbl);
  
  /*** SERVER TABLE ***/
  ipcmain->srvtbl_ipcid = shmget(IPC_PRIVATE, sizeof(struct serverentry) * maxservers,
				 tablemode);
  if (ipcmain->srvtbl_ipcid == -1) {
    mwlog(MWLOG_ERROR,"Failed to attach Server table with id = %d reason %s",
	  srvtbl, strerror(errno));
    return -errno;
  };
  srvtbl = (serverentry *) shmat(ipcmain->srvtbl_ipcid, (void *) 0, 0);
  mwlog(MWLOG_DEBUG,"Server table id=%d attached at 0x%x",
	ipcmain->srvtbl_ipcid ,srvtbl);

  /*** SERVICE TABLE ***/
  ipcmain->svctbl_ipcid = shmget(IPC_PRIVATE, sizeof(struct serviceentry) * maxservices,
				 tablemode);
  if (ipcmain->svctbl_ipcid == -1) {
    mwlog(MWLOG_ERROR,"Failed to attach service table with id = %d reason %s",
	  ipcmain->svctbl_ipcid, strerror(errno));
    return -errno;
  };
  svctbl = (serviceentry *) shmat(ipcmain->svctbl_ipcid, (void *) 0, 0);
  mwlog(MWLOG_DEBUG,"service table id=%d attached at 0x%x",
	ipcmain->svctbl_ipcid, svctbl);

  /*** GATEWAYS TABLE ***/
  ipcmain->gwtbl_ipcid = shmget(IPC_PRIVATE, sizeof(struct gatewayentry) * maxgateways,
				tablemode);
  if (ipcmain->gwtbl_ipcid == -1) {
    mwlog(MWLOG_ERROR,"Failed to attach gateway table with id = %d reason %s",
	  ipcmain->gwtbl_ipcid, strerror(errno));
    return -errno;
  };
  gwtbl = (gatewayentry *) shmat(ipcmain->gwtbl_ipcid, (void *) 0, 0);
  mwlog(MWLOG_DEBUG,"gateway table id=%d attached at 0x%x",
	ipcmain->gwtbl_ipcid, gwtbl);

  /*** CONVERSATIONS TABLE ***/
  ipcmain->convtbl_ipcid = shmget(IPC_PRIVATE, sizeof(struct conv_entry) * maxconversations,
				  tablemode);
  if (ipcmain->convtbl_ipcid == -1) {
    mwlog(MWLOG_ERROR,"Failed to attach convserver table with id = %d reason %s",
	  ipcmain->convtbl_ipcid, strerror(errno));
    return -errno;
  };
  convtbl = (conv_entry *) shmat(ipcmain->convtbl_ipcid, (void *) 0, 0);
  mwlog(MWLOG_DEBUG,"convserver table attached at 0x%x",convtbl);

  /*** SHARED MEMORY DATA BUFFERS ***/
  ipcmain->heap_ipcid = shmb_format(heapmode, bufferbasesize, numbuffers);
  if (ipcmain->heap_ipcid < 0) {
    mwlog(MWLOG_ERROR,"Failed to creat heap reason %s",
	  strerror(errno));
    return -errno;
  };
  mwlog(MWLOG_DEBUG,"buffer heap id=%d attached ",
	ipcmain->heap_ipcid);
  return 0;

}

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

int cleanup_ipc()
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
    mwlog(MWLOG_WARNING, 
	  "Shared memory segment with key %#x is in use by another application", 
	  masteripckey);
    return -EBUSY;
  };
    
  mwlog(MWLOG_INFO, "Beginning cleaning up after previous instance");
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

/* after creation, fill in initial data in the tables. */
void
init_maininfo()
{
  
  strcpy(ipcmain->magic,_mwgetmagic());
  _mwgetversion(&ipcmain->vermajor,&ipcmain->vermajor,&ipcmain->vermajor);
  mwlog (MWLOG_DEBUG,"magc = %s version = %d.%d.%d\n", 
	 ipcmain->magic,ipcmain->vermajor,ipcmain->vermajor,ipcmain->vermajor);

  ipcmain->mwdpid = getpid();
  ipcmain->mwwdpid = -1;
  strcpy(ipcmain->mw_system_name,"Standalone MidWay");
  ipcmain->status = MWBOOTING;
  ipcmain->clttbl_length  = maxclients;
  ipcmain->srvtbl_length  = maxservers;
  ipcmain->svctbl_length  = maxservices;
  ipcmain->gwtbl_length   = maxgateways;
  ipcmain->convtbl_length = maxconversations;

  ipcmain->mwd_mqid = msgget(masteripckey,IPC_CREAT|queuemode);
  mwlog(MWLOG_DEBUG, " mwd mqid is %d", ipcmain->mwd_mqid);

  ipcmain->boottime = time(NULL);;
  ipcmain->lastactive = 0;
  ipcmain->configlastloaded = 0;
  ipcmain->shutdowntime = 0;
};
/* mark the main seg athat the mwd has gone down. */
int term_maininfo()
{
  int mainid;

  if (ipcmain == NULL) return -ENOTCONN;
  ipcmain->status = MWDEAD;
  msgctl(ipcmain->mwd_mqid, IPC_RMID, NULL);
  ipcmain->mwd_mqid = -1;
  ipcmain->mwdpid = -1;
  
  mainid = shmget(masteripckey, 0, 0);
  shmctl(mainid, IPC_RMID, NULL);
  shmdt(ipcmain);
  ipcmain = NULL;
  return 0;
};

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

  action.sa_flags = SA_RESETHAND;
  action.sa_handler = sighandler;
  sigfillset(&action.sa_mask);
  
  if (sigaction(SIGALRM, &action, NULL)) failed++;
  if (sigaction(SIGTERM, &action, NULL)) failed++;
  if (sigaction(SIGHUP, &action, NULL)) failed++;
  if (sigaction(SIGINT, &action, NULL)) failed++;
  if (sigaction(SIGQUIT, &action, NULL)) failed++;
  if (sigaction(SIGCHLD, &action, NULL)) failed++;
  if (sigaction(SIGUSR1, &action, NULL)) failed++;
  if (sigaction(SIGUSR2, &action, NULL)) failed++;

  if (failed != 0) {
    mwlog(MWLOG_ERROR, "failed to install %d signals handlers\n", failed);
    exit(-19);
  }
};

main(int argc, char ** argv)
{
  char c;
  int rc, loglevel, daemon = 0, n;
  char * nextdir, * thisdir, *tmphome, wd[256]; 
  
  /* obtaining info about who I am, must be fixed for suid, check masterd*/
  mepw = getpwuid(getuid());

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
      else usage();
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
      maxclients = atoi(optarg);
      break;
    case 'C':
      maxconversations = atoi(optarg);
      break;
    case 's':
      maxservers = atoi(optarg);
      break;
    case 'S':
      maxservices = atoi(optarg);
      break;
    case 'b':
      bufferbasesize = atoi(optarg);
      break;
    case 'B':
      break;
    case 'g':
      maxgateways = atoi(optarg);
      break;
    default:
      usage();
      break;
    }
  }

  /* the only argument allowed is the instance name. */
  if (optind +1  == argc) {
    instancename = optarg;
  } else if (optind == argc) {
    instancename = strdup(mepw->pw_name);
  } else {
    usage();
  };

  /* change for production */
  _mw_copy_on_stdout(TRUE);
  mwsetloglevel(MWLOG_DEBUG);

  _mwdecode_url(uri);
  if (_mwaddress.protocol != MWSYSVIPC) {
    mwlog(MWLOG_ERROR, "url prefix must be ipc for mwd, url=%s", uri);
    exit(-1);
  };
  masteripckey = _mwaddress.sysvipckey;

  /* Figure out the instance home. If not given by -D or thru env var MWHOME,
     Default to ~/MidWay/instancename */
  if (mwhome == NULL) {
    mwhome = getenv("MWHOME");
  };
  if (mwhome == NULL) {
    mwhome = malloc(strlen(mepw->pw_dir)+10+strlen(instancename));
    sprintf(mwhome,"%s/%s/%s", mepw->pw_dir, "MidWay", instancename);
  };

  /* We apply PATH_MAX == 255 here, just to make life easy. */
  if (strlen(mwhome) > 250) {
    mwlog(MWLOG_ERROR, "Path to MidWayHome is to long. %d is longer than max 250", strlen(mwhome));
    exit(-1);
  };

  /* now do chdir to mwhome, if fail, go thru and create directories as needed.
     If we fail to create them, abort. */
  rc = chdir(mwhome);
  if (rc != 0) {
    /* there was a failure, step thru, create as need and abort on failure.*/
    tmphome  = strdup(mwhome);
    /* absloute path*/
    if (tmphome[0] == '/') {
      chdir ("/"); /* that better be ok.*/
      thisdir = tmphome+1;
    } else {
      thisdir = tmphome;
    };

    do {
      nextdir = strchr(thisdir, '/');
      if (nextdir != NULL) {
	*nextdir = '\0';
	nextdir++;
      };
      getcwd(wd,255);
      rc = chdir(thisdir);
      if (rc != 0) {
	rc = mkdir(thisdir, 0777);
	if (rc != 0) {
	  mwlog(MWLOG_ERROR, "Failed to create the directory %s in directory %s reason %s", 
		thisdir, wd,strerror(errno));
	  exit(-1);
	};
	chdir(thisdir);
      };
      thisdir = nextdir;
    } while(thisdir != NULL);
    free(tmphome);
  }
   
  /* when we get here, CWD is mwhome and it exists.*/
    
  rc = chdir("log");
  if (rc != 0) {
    rc = mkdir ("log",0777);
    if (rc != 0) {
      mwlog(MWLOG_ERROR, "Failed to create the directory %s in directory %s reason %s", 
	    thisdir, wd,strerror(errno));
      exit(-1);
    };
    chdir("log");
  };
  getcwd(wd,255);
  strcat(wd,"/SYSTEM");
  mwsetlogprefix(wd);
  mwsetloglevel(loglevel);

  mwlog(MWLOG_INFO, "MidWay daemon starting with userid %s (uid=%d) proccessid=%d", 
	mepw->pw_name, getuid(), getpid());
  mwlog(MWLOG_INFO, "Version $Name$");
  mwlog(MWLOG_INFO, "%s", mwversion());
  mwlog(MWLOG_INFO, "MidWay instance name is %s", instancename); 
  

  /* more subdirs to follow. */

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
  if ( (maxclients < 0) ||
       (maxservers < 0) ||
       (maxservices < 0) ||
       (maxgateways < 0) ||
       (maxconversations < 0) ) {
    usage();
  }

  if (numbuffers < 0) {
    /* this is actually an important formula. Here we we try to
       estimate the number of shm buffer chunks needed. Currently this
       is just guess work.  */
    numbuffers = (maxclients + maxservers + maxgateways) * 2 ;
  };

  if (maxservices < maxservers) maxservices = maxservers;
  if (maxservices < maxgateways) maxservices = maxgateways;

  mwlog(MWLOG_INFO,"  max clients %d, servers %d, services %d, gateways %d, conversations %d\n",
	maxclients, maxservers, maxservices, maxgateways, maxconversations);

  /* We have a protection mode = 0 here. We should use umask.
   * but then again we should alse allow this to be set by cmdline 
   */
  rc = create_ipc(0);
  n = 0;
  while (rc < 0) {
    if (n > 5) {
      mwlog(MWLOG_ERROR, "This can't happen clean up of old ipc fails in %s line %d", 
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
  mwlog(MWLOG_DEBUG,"Shm data segments created.");

  init_maininfo();
  init_tables();
  
  /* module ipctables.c has a static defined ipcmain, we must set it. */
  _mw_set_shmadr (ipcmain, clttbl, srvtbl, svctbl, gwtbl, convtbl);

  if (daemon) {
    rc = fork();
    if (rc == -1) {
      mwlog(MWLOG_ERROR, "fork failed reason: %s", strerror(errno));
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
  /*
   * main loop of mwd, apart from occational timers, alle we do is
   * to waut for administrative request in message queue.
   *
   * Note that child processes will be TCP client servers and Gateway servers
   * they will have a diffrent main loop 
   */
  rc = start_watchdog();
  if (rc <= 0) {
    mwlog(MWLOG_ERROR, "MidWay WatchDog daemon failed to start. reason %d", rc);
    shm_destroy();
    term_tables();
    term_maininfo();
    exit(-1);
  };
  mwlog(MWLOG_INFO, "MidWay WatchDog daemon started pid=%d", rc); 
  mwlog(MWLOG_INFO, "MidWay daemon boot complete systemname is %s", ipcmain->mw_system_name); 
  _mw_copy_on_stdout(FALSE);

  fclose (stdin);
  fclose (stdout);
  fclose (stderr);

  ipcmain->status = MWNORMAL;
  time(&ipcmain->boottime);
  while(1) {
    rc = parse_request();
    if (rc == -ESHUTDOWN) break;
    if (ipcmain->status == MWDEAD) exit(-1);
    if (flags.terminate) {
      mwlog(MWLOG_INFO, "MidWay daemon initiated shutdown on signal %d", flags.terminate);
      ipcmain->shutdowntime = time(NULL);
      kill(SIGTERM, ipcmain->mwwdpid);
    };
  }
  mwlog(MWLOG_INFO, "MidWay daemon shutdown complete", rc);
  shm_destroy();
  term_tables();
  term_maininfo();
};
