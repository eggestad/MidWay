/*
  MidWay
  Copyright (C) 2002 Terje Eggestad

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

/*
 * $Log$
 * Revision 1.1  2002/07/07 22:45:48  eggestad
 * *** empty log message ***
 *
 */

#include <MidWay.h>
#include "toolbox.h"

static char * RCSId UNUSED = "$Id$";
static char * RCSName UNUSED = "$Name$";



/* this  is a function  that create a  list of strings, but  the char*
  array and all the strings  are placed in the same malloc()ed buffer,
  so that the whole list can be free()ed with a single free(). Much in
  the same manner  as the environ ** all processes  are passed.  We do
  however  sacrifice insert  speed for  this convinience,  this should
  only be used  for fairly short lists. The math  here is pretty hairy
  since  we  do   math  with  char**,  char*  and   int  in  the  same
  expresions. Terje */

char ** _charlistadd(char ** list, char * add, int flags)
{
  int n, s, ns, len;
  char * dst, * src;
  
  if (add == NULL) return list;

  /* if the first element we create a buffer that hold a char * [2]
     (The last pointer in the array must alway be NULL), and the string
     itself */
  if (list == NULL) {
    s = strlen(add) + sizeof(char *) * 2 + 1;
    DEBUG2("1: mallocing list  %d bytes long, strlen(add) = %d", 
	    s, strlen(add));
    list = malloc(s);
    
    DEBUG2("1: list at %p", list);
    list[0] = ((char * ) list) + sizeof(char *) * 2;
    DEBUG2("1: list[0] at %p  offset %d\n", list[0], (int)list[0] - (int)list);
    list[1] = NULL;
    
    strcpy(list[0], add);
    DEBUG2("1: copied in %s to %s", add, list[0]);
    return list;
  };
  
  // find the size of the char * [n] at the beginning. 
  for (n = 0; list[n] != NULL; n++) {

    /* if unieq is req, check to see if it already exists */
    if (flags &= UNIQUE) {
      if (strcmp(list[n], add) == 0) {
	DEBUG2("Uniqueness req, and new striung exists");
	return;
      };
    };
  };

  
  /* now the length of the current list (actually malloc()ed buffer)
     is the address of the last string, minus the addess of the list
     itself plus the length of the last string plus the \0 terminating
     the last string */
  s = (int) list[n-1] + strlen (list[n-1]) + 1 - (int) list;
  DEBUG2("list has  %d entries and is %d long", n, s);

  /* the new size if the old size plus a new entry in the char * [n+1]
     array at the beginning, plus the length of the new string with
     terminator */
  ns = s + sizeof(char *) + strlen(add)+1;
  list = realloc(list, ns);

  DEBUG2("list has new length %d bytes %d) added", ns, ns - s);

  /* now the char * array at the beginning must be extended with one
     entry, thus we must move the rest back. */ 

  /* NOTE: we can make an optimization here by extending the char *
     array with more than one. THe lest of teh array is given my the
     address of list[0] - the address of list / sizeof(char *). */

  dst = ((char *) list) + sizeof(char*)*(n+2);
  src = list[0];
  len = s - (sizeof(char*)*(n+1));
  
  DEBUG2("list at %p: memmove(%p(%d), %p(%d), %d)", 
	   list, dst, (int)dst - (int) list, src, (int)src - (int) list, len);
  memmove(dst, src, len);
  
  /* set to point if list[n] to the point we place the new string, and
     copy in the new string */
  list[n] = dst + len;
  DEBUG2("list[n(%d)] at %p", n, list[n]);

  strcpy(list[n], add);
  DEBUG2("copied in %s to %s", add, list[n]);

  /* NULL terminate the array */
  list[n+1] = NULL;
  DEBUG2("list[%d] = %p", n+1, NULL);
  n--;

  /* since we moved all the strings we must update all the old
     pointers in the char * array */
  while(n >= 0) {
    DEBUG2("list[%d] = %p", n, list[n]);
    list[n] += sizeof(char*);
    DEBUG2("list[%d] = %p", n, list[n]);
    n--;
  };
  return list; 
};

