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
 * Revision 1.9  2002/08/09 20:50:15  eggestad
 * A Major update for implemetation of events and Task API
 *
 * Revision 1.8  2002/07/07 22:45:48  eggestad
 * *** empty log message ***
 *
 * Revision 1.7  2001/10/03 22:56:12  eggestad
 * added multicast query
 * added view of gateway table
 *
 * Revision 1.6  2001/09/15 23:42:56  eggestad
 * fix for changing ipcmain systemname to instance name
 *
 * Revision 1.5  2001/08/29 17:57:59  eggestad
 * had declared a shutdown() function that collided with the syscall, renamed to cmd_shutdown
 *
 * Revision 1.4  2001/05/12 17:57:08  eggestad
 * call didn't print return buffer on failed service
 *
 * Revision 1.3  2000/11/29 23:19:54  eggestad
 * No data to service in call was illegal, now legal
 *
 * Revision 1.2  2000/07/20 19:42:34  eggestad
 * Listing of SRB clients fix.
 *
 * Revision 1.1.1.1  2000/03/21 21:04:20  eggestad
 * Initial Release
 *
 * Revision 1.1.1.1  2000/01/16 23:20:12  terje
 * MidWay
 *
 */


#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h> 
#include <sys/sem.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <ctype.h>
#include <getopt.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <net/if.h>

#include <MidWay.h>
#include <ipctables.h>
#include <ipcmessages.h>
#include <shmalloc.h>
#include <address.h>

static char * RCSId UNUSED = "$Id$";
static char * RCSName UNUSED = "$Name$"; /* CVS TAG */

/* globals from mwadm.c */
extern struct ipcmaininfo * ipcmain;
extern int extended;

/* We must provide a function for each entry in the 
   struct command  commands[]; in mwadm.c.
   mwadm.c expects these functions to print on stdout. 
   We expect that mwadm.c has spilt the command line on white space, 
   and gives the args as separate strings.

   In future this module will be used by mwd to provide these call as services.
   At that time, the results shall be encoded in another format than tty "print".
*/

char * location[5] = { 
  [GWLOCAL]   "Local   ", 
  [GWPEER]    "PeerDom ", 
  [GWREMOTE]  "Foreign ", 
  [GWCLIENT]  "Client  ", 
  NULL};


int info(int argc, char ** argv)
{
  if (ipcmain == NULL) {
    printf ("We are not connected to a MidWay system\n");
    return -1;
  };

  printf ("MidWay system has version %d.%d.%d\n", 
	  ipcmain->vermajor, ipcmain->verminor, ipcmain->patchlevel);
  printf ("  instance name \"%s\" id \"%s\"\n",
	  ipcmain->mw_instance_name==NULL?"(Anonymous)":ipcmain->mw_instance_name,
	  ipcmain->mw_instance_id==NULL?"(none)":ipcmain->mw_instance_id);

  printf ("  master Processid = %d status = %d boottime %s", 
	  ipcmain->mwdpid, ipcmain->status, ctime(&ipcmain->boottime));
  printf ("  Home directory=%s\n", 
	  ipcmain->mw_homedir);
  return 0;
};

static const char * clienttypestring(int type) 
{
  switch(type) {

  case MWIPCCLIENT:
  case MWNETCLIENT:
    return "Client";
  case MWIPCSERVER:
    return "Server";
  default:
    return "      ";
  };
};

int clients(int argc, char ** argv)
{
  int i, count = 0;
  cliententry * cltent;
  char * loc = "N/A     ";

  if (ipcmain == NULL) {
    printf ("We are not connected to a MidWay system\n");
    return -1;
  };
  
  printf ("Client table:\n");
  cltent = _mw_getcliententry(0);
  /*if (extended) printf ("table address %#x \n", 
    (long)cltent); */
  printf ("ClientID Type   Location Pid    msgqid Clientname\n");
  for (i = 0; i < ipcmain->clttbl_length; i++) {
    if (cltent[i].status != UNASSIGNED) {
      count ++;
      if (cltent[i].location == GWLOCAL)  loc = "LocalIPC";
      if (cltent[i].location == GWCLIENT) loc = "Network ";
      /*      if (extended) printf ("@ %#x ", &cltent[i]); */
      printf ("%8d %-6s %-8s %-6d %-6d %s\n", i,  clienttypestring(cltent[i].type), 
	      loc, cltent[i].pid, cltent[i].mqid, cltent[i].clientname);
    };
  }
  printf ("\n %d/%d (%5.3f%%) entries used\n", count,i, 
	    (float) count*100 / (float) i);
  return 0;
}

