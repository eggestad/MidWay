/*
  MidWay
  Copyright (C) 2003 Terje Eggestad

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
 * Revision 1.1  2003/07/06 18:59:56  eggestad
 * introduced a table api for commands.c to return data in
 *
 */

#include "dtbl.h"

/* the functions that implement the commands. They calling convention is based on 
   The UNIX convention from exec() to programs.
   The basis is for this use is:
   - other programs than mwadm may call them
   - it is simple to convert a commandline from readline() to this by filling inn \0 for 
   white space, and just adding a pointer array.
   - a single command can be given on mwadm arguments.

   Past the argc and argv, the return data args. this is typically a
   DTable and an error string pointer. 

*/

int do_info          (int argc, char ** argv, DTable dtbl , char ** errstr);
int boot          (int argc, char ** argv);
int do_clients       (int argc, char ** argv, DTable dtbl , char ** errstr);
int do_servers       (int argc, char ** argv, DTable dtbl , char ** errstr);
int do_services      (int argc, char ** argv, DTable dtbl , char ** errstr);
int do_gateways      (int argc, char ** argv, DTable dtbl , char ** errstr);
int heapinfo      (int argc, char ** argv);
int dumpipcmain   (int argc, char ** argv);
int call          (int argc, char ** argv);
int event         (int argc, char ** argv);
int subscribe     (int argc, char ** argv);
int unsubscribe   (int argc, char ** argv);
int query         (int argc, char ** argv);
int cmd_shutdown  (int argc, char ** argv);

