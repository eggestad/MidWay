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
#include <netdb.h>

#include <MidWay.h>
#include <ipctables.h>
#include <ipcmessages.h>
#include <shmalloc.h>
#include <address.h>
#include <multicast.h>

#include "mwadm.h"
#include "dtbl.h"
#include "commands.h"

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
   [GWLOCAL]   = "Local   ", 
   [GWPEER]    = "PeerDom ", 
   [GWREMOTE]  = "Foreign ", 
   [GWCLIENT]  = "Client  ", 
   NULL};

char * status_by_name[7] = {
   [MWREADY]    = "Ready", 
   [MWSHUTDOWN] = "Shutdown", 
   [MWBOOTING]  = "Booting", 
   [MWBUSY]     = "Busy", 
   [MWDEAD]     = "Dead", 
   [MWBLOCKED]  = "Blocked", 
   NULL};

#define DATESTR "%a, %e %b %Y %H:%M:%S  %Z"

int do_info(int argc, char ** argv, DTable dtbl , char ** errstr)
{
   char date[64];
   if (ipcmain == NULL) {
      if (errstr)
	 *errstr = "We are not connected to a MidWay system";
      return -1;
   };
   mwaddress_t * mwadr =  _mw_get_mwaddress();
   
   dtbl_title(dtbl, "MidWay system Informaton");
   
   dtbl_style(dtbl, DTBL_STYLE_SIDE);
   dtbl_newrow(dtbl, NULL);
   dtbl_setfield(dtbl, -1, 0, "version");
   dtbl_setfield(dtbl, -1, 1, "%d.%d.%d", 
		 ipcmain->vermajor, ipcmain->verminor, ipcmain->patchlevel);
   
   
   dtbl_newrow(dtbl, NULL);
   dtbl_setfield(dtbl, -1, 0, "instance name");
   dtbl_setfield(dtbl, -1, 1, "%s", ipcmain->mw_instance_name); 

   dtbl_newrow(dtbl, NULL);
   dtbl_setfield(dtbl, -1, 0, "instance id");	
   dtbl_setfield(dtbl, -1, 1, "%s", ipcmain->mw_instance_id);

   dtbl_newrow(dtbl, NULL);
   dtbl_setfield(dtbl, -1, 0,  "mwd ProcessId"); 
   dtbl_setfield(dtbl, -1, 1, "%d", ipcmain->mwdpid);

   dtbl_newrow(dtbl, NULL);
   dtbl_setfield(dtbl, -1, 0,  "mwd Watchdog ProcessId"); 
   dtbl_setfield(dtbl, -1, 1, "%d", ipcmain->mwwdpid);

   dtbl_newrow(dtbl, NULL);
   dtbl_setfield(dtbl, -1, 0,  "mwd Message queue id"); 
   dtbl_setfield(dtbl, -1, 1, "%d", ipcmain->mwd_mqid);

   if (mwadr != NULL) {
      
      dtbl_newrow(dtbl, NULL);
      dtbl_setfield(dtbl, -1, 0,  "mwd IPCKEY"); 
      dtbl_setfield(dtbl, -1, 1, "%#x", mwadr->sysvipckey);

      dtbl_newrow(dtbl, NULL);
      dtbl_setfield(dtbl, -1, 0,  "mwd IPC url"); 
      dtbl_setfield(dtbl, -1, 1, "ipc:/%s ipc:%u",
		    ipcmain->mw_instance_name, mwadr->sysvipckey);
   }
   dtbl_newrow(dtbl, NULL);
   dtbl_setfield(dtbl, -1, 0,  "mwd last active"); 
   strftime(date, 64, DATESTR, localtime((time_t *)&ipcmain->lastactive));
   dtbl_setfield(dtbl, -1, 1, "%s", date);

   dtbl_newrow(dtbl, NULL);
   dtbl_setfield(dtbl, -1, 0, "status"); 
   dtbl_setfield(dtbl, -1, 1, "%s", status_by_name[ipcmain->status]);

   dtbl_newrow(dtbl, NULL);
   strftime(date, 64, DATESTR, localtime((time_t *)&ipcmain->boottime));
   dtbl_setfield(dtbl, -1, 0, "boottime"); 
   dtbl_setfield(dtbl, -1, 1, "%s", date);

   if (ipcmain->configlastloaded) {
      dtbl_newrow(dtbl, NULL);
      dtbl_setfield(dtbl, -1, 0,  "configlastloaded"); 
      strftime(date, 64, DATESTR, localtime((time_t *)&ipcmain->configlastloaded));
      dtbl_setfield(dtbl, -1, 1, "%s", date);
   };
   if (ipcmain->shutdowntime) {
      dtbl_newrow(dtbl, NULL);
      dtbl_setfield(dtbl, -1, 0,  "shutdowntime"); 
      strftime(date, 64, DATESTR, localtime((time_t *)&ipcmain->shutdowntime));
      dtbl_setfield(dtbl, -1, 1, "%s", date);
   };
   dtbl_newrow(dtbl, NULL);
   dtbl_setfield(dtbl, -1, 0, "Home directory"); 
   dtbl_setfield(dtbl, -1, 1, "%s", ipcmain->mw_homedir);
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

int do_clients(int argc, char ** argv, DTable dtbl, char ** retstr)
{
   int i, count = 0;
   cliententry * cltent;
   char * loc = "N/A     ";

   if (ipcmain == NULL) {
      if (retstr)
	 *retstr = "We are not connected to a MidWay system";
      return -1;
   };
  
   dtbl_title(dtbl, "Client table:");

   cltent = _mw_getcliententry(0);

   /*if (extended) printf ("table address %#x \n", 
     (long)cltent); */
   dtbl_headers (dtbl, 
		 "ClientID", "Type", "Location", "Pid", "msgqid", "Clientname", "address", 
		 NULL);
   dtbl_col_format(dtbl, 0, DTBL_ALIGN_RIGHT, -1);
   dtbl_col_format(dtbl, 3, DTBL_ALIGN_RIGHT, -1);
   dtbl_col_format(dtbl, 4, DTBL_ALIGN_RIGHT, -1);

   for (i = 0; i < ipcmain->clttbl_length; i++) {
      if (cltent[i].status != UNASSIGNED) {
	 count ++;
	 dtbl_newrow(dtbl, NULL);
	 dtbl_setfield(dtbl, -1, 0, "%d", i);

	 dtbl_setfield(dtbl, -1, 1, "%s", clienttypestring(cltent[i].type));

	 if (cltent[i].location == GWLOCAL)  loc = "LocalIPC";
	 if (cltent[i].location == GWCLIENT) loc = "Network ";
      
	 dtbl_setfield(dtbl, -1, 2, "%s", loc);
      
	 dtbl_setfield(dtbl, -1, 3, "%d", cltent[i].pid);
	 dtbl_setfield(dtbl, -1, 4, "%d", cltent[i].mqid);
	 dtbl_setfield(dtbl, -1, 5, "%s", cltent[i].clientname);
	 dtbl_setfield(dtbl, -1, 6, "%s", cltent[i].addr_string);

      };
   }
   dtbl_subtext (dtbl, "%d/%d (%5.3f%%) entries used", count,i, 
		 (float) count*100 / (float) i);
   return 0;
}

int do_servers(int argc, char ** argv, DTable dtbl , char ** errstr)
{
   serverentry * srvent;
   serviceentry * svcent;
   int i, count = 0;
   
   if (ipcmain == NULL) {
      if (errstr)
	 *errstr = "We are not connected to a MidWay system";
      return -1;
   };

   dtbl_title(dtbl, "Server table:");
   srvent = _mw_getserverentry(0);
   svcent = _mw_getserviceentry(0);
  
   /*  if (extended) printf ("table address %#x \n", 
       (long)srvent);*/

   dtbl_headers (dtbl, "ServerID", "Pid", "msgqid", "block", "Status", "Service", NULL);

   dtbl_col_format(dtbl, 0, DTBL_ALIGN_RIGHT, -1);
   dtbl_col_format(dtbl, 1, DTBL_ALIGN_RIGHT, -1);
   dtbl_col_format(dtbl, 2, DTBL_ALIGN_RIGHT, -1);
   dtbl_col_format(dtbl, 3, DTBL_ALIGN_RIGHT, -1);
   dtbl_col_format(dtbl, 4, DTBL_ALIGN_RIGHT, -1);

   for (i = 0; i < ipcmain->srvtbl_length; i++) {
      if (srvent[i].status != UNASSIGNED) {
	 count ++;
	 dtbl_newrow(dtbl, NULL);
	 /*if (extended) printf ("@ %#x ", &srvent[i]);*/
	 /*	if (srvent[i].nowserving == UNASSIGNED) servicename = "(IDLE)";
		else svcent[srvent[i].nowserving & MWINDEXMASK].servicename;*/

	 dtbl_setfield(dtbl, -1, 0, "%d", i);
	 dtbl_setfield(dtbl, -1, 1, "%d", srvent[i].pid);
	 dtbl_setfield(dtbl, -1, 2, "%d", srvent[i].mqid); 
	 dtbl_setfield(dtbl, -1, 3, "%d", srvent[i].mwdblock);
	 dtbl_setfield(dtbl, -1, 4, "%d", srvent[i].status);
	 dtbl_setfield(dtbl, -1, 5, "%s", srvent[i].statustext);

      };
   }
   dtbl_subtext (dtbl, "%d/%d (%5.3f%%) entries used", count,i, 
		 (float) count*100 / (float) i);
   return 0 ;
};

int do_services(int argc, char ** argv, DTable dtbl , char ** errstr)
{
   int i, count = 0, provider;
   serviceentry * svcent;

   if (ipcmain == NULL) {
      if (errstr)
	 *errstr = "We are not connected to a MidWay system";
      return -1;
   };

   dtbl_title(dtbl, "Services table:");
   svcent = _mw_getserviceentry(0);
   /*  if (extended) printf ("table address %#x \n", 
       (long)svcent);*/

   dtbl_headers (dtbl, "ServiceID", "type", "Location", "cost", "Srv/GWID", "Service", NULL);

   dtbl_col_format(dtbl, 0, DTBL_ALIGN_RIGHT, -1);
   dtbl_col_format(dtbl, 1, DTBL_ALIGN_RIGHT, -1);
   dtbl_col_format(dtbl, 3, DTBL_ALIGN_RIGHT, -1);
   dtbl_col_format(dtbl, 4, DTBL_ALIGN_RIGHT, -1);

   for (i = 0; i < ipcmain->svctbl_length; i++) {
      if (svcent[i].type != UNASSIGNED) {
	 count ++;
	 dtbl_newrow(dtbl, NULL);

	 if (svcent[i].location == GWLOCAL) 
	    provider = SRVID2IDX(svcent[i].server);
	 else 
	    provider = GWID2IDX(svcent[i].gateway);
 
	 /* if (extended) printf ("@ %#x ", &svcent[i]);*/

	 dtbl_setfield(dtbl, -1, 0, "%d", i);
	 dtbl_setfield(dtbl, -1, 1, "%d", svcent[i].type);
	 dtbl_setfield(dtbl, -1, 2, "%s", location[svcent[i].location]);
	 dtbl_setfield(dtbl, -1, 3, "%d", svcent[i].cost);
	 dtbl_setfield(dtbl, -1, 4, "%d", provider);
	 dtbl_setfield(dtbl, -1, 5, "%s", svcent[i].servicename);
      };
   }
   dtbl_subtext (dtbl, "%d/%d (%5.3f%%) entries used", count,i, 
		 (float) count*100 / (float) i);
   return 0 ;
};

int do_gateways(int argc, char ** argv, DTable dtbl , char ** errstr)
{
   int i, count = 0;
   gatewayentry * gwent;
   char * roles[5] = { "NONE", "Clients", "Gateways", "All", NULL};

   if (ipcmain == NULL) {
      if (errstr)
	 *errstr = "We are not connected to a MidWay system";
      return -1;
   };

   dtbl_title(dtbl, "Gateways table:\n");
   gwent = _mw_getgatewayentry(0);
   /*  if (extended) printf ("table address %#x \n", 
       (long)svcent);*/

   dtbl_headers (dtbl, "GWID", "Domain", "Location", "peer-roles", "Pid", "mqid", "Status", 
		 "Instance", "Peer Address", NULL);

   dtbl_col_format(dtbl, 0, DTBL_ALIGN_RIGHT, -1);
   dtbl_col_format(dtbl, 4, DTBL_ALIGN_RIGHT, -1);
   dtbl_col_format(dtbl, 5, DTBL_ALIGN_RIGHT, -1);
   dtbl_col_format(dtbl, 6, DTBL_ALIGN_RIGHT, -1);

   for (i = 0; i < ipcmain->gwtbl_length; i++) {
      if (gwent[i].pid != UNASSIGNED) {
	 count ++;
	 dtbl_newrow(dtbl, NULL);

	 /* if (extended) printf ("@ %#x ", &svcent[i]);*/

	 dtbl_setfield(dtbl, -1, 0, "%d", i); 
	 dtbl_setfield(dtbl, -1, 1, "%s", gwent[i].domainname);
	 dtbl_setfield(dtbl, -1, 2, "%s", location[gwent[i].location]);
	 dtbl_setfield(dtbl, -1, 3, "%s", roles[gwent[i].srbrole]);
	 dtbl_setfield(dtbl, -1, 4, "%d", gwent[i].pid);
	 dtbl_setfield(dtbl, -1, 5, "%d", gwent[i].mqid);
	 dtbl_setfield(dtbl, -1, 6, "%d", gwent[i].status);
	 dtbl_setfield(dtbl, -1, 7, "%s", gwent[i].instancename);
	 dtbl_setfield(dtbl, -1, 8, "%s", gwent[i].addr_string);      
      };
   }
   dtbl_subtext (dtbl, "%d/%d (%5.3f%%) entries used", count,i, 
		 (float) count*100 / (float) i);
   return 0 ;
};


int heapinfo(int argc, char ** argv) 
{
   extern struct segmenthdr * _mwHeapInfo;
   unsigned short semarray[BINS];
   int i, rc;
   int c;
   int listall = 0, listinuse = 0;
   int l;
   int fmt = 0, dumpdata = 0;
   int writetofile = 0;
   FILE * fp = stdout;
   int offset = 0, s;
   chunkhead * pChead;
   chunkfoot * pCFoot;

   if (_mwHeapInfo == NULL) {
      printf("shm head not attached\n");
      return -1;
   };
   debug("parsing args");
   optind = 0;
   while( (c = getopt(argc, argv, "uao:xd")) != -1) {
      debug("got the option %c", (char)c);
      switch(c) {
      case 'u':
	 listinuse = 1;
	 break;
	 
      case 'a':
	 listall = 1;
	 break;
      
      case 'o':
	 fp = fopen(optarg, "w");
	 writetofile = 1;
	 break;

      case 'x':
	 fmt = 1;
	 break;

      case 'd':
	 dumpdata = 1;
	 break;
      };
   };

   if (listall) listinuse = 1;

   fprintf (fp,"magic %x segment size %llu semid %lld\n", 
	   _mwHeapInfo->magic, _mwHeapInfo->segmentsize, _mwHeapInfo->semid);
   fprintf (fp," Basechunksize %d chunkspersize %d Bins %d\n", 
	   _mwHeapInfo->basechunksize, _mwHeapInfo->chunkspersize, BINS);
   fprintf (fp," Chunks inuse=%d highwater=%d average=%d avgcount=%d\n", 
	   _mwHeapInfo->inusecount, _mwHeapInfo->inusehighwater, 
	   _mwHeapInfo->inuseaverage, _mwHeapInfo->inuseavgcount );

   fprintf (fp," top=%llu bottom=%llu \n", _mwHeapInfo->top, _mwHeapInfo->bottom);

   rc = semctl(_mwHeapInfo->semid,BINS, GETALL, semarray);
   if (rc != 0) {
      printf ("semctl() failed, rc = %d reason %d\n",rc, errno);
      return -errno;
   };
   
   if  (listall || listinuse) {
      fprintf (fp,"  %16s: header=(%16s: %10s %8s) footer=(%16s: %16s %16s %16s\n",
	      "Buffer offset", "Header offset", "Ownerid", "Size", 
	      "Footer offset", "Above offset", "Previous", "Next");      
   } else {
      fprintf (fp,"Chunksize freecount locked\n");
   };
   
   for (i = 0; i < BINS-1; i++) {

      if (listall || listinuse) {
	 long long  h,f;
	 offset =  _mw_gettopofbin(_mwHeapInfo, i);
	 s = (1 << i) *_mwHeapInfo ->basechunksize;
	 debug("listing chunks start %p end %p", _mwHeapInfo, ((void *)_mwHeapInfo) + _mwHeapInfo->segmentsize );
	 for (l = 0; l < _mwHeapInfo->chunkspersize; l++) {
	    c = (s + CHUNKOVERHEAD) * l;
	    
	    h =  offset + c;
	    f = h + sizeof(chunkhead) + s;
	    pChead = ((void*) _mwHeapInfo) + h;
	    pCFoot = ((void*) _mwHeapInfo) + f; 
	    
	    debug ("header %p(%lld) owner %x, size %llu", pChead, h, pChead->ownerid,  pChead->size);
	    debug("footer %p(%lld) %llu %llu %llu", pCFoot, f, pCFoot->above, pCFoot->next, pCFoot->prev);
	    if (listall || (listinuse && pChead->ownerid != UNASSIGNED)) {
	       fprintf (fp,"  %16lld: header=(%16lld: %10s %8llu) footer=(%16lld: %16llu %16llu %16llu)",
		       h+sizeof(chunkhead), 
		       h, _mwid2str(pChead->ownerid, NULL),  pChead->size * _mwHeapInfo->basechunksize,
		       f, pCFoot->above, pCFoot->next, pCFoot->prev);
	       if (dumpdata) {
		  char * buf;
		  buf = (char *) ( ((void*) _mwHeapInfo) + h + sizeof(chunkhead));
		  fprintf(fp, " data=(");;
		  if(fmt) {
		     int n; 
		     for (n = 0; n < pChead->size * _mwHeapInfo->basechunksize; n++) {
			fprintf(fp, "%hhx ", buf[n]);
		     };
		  } else {
		     fprintf(fp, "%.*s ", (int) (pChead->size * _mwHeapInfo->basechunksize), buf);
		  };
	       }
	       fprintf(fp, "\n");	       
	    }
	 }
	 
      } else {
	 fprintf(fp," %8d  %8d   %s\n", 
		 (1<<i) * _mwHeapInfo->basechunksize, 
		 _mwHeapInfo->freecount[i],
		 semarray[i] ? "no " : "yes");
      };
   }
   if (writetofile) fclose(fp);
   return 0;
};

int query(int argc, char ** argv) 
{
   int s, rc;
   char addr[INET_ADDRSTRLEN+1];
   char * hostname = NULL;
   struct sockaddr_in * inaddr;
   instanceinfo reply;
   char * domain = NULL, * instance = NULL;

   struct addrinfo hints = { 0 };
   hints.ai_flags = 0; //AI_CANONNAME;
   hints.ai_family = AF_INET; // when we support IP6 : AF_UNSPEC;
   hints.ai_socktype = SOCK_DGRAM;
   
   struct addrinfo *res = NULL;
   char dftport[16];
   snprintf(dftport, 16, "%d", SRB_BROKER_PORT);
   char * port = dftport;
   int c;
   
   while( (c = getopt(argc, argv, "h:p:")) != -1) {
      switch(c) {
      case 'h':
	 hostname = optarg;
	 break;

      case 'p':
	 port = optarg;
	 break;

      default:
	 fprintf(stderr, "error parsing args \n");
	 return -1;
      };
   };

   if ( (argc - optind) > 1) {
      fprintf(stderr, "error argc %d optind %d \n", argc, optind);
      fprintf(stderr, "error in query wrong number of args \n");
      return 0;
   };

   if ( (argc - optind) == 1) {
      domain = argv[optind];
   } 

   s = socket(AF_INET, SOCK_DGRAM, 0);
   if (s == -1) return -errno;
   if (hostname == NULL) {
      rc = _mw_sendmcastquery(s, domain, instance);
      if (rc == -1) {
	 close(s);
	 return rc;
      };
   } else {
      int rc = getaddrinfo(hostname, port, &hints, &res);
      if (rc != 0) {
	 Error("Failed to resolve hostname %s", hostname);
	 return rc;
      }
      while(res != NULL) {
	 if (res->ai_family == AF_INET) {
	    struct sockaddr_in *in = (struct sockaddr_in *) res->ai_addr;
	    rc = _mw_sendunicastquery(s, *in, domain, instance);
	 }
	 res = res->ai_next;
      }
      freeaddrinfo(res);
   }
   printf("Domain               Instance             Version  Address\n");

   rc = 0;
   /* we wait for 2-3 secs on replies */
   while (rc == 0) { 
      memset(&reply, '\0', sizeof(instanceinfo));
      rc =  _mw_getmcastreply(s, &reply, 2.0);
    
      if (rc == -1) {
	 switch (errno) {
	 case EBADMSG:
	    rc = 0;
	 };
	 continue;	  
      };

      addr[0] = '\0';
      inaddr = (struct sockaddr_in *) &reply.address;
      inet_ntop(AF_INET, 
		&inaddr->sin_addr, 
		addr, INET_ADDRSTRLEN);
      printf("%-20s %-20s %7s  AF_INET:%s:%d\n", 
	     reply.domain,
	     reply.instance,
	     reply.version,
	     addr, ntohs(inaddr->sin_port));          
   }

   close(s);
   return 0;
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
	 fprintf(stderr,"wrote %d bytes on new stdout, should be %lu\n", 
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
   
   while(( rc= attach(0, NULL)) < 0) usleep(100000);;

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
   if (rc != 0) 
      fprintf(stderr, "shutdown failed, reason %d", rc);
   else 
      detach(0, NULL);
   return rc;
};

int dumpipcmain(int argc, char ** argv)
{
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
#ifdef CONV
   convtbl = _mw_getconv_entry(0);
#endif

   printf ("\nIPCMAIN struct is located at %p\n", ipcmain);
  
   printf ("Magic                = %-8s\n", ipcmain->magic);
   printf ("Versions             = %d.%d.%d\n", 
	   ipcmain->vermajor, 
	   ipcmain->verminor, 
	   ipcmain->patchlevel);
   printf ("master mwd pid       = %d\n", ipcmain->mwdpid);
   printf ("Watchdog pid         = %d\n", ipcmain->mwwdpid);
   printf ("Mwds Message queueid = %d\n", ipcmain->mwd_mqid);
   printf ("MW System name       = %s\n", ipcmain->mw_instance_name);
   printf ("MW System ID         = %s\n", ipcmain->mw_instance_id);
   printf ("Home Directory       = %s\n", ipcmain->mw_homedir);
   printf ("Data Directory       = %s\n", ipcmain->mw_bufferdir);


   printf ("Status               = (%d)%s\n", 
	   ipcmain->status, status_by_name[ipcmain->status]);
   printf ("Boottime             = %s", ctime((time_t*)&ipcmain->boottime));
   printf ("lastactive           = %s", ctime((time_t*)&ipcmain->lastactive));
   printf ("configlastloaded     = %s", ctime((time_t*)&ipcmain->configlastloaded));
   printf ("shutdown time        = %s", ctime((time_t*)&ipcmain->shutdowntime));

   printf("\n");
   printf ("Heap ipcid          = %d\n", ipcmain->heap_ipcid);
   printf ("Client  table ipcid = %d length = %7d at address %p\n", 
	   ipcmain->clttbl_ipcid, ipcmain->clttbl_length, clttbl);
   printf ("Server  table ipcid = %d length = %7d at address %p\n", 
	   ipcmain->srvtbl_ipcid, ipcmain->srvtbl_length, srvtbl);
   printf ("Service table ipcid = %d length = %7d at address %p\n", 
	   ipcmain->svctbl_ipcid, ipcmain->svctbl_length, svctbl);
   printf ("Gateway table ipcid = %d length = %7d at address %p\n", 
	   ipcmain->gwtbl_ipcid,  ipcmain->gwtbl_length,  gwtbl);
   printf ("Convers table ipcid = %d length = %7d at address %p\n", 
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
   fprintf(stderr, 
	   "error in event: wrong number of args event [-u username] [-c clientname] eventname [data]\n");
   return 0;
};

static void event_handler(const char * event, const char * data, size_t datalen)
{
   printf("  EVENT: %s data:%*.*s(%zu)\n", event, (int) datalen, (int)datalen, data, datalen);
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
   int i, j,  apprc = 0, rc;
   size_t len;
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
	       data[j++] = argv[2][i+1] & (0x0f << 4)
		  + argv[2][i+2] & 0x0f;
	       i+=2;
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

   char * resultlbl = "fail";
   switch (rc) {
   case MWSUCCESS:
      resultlbl = "success";
      break;
   case MWFAIL:
      resultlbl = "fail";
      break;
   case MWMORE:
      resultlbl = "more";
      break;
   }
   
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
      printf ("Call to \"%s\" returned %d(%s), with data \"%.*s\" %zu bytes\n",  
	      argv[1], rc, resultlbl, j, data, len);
   } else {
      printf ("Call to \"%s\" returned %d(%s), without data\n",  
	      argv[1], rc, resultlbl);    
   };
   printf ("  with application return code %d in %12.6f secs \n", apprc, secs);

   DEBUG("call to %s returned %s len %zu bytes rc = %d apprc=%d in %12.6f secs\n", 
	 argv[1], data, len, rc, apprc, secs);

   mwfree(rdata);

   return rc;
};

/* Emacs C indention
Local variables:
c-basic-offset: 3
End:
*/

