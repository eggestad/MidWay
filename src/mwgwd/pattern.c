/*
  MidWay
  Copyright (C) 2002 Terje Eggestad

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
 * $Log$
 * Revision 1.1  2002/07/07 22:45:48  eggestad
 * *** empty log message ***
 *
 * 
 */

#include <string.h>

#include <MidWay.h>
#include "pattern.h"

static char * RCSId UNUSED = "$Id$";


int getPatternType(char * patterntype)
{
  if (strcasecmp("string", patterntype) == 0) return GW_PATTERN_STRING;
  if (strcasecmp("regexp", patterntype) == 0) return GW_PATTERN_REGEXP_BASIC;
  if (strcasecmp("basic", patterntype) == 0)  return GW_PATTERN_REGEXP_BASIC;
  if (strcasecmp("reg_extended", patterntype) == 0) return GW_PATTERN_REGEXP_EXTENDED;
  if (strcasecmp("regexp_ext", patterntype) == 0)   return GW_PATTERN_REGEXP_EXTENDED;
  if (strcasecmp("glob", patterntype) == 0) return GW_PATTERN_GLOB;
  return -1;
};

