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

/*
 * $Id$
 * $Name$
 * 
 * $Log$
 * Revision 1.3  2004/03/01 12:56:14  eggestad
 * added event API for SRB client
 *
 * Revision 1.2  2002/08/09 20:50:15  eggestad
 * A Major update for implemetation of events and Task API
 *
 * Revision 1.1  2002/07/07 22:45:48  eggestad
 * *** empty log message ***
 *
 *
 */

#include <address.h>

int _mwattach_srb(mwaddress_t *mwadr, char * name, 
		  char * username, char * password, int flags);
int _mwdetach_srb(void);
int _mwacall_srb(char * svcname, char * data, int datalen, int flags);
int _mwfetch_srb(int * handle, char ** data, int * len, int * appreturncode, int flags);


int _mwevent_srb(char * evname, char * data, int datalen, char * username, char * clientname);
int _mwsubscribe_srb(char * pattern, int flags);
int _mwunsubscribe_srb(char * pattern, int flags); 
