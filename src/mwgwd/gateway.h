/*
  MidWay
  Copyright (C) 2000 Terje Eggestad

  MidWay is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License as
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

/* $Id$ */

/*
 * $Log$
 * Revision 1.2  2000/08/31 22:08:31  eggestad
 * new global vars in gateway.c
 *
 * Revision 1.1  2000/07/20 18:49:59  eggestad
 * The SRB daemon
 *
 */
#include <urlencode.h>

typedef struct {
  int shutdownflag;
} globaldata;

int gwattachclient(int, int, char *, char *, char *, urlmap *);
int gwdetachclient(int);

int gwattachgateway(char *);
int gwdetachgateway(int);
int gwimportservice(char *, char *);
int gwunimportservice(char *, char *);

GATEWAYID allocgwid(int location, int role);
void freegwid(GATEWAYID gwid);

#ifndef _GATEWAY_C
extern char * mydomain;
extern char * myinstance;
#endif