int servers(int argc, char ** argv)
{
  serverentry * srvent;
  serviceentry * svcent;
  int i, count = 0;
   
  if (ipcmain == NULL) {
    printf ("We are not connected to a MidWay system\n");
    return -1;
  };

  printf ("Server table:\n");
  srvent = _mw_getserverentry(0);
  svcent = _mw_getserviceentry(0);
  
  /*  if (extended) printf ("table address %#x \n", 
      (long)srvent);*/
  printf ("ServerID    Pid msgqid block Status Service\n");
  for (i = 0; i < ipcmain->srvtbl_length; i++) {
    if (srvent[i].status != UNASSIGNED) {
      count ++;
      /*if (extended) printf ("@ %#x ", &srvent[i]);*/
      /*	if (srvent[i].nowserving == UNASSIGNED) servicename = "(IDLE)";
		else svcent[srvent[i].nowserving & MWINDEXMASK].servicename;*/
      printf ("%8d %6d %6d %5d %6d %.32s\n", i, srvent[i].pid, 
	      srvent[i].mqid, srvent[i].mwdblock, srvent[i].status, 
	      srvent[i].statustext);
    };
  }
  printf ("\n %d/%d (%5.3f%%) entries used\n", count,i, 
	  (float) count*100 / (float) i);
  return 0 ;
};

int services(int argc, char ** argv)
{
  int i, count = 0;
  serviceentry * svcent;

  printf ("Services table:\n");
  svcent = _mw_getserviceentry(0);
  /*  if (extended) printf ("table address %#x \n", 
      (long)svcent);*/
  printf ("ServiceID type Location cost Srv/GWID Service\n");
  for (i = 0; i < ipcmain->svctbl_length; i++) {
    if (svcent[i].type != UNASSIGNED) {
      count ++;
      /* if (extended) printf ("@ %#x ", &svcent[i]);*/
      printf ("%9d %4d %-8s %4d %8d %s\n", i, svcent[i].type, 
	      location[svcent[i].location], 
	      svcent[i].cost, 
	      svcent[i].location?
	      MWINDEXMASK&svcent[i].gateway : MWINDEXMASK&svcent[i].server, 
	      svcent[i].servicename);
    };
  }
  printf ("\n %d/%d (%5.3f%%) entries used\n", count,i, 
	  (float) count*100 / (float) i);
  return 0 ;
};

int gateways(int argc, char ** argv)
{
  int i, count = 0;
  gatewayentry * gwent;
  char * roles[5] = { "NONE", "Clients", "Gateways", "All", NULL};

  printf ("Gateways table:\n");
  gwent = _mw_getgatewayentry(0);
  /*  if (extended) printf ("table address %#x \n", 
      (long)svcent);*/
  printf ("GWID Domain           location peer-roles pid   status instance peeraddr\n");
  for (i = 0; i < ipcmain->gwtbl_length; i++) {
    if (gwent[i].pid != UNASSIGNED) {
      count ++;
      /* if (extended) printf ("@ %#x ", &svcent[i]);*/
      printf ("%4d %-16.32s %8s %10s %5d %5d %6d %-.32s %s\n", 
	      i, 
	      gwent[i].domainname, 
	      location[gwent[i].location], 
	      roles[gwent[i].srbrole], 
	      gwent[i].pid, 
	      gwent[i].mqid, 
	      gwent[i].status,
	      gwent[i].instancename, _mw_sprintsa(&gwent[i].addr.sa, NULL));
      
    };
  }
  printf ("\n %d/%d (%5.3f%%) entries used\n", count,i, 
	  (float) count*100 / (float) i);
  return 0 ;
};


