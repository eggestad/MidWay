/*
  The MidWay API
  Copyright (C) 2002 Terje Eggestad

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
 * Revision 1.1  2002/07/07 22:45:48  eggestad
 * *** empty log message ***
 *
 *
 */

struct _peerlink {
  struct gwpeerinfo * peer;
  struct _peerlink * next;
};

typedef struct _peerlink peerlink;

struct _Import {
  char servicename[MWMAXSVCNAME];
  SERVICEID svcid;
  int cost;
  peerlink * peerlist;
  struct _Import * next;
};

typedef struct _Import Import;

struct _Export {
  char servicename[MWMAXSVCNAME];
  peerlink * peerlist;
  int cost;
  struct _Export * next;
};

typedef struct _Export Export;



int importservice(char *, int, struct gwpeerinfo *);
int unimportservice(char *, char *);
int exportservicetopeer(char *, struct gwpeerinfo * );
//int exportservice(char *);
int unexportservice(char *);
