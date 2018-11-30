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