int heapinfo(int argc, char ** argv) 
{
  extern struct segmenthdr * _mwHeapInfo;
  unsigned short semarray[BINS];
  int i, rc;

  if (_mwHeapInfo == NULL) {
    printf("shm head not attached\n");
    return -1;
  };
  printf ("magic %x segnment size %d semid %d\n", 
	  _mwHeapInfo->magic, _mwHeapInfo->segmentsize, _mwHeapInfo->semid);
  printf (" Basechunksize %d chunkspersize %d Bins %d\n", 
	  _mwHeapInfo->basechunksize, _mwHeapInfo->chunkspersize, BINS);
  printf (" Chunks inuse=%d highwater=%d average=%d avgcount=%d\n", 
	  _mwHeapInfo->inusecount, _mwHeapInfo->inusehighwater, 
	  _mwHeapInfo->inuseaverage, _mwHeapInfo->inuseavgcount );


  rc = semctl(_mwHeapInfo->semid,BINS, GETALL, semarray);
  if (rc != 0) {
    printf ("semctl() failed, rc = %d reason %d\n",rc, errno);
    return -errno;
  };
  printf ("Chunksize freecount locked\n");
  for (i = 0; i < BINS; i++) {
    printf(" %8d  %8d   %s\n", 
	   (1<<i) * _mwHeapInfo->basechunksize, 
	   _mwHeapInfo->freecount[i], 
	   semarray[i] ? "no " : "yes");
  };
  return 0;
};

int query(int argc, char ** argv) 
{
  int i;
  char addr[INET_ADDRSTRLEN+1];
  struct sockaddr_in * inaddr;
  instanceinfo * replies;
  char * domain = NULL, * instance = NULL;
  
  if (argc == 2) {
    domain = argv[1];
  } 
  
  printf("Domain               Instance             Version  Address\n");
  replies = mwbrokerquery(domain, instance);
  if (replies) {
    for (i = 0; replies[i].version[0] != '\0'; i++) {
      addr[0] = '\0';
      inaddr = (struct sockaddr_in *) &replies[i].address;
      inet_ntop(AF_INET, 
		&inaddr->sin_addr, 
		addr, INET_ADDRSTRLEN);
      printf("%-20s %-20s %7s  AF_INET:%s:%d\n", 
	     replies[i].domain,
	     replies[i].instance,
	     replies[i].version,
	     addr, ntohs(inaddr->sin_port));      
    }
  };
  return i;
};
 

int boot(int argc, char ** argv)
{
  pid_t pid1, pid2;
  int status, rc;
  int fds[2], c, n;
  FILE * sin;

  pid1 = fork ();
  if (pid1 == 0) {
    close(0);
    close(1);
    rc = pipe(fds);
    if (rc != 0) {
      fprintf(stderr, "boot failed because: %s\n", strerror(errno));
      return -errno;
    };
    printf ("child 1\n");
    fflush(stdout);
    pid2 = fork();
    if (pid2 == 0) {
      printf("child 2 about to reassign stdout\n");

      close(fds[0]);
      /*stdout = fdopen(fds[1], "w");*/
      if (stdout == NULL) {
	 fprintf(stderr,"reasignment failed reason: %s", strerror(errno));
	 exit(-1);
      };
      rc = printf("child 2 reassignment complete\n");
      fprintf(stderr,"wrote %d bytes on new stdout, should be %d\n", 
	      rc, strlen("child 2 reassignment complete\n"));
      argv[0] = "-mwd";  
      execvp("mwd", argv); 
      fclose(stdout);
      sleep(10);
      fprintf(stderr,"boot failed, reason exec of mwd failed with: %s\n", strerror(errno)); 
      exit(0);
    } else {
      printf ("child 1 reading\n");
      n = 0;
      close(fds[1]);
      sin = fdopen(fds[0], "r");
      while(!feof(sin)) {
	c = fgetc(sin);
	/*	printf("got a char from sin %d\n", c);*/
	n++;
	if (c != EOF) {
	  fputc(c, stderr);
	  fflush(stderr);
	};
      };
      printf("sin empty after %d bytes\n",n);
      exit(0);
    };
  } 
  wait(&status);
  return 0;
};

