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
 * Revision 1.8  2005/10/11 22:30:12  eggestad
 * removed unused externs
 *
 * Revision 1.7  2004/08/11 20:32:30  eggestad
 * - daemonize fix
 * - umask changes (Still wrong, but better)
 * - large buffer alloc
 *
 * Revision 1.6  2003/06/12 07:27:03  eggestad
 * sighandlers are now private, watchdog needed it's own
 *
 * Revision 1.5  2003/04/25 13:03:09  eggestad
 * - fix for new task API
 * - new shutdown procedure, now using a task
 *
 * Revision 1.4  2002/07/07 22:45:48  eggestad
 * *** empty log message ***
 *
 * Revision 1.3  2002/02/17 14:48:38  eggestad
 * - added missing includes
 * - added missing prototypes
 * - added IPC param aliases
 * - Added define for server manager service name
 *
 * Revision 1.2  2000/07/20 19:44:00  eggestad
 * prototype fixup.
 *
 * Revision 1.1.1.1  2000/03/21 21:04:24  eggestad
 * Initial Release
 *
 * Revision 1.1.1.1  2000/01/16 23:20:12  terje
 * MidWay
 *
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
