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
 * Revision 1.7  2004/04/08 10:34:05  eggestad
 * introduced a struct with pointers to the functions implementing the midway functions
 * for a given protocol.
 * This is in preparation for be able to do configure with/without spesific protocol.
 * This creates a new internal API each protocol must addhere to.
 *
 * Revision 1.6  2002/09/05 23:25:33  eggestad
 * ipaddres in  mwaddress_t is now a union of all possible sockaddr_*
 * MWURL is now used in addition to MWADDRESS
 *
 * Revision 1.5  2002/08/09 20:50:15  eggestad
 * A Major update for implemetation of events and Task API
 *
 * Revision 1.4  2002/07/07 22:45:48  eggestad
 * *** empty log message ***
 *
 * Revision 1.3  2000/08/31 19:49:16  eggestad
 * - added domain to the address struct
 * - corrected som defs for protocol types.
 * - fix to prototype for _mwdecode_url(- added domain to the address struct
 * - corrected som defs for protocol types.
 * - fix to prototype for _mwdecode_url()
 *
 * Revision 1.2  2000/07/20 19:14:10  eggestad
 * fix up on double include prevention
 *
 * Revision 1.1.1.1  2000/03/21 21:04:03  eggestad
 * Initial Release
 *
 * Revision 1.1.1.1  2000/01/16 23:20:12  terje
 * MidWay
 *
 */

#ifndef _ADDRESS_H
#define _ADDRESS_H

#include <mwclientapi.h>

int _mwdecode_url(char * url, mwaddress_t * mwaddr);
const char * _mw_sprintsa(struct sockaddr * sa, char * buffer);

#endif