int cmd_shutdown (int argc, char ** argv)
{
  Administrative admmesg;
  int rc;

  admmesg.mtype = ADMREQ;
  admmesg.opcode = ADMSHUTDOWN;
  admmesg.delay = 0;
  admmesg.cltid = _mw_get_my_clientid();
  
  rc = _mw_ipc_putmessage(0, (char *) &admmesg, sizeof(Administrative), 0);
  if (rc != 0) fprintf(stderr, "shutdown failed, reason %d", rc);
  return rc;
};

int dumpipcmain(int argc, char ** argv)
{
  const char * status_by_name[] = { "RUNNING", "SHUTDOWN", 
				    "BOOTING", "BUSY", "DEAD", NULL };
  cliententry  * clttbl  = NULL;
  serverentry  * srvtbl  = NULL;
  serviceentry * svctbl  = NULL;
  gatewayentry * gwtbl   = NULL;
  conv_entry   * convtbl = NULL;

  if (ipcmain == NULL) {
    printf ("\n ipcmain struct (shared memory) is not attached\n");
    return -1;
  };

  clttbl  = _mw_getcliententry(0);
  srvtbl  = _mw_getserverentry(0);
  svctbl  = _mw_getserviceentry(0);
  gwtbl   = _mw_getgatewayentry(0);
  convtbl = _mw_getconv_entry(0);

  printf ("\nIPCMAIN struct is located at %#X\n", ipcmain);
  
  printf ("Magic                = %-8s\n", ipcmain->magic);
  printf ("Versions             = %d.%d.%d\n", ipcmain->vermajor, 
	  ipcmain->verminor, ipcmain->patchlevel);
  printf ("master mwd pid       = %d\n", ipcmain->mwdpid);
  printf ("Watchdog pid         = %d\n", ipcmain->mwwdpid);
  printf ("Mwds Message queueid = %d\n", ipcmain->mwd_mqid);
  printf ("MW System name       = %s\n", ipcmain->mw_instance_name);
  printf ("Status               = (%d)%s\n", 
	  ipcmain->status, status_by_name[ipcmain->status]);
  printf ("Boottime             = %s", ctime(&ipcmain->boottime));
  printf ("lastactive           = %s", ctime(&ipcmain->lastactive));
  printf ("configlastloaded     = %s", ctime(&ipcmain->configlastloaded));
  printf ("shutdown time        = %s", ctime(&ipcmain->shutdowntime));

  printf("\n");
  printf ("Heap ipcid          = %d\n", ipcmain->heap_ipcid);
  printf ("Client  table ipcid = %d length = %7d at address %#X\n", 
	  ipcmain->clttbl_ipcid, ipcmain->clttbl_length, clttbl);
  printf ("Server  table ipcid = %d length = %7d at address %#X\n", 
	  ipcmain->srvtbl_ipcid, ipcmain->srvtbl_length, srvtbl);
  printf ("Service table ipcid = %d length = %7d at address %#X\n", 
	  ipcmain->svctbl_ipcid, ipcmain->svctbl_length, svctbl);
  printf ("Gateway table ipcid = %d length = %7d at address %#X\n", 
	  ipcmain->gwtbl_ipcid,  ipcmain->gwtbl_length,  gwtbl);
  printf ("Convers table ipcid = %d length = %7d at address %#X\n", 
	  ipcmain->convtbl_ipcid, ipcmain->convtbl_length, convtbl);
  return 0 ;
}

int event(int argc, char ** argv)
{
  int c, rc;
  char * username = NULL;
  char * clientname = NULL;
  char * data = NULL;
  char * eventname = NULL;
  if (ipcmain == NULL) {
    printf ("We are not connected to a MidWay system\n");
    return -1;
  };

  optind = 0;
  while( (c = getopt(argc, argv, "u:c:")) != -1) {
    switch(c) {
    case 'c':
      clientname = optarg;
      break;
    case 'r':
      username = optarg;
      break;
    };
  };

  switch (argc - optind) {

  case 2:
    data = argv[optind+1];
  case 1:
    eventname = argv[optind];
    break;

  default:
    goto usage;
  };

  rc = mwevent(eventname, data, 0, username, clientname);
  return rc ;

  usage:
  fprintf(stderr, "error in event: wrong number of args event [-u username] [-c clientname] eventname [data]\n", argc);
  return 0;
};

