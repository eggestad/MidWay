/*
  MidWay
  Copyright (C) 2001 Terje Eggestad

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
 * Revision 1.1  2002/02/17 13:40:57  eggestad
 * The XML configuration file
 *
 *
 */

#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/debugXML.h>

#ifndef _XMLCONFIG_H

xmlNodePtr mwConfigFindNode(xmlNodePtr start, ...);
int xmlConfigLoadFile(char * configfile);
int xmlConfigParseTree(void);


#endif 
