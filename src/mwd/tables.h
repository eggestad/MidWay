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
 * Revision 1.9  2003/06/12 07:33:15  eggestad
 *  numerous fixes to check_tables()
 *
 * Revision 1.8  2003/04/25 13:03:12  eggestad
 * - fix for new task API
 * - new shutdown procedure, now using a task
 *
 * Revision 1.7  2002/10/03 21:14:30  eggestad
 * - cost field in provide was ignored, now correctly done
 *
 * Revision 1.6  2002/09/04 07:13:31  eggestad
 * mwd now sends an event on service (un)provide
 *
 * Revision 1.5  2002/08/09 20:50:16  eggestad
 * A Major update for implemetation of events and Task API
 *
 * Revision 1.4  2002/02/17 17:57:12  eggestad
 * - added prototype for delallservices()
 *
 * Revision 1.3  2001/08/29 17:57:59  eggestad
 * had declared a shutdown() function that collided with the syscall, renamed to cmd_shutdown
 *
 * Revision 1.2  2000/07/20 19:50:19  eggestad
 * prototype fixup.
 *
 * Revision 1.1.1.1  2000/03/21 21:04:30  eggestad
 * Initial Release
 *
 * Revision 1.1.1.1  2000/01/16 23:20:12  terje
 * MidWay
 *
 */
#ifndef _TABLES_C
#define _TABLES_C

extern cliententry  * clttbl;
extern serverentry  * srvtbl;
extern serviceentry * svctbl;
extern gatewayentry * gwtbl;
extern conv_entry   * convtbl;
#endif


#include <MidWay.h>
#include <ipctables.h>

void init_tables(void);
void term_tables(void);

SERVERID addserver(char * name, int mqid, pid_t pid);
CLIENTID addclient(int type, char * name, int mqid, pid_t pid, int sid);

int delserver(SERVERID sid);
int delclient(CLIENTID cid);
int stop_server(SERVERID sid);


SERVICEID addlocalservice(SERVERID srvid, char * name, int type);
SERVICEID addremoteservice(GATEWAYID gwid, char * name, int cost, int type);
int delservice(SERVICEID svcid);
int delallservices(SERVERID srvid);

serverentry *  _mw_get_server_byid(SERVERID);

int kill_all_servers(int);
void hard_disconnect_ipc(void);
int check_tables(void);
int get_pids(int *, int **);