static void event_handler(char * event, char * data, int datalen)
{
  printf("  EVENT: %s data:%*.*s(%d)\n", event, datalen, datalen, data, datalen);
  return;
};

int sceleton(int argc, char ** argv) 
{
  int c;

  optind = 0;
  while( (c = getopt(argc, argv, "grR")) != -1) {
    switch(c) {
    case 'g':
      break;
    case 'r':
      break;
    case 'R':
      break;
    };
  };
  if ( (argc - optind) != 1) {
    fprintf(stderr, "error in ?? wrong number of args \n");
    return 0;
  };

  return 0;
};


int subscribe(int argc, char ** argv)
{
  int c, rc;
  int flags = 0;

  optind = 0;
  while( (c = getopt(argc, argv, "grR")) != -1) {
    switch(c) {
    case 'g':
      flags = MWEVGLOB;
      break;
    case 'r':
      flags = MWEVREGEXP;
      break;
    case 'R':
      flags = MWEVEREGEXP;
      break;
    };
  };
  if ( (argc - optind) != 1) {
    fprintf(stderr, "error in subscribe wrong number of optind = %d args %d\n", optind, argc);
    return 0;
  };

  rc = mwsubscribeCB(argv[optind], flags, event_handler);
  printf ("mwsubscribe returned %d\n", rc);
  return 0;

};

int unsubscribe(int argc, char ** argv)
{
  int rc;

  if (argc != 2) {
    fprintf(stderr, "error in %s wrong number of args\n", __FUNCTION__);
    return 0;
  };

  rc = mwunsubscribe(atoi(argv[1]));
  printf ("mwsubscribe returned %d\n", rc);
  return 0;
};

int call(int argc, char ** argv) 
{
  int i, j, len, apprc = 0, rc;
  char * data, * rdata = NULL;
  struct timeval start, end;
  //  long long llstart, llend;
  double secs;

  if ( (argc <= 1) || (argv[1] == NULL) ) {
    return -1;
  }

  if (argc > 2) {
    len = strlen(argv[2]);
    data = (char *) malloc(len);
    j = 0;
    for (i = 0; i < len; i++) {
      if (argv[2][i] == '%') {
	/* simlified, we accept only %UL where H is the high nibble
	   and L is teh lower nibble. Thus a char in hex.
	   fortunalty the lowe niddle in an ascii hex reprenentation
	   is the binary value */
	if ((i + 2) < len) {
	  data[j++] = argv[2][++i] & 0x0f << 4
	    + argv[2][++i] & 0x0f;
	} 
      } else {
	data[j++] = argv[2][i];
      }
    }
    data[j] = '\0';
  } else {
    data = NULL;
    j = 0;
  };
  DEBUG("about to call(%s, %s, %d, ...)", argv[1], data, j);
  gettimeofday(&start, NULL); 
  rc = mwcall(argv[1], data, j, &rdata, &len, &apprc, 0);
  gettimeofday(&end, NULL); 

  secs = end.tv_usec - start.tv_usec;
  secs /= 1000000;
  secs += end.tv_sec - start.tv_sec;

  if ( (rdata != NULL) && (len > 0) ) {
    data = NULL;
    data = realloc(data,len*3);
    j = 0;
    for (i = 0; i < len; i++) {
      if ( (!isprint(rdata[i])) || (rdata[i] == '%') ) {
	sprintf(&data[j], "%%%2.2x", (unsigned char) rdata[i]);
	j += 3;
      } else {
	data[j++] = rdata[i];
      };
    };
    
    
    data[j] = '\0';
    printf ("Call to \"%s\" returned %d(%s), with data \"%.*s\" %d bytes\n",  
	    argv[1], rc, rc==MWFAIL?"failed":rc==MWSUCCESS?"succeded":"mwmore", j, data, len);
  } else {
    printf ("Call to \"%s\" returned %d(%s), without data\n",  
	    argv[1], rc, rc?"fail":"success");    
  };
  printf ("  with application return code %d in %12.6f secs \n", apprc, secs);

  DEBUG("call to %s returned %s len %d bytes rc = %d apprc=%d in %12.6f secs\n", 
	argv[1], data, len, rc, apprc, secs);

  mwfree(rdata);

  return rc;
};


