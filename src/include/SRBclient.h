/*
  The MidWay API
  Copyright (C) 2000 Terje Eggestad

  The MidWay API is free software; you can redistribute it and/or
  modify it under the terms of the GNU Library General Public License as
  published by the Free Software Foundation; either version 2 of the
  License, or (at your option) any later version.
  
  The MidWay API is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Library General Public License for more details.
  
  You should have received a copy of the GNU Library General Public
  License along with the MidWay distribution; see the file COPYING.  If not,
  write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA. 
*/


#include <address.h>

void _mwsrbprotosetup(mwaddress_t * mwadr);

int _mwattach_srb(mwaddress_t *mwadr, char * name, 
		  char * username, char * password, int flags);
int _mwdetach_srb(void);
int _mwacall_srb(char * svcname, char * data, int datalen, int flags);
int _mwfetch_srb(int * handle, char ** data, int * len, int * appreturncode, int flags);


int _mwevent_srb(char * evname, char * data, int datalen, char * username, char * clientname);
int _mwsubscribe_srb(char * pattern, int id, int flags);
int _mwunsubscribe_srb(int id); 

int _mw_drain_socket(int flags);
