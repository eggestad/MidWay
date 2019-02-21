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


typedef struct {
  /* signal flags */
  int terminate ;
  int childdied;
  int alarm;
  int user1;
  int user2;
} Flags;


#include <MidWay.h>

#ifndef _MWD_C
#include <ipctables.h>

extern ipcmaininfo * ipcmain;
extern char * uri;
extern char * instancename;
#endif

int do_shutdowntrigger(PTask pt);

typedef enum  { MAXCLIENTS = 10, MAXSERVERS, MAXSERVICES, MAXGATEWAYS, MAXCONVS, 
	       BUFFERBASESIZE, MASTERIPCKEY, NUMBUFFERS, UMASK } IPCPARAM;

void init_maininfo(void);
int cleanup_ipc(void) ;
int mymqid(void);
int mwdheapmode(void);
      
int mwdSetIPCparam(IPCPARAM, int);
int mwdGetIPCparam(IPCPARAM);
ipcmaininfo * getipcmaintable(void); 
cliententry * getcliententry(int i);
serverentry * getserverentry(int i);
serviceentry * getserviceentry(int i);
gatewayentry * getgatewayentry(int i);
conv_entry * getconv_entry(int i);

void usage(void);

#define MWSRVMGR ".mwsrvmgr"

void * serverManagerThread(void*);
