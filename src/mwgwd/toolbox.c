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
 * Revision 1.3  2002/11/18 00:24:26  eggestad
 * - charlistadd() just didnæt work, major corruption if realloc returned
 *   buffer at another address.
 *
 * Revision 1.2  2002/08/09 20:50:16  eggestad
 * A Major update for implemetation of events and Task API
 *
 * Revision 1.1  2002/07/07 22:45:48  eggestad
 * *** empty log message ***
 *
 */

#include <string.h>
#include <ctype.h>

#include <MidWay.h>
#include "toolbox.h"

static char * RCSId UNUSED = "$Id$";


#ifdef DEBUGGING
static void memprint(char * mem, int len)
{
  int i;
  char * here;
  here = mem;
  DEBUG4("memprint %p", mem);
  for (i = 0; i < len; i++) {
    here = mem + i;;
    DEBUG4 ("    %3d  %p +%3ld %2.2hhx (%c) ", 
	    i, here, ((long)here - (long) mem), 
	    *here, isprint(*here)?*here:' ');
  };
  DEBUG4("memprint end");
};

  
static void printlist(char ** list)
{
  int len, l;
  char * start, * end, *s;
  char ** pptr;
  char line[8192]; 

  DEBUG4("PRINTLIST : START");
  s = (char *) list;


  pptr = list;
  while(*pptr != NULL) {
    DEBUG4("   -  %s", *pptr);
    pptr++;
  };

  pptr = list;

  start = (char *) *pptr;
  
  l = sprintf (line, "%p  ", pptr);
    
  while(*pptr != NULL) {
    l += sprintf (line+l, "%d ", *pptr - s); 
    pptr++;
  };

  l += sprintf (line+l, "%ld ", (long)*pptr);

  pptr--;

  len = strlen(*pptr);
  end = ((char *) *pptr) +len  ;

  while (start <= end) {
    l += sprintf (line+l, "%hhx ", *start);
    start++;
  };
  DEBUG4(line);
  
  memprint((char *) list, (long)end - (long)list+1);
  DEBUG4("PRINTLIST : END");
};
#else 
#define printlist(x)
#define memprint(x)
#endif

/* this  is a function  that create a  list of strings, but  the char*
  array and all the strings  are placed in the same malloc()ed buffer,
  so that the whole list can be free()ed with a single free(). Much in
  the same manner  as the environ ** all processes  are passed.  We do
  however  sacrifice insert  speed for  this convinience,  this should
  only be used  for fairly short lists. The math  here is pretty hairy
  since  we  do   math  with  char**,  char*  and   int  in  the  same
  expresions. Terje */

char ** _charlistadd(char ** list, char * addon, int flags)
{
  int n, oldsize, newsize, len, entries;
  int i, offset;
  char * dst, * src;
  char ** newlist;
  if (addon == NULL) return list;

  /* if the first element we create a buffer that hold a char * [2]
     (The last pointer in the array must alway be NULL), and the string
     itself */
  if (list == NULL) {
    newsize = strlen(addon) + sizeof(char *) * 2 + 1;
    DEBUG4("1: mallocing list  %d bytes long, strlen(add) = %d", 
	    newsize, strlen(addon));
    list = malloc(newsize);
    
    DEBUG4("1: list at %p", list);
    list[0] = ((char * ) list) + sizeof(char *) * 2;
    DEBUG4("1: list[0] at %p  offset %d\n", list[0], (int)list[0] - (int)list);
    list[1] = NULL;
    
    strcpy(list[0], addon);
    DEBUG4("1: copied in %s to %s", addon, list[0]);
    printlist(list);
    return list;
  };
  
  // find the size of the char * [n] at the beginning. 
  for (n = 0; list[n] != NULL; n++) {

    /* if unieq is req, check to see if it already exists */
    if (flags & UNIQUE) {
      if (strcmp(list[n], addon) == 0) {
	DEBUG4("Uniqueness req, and new string exists");
	return list;
      };
    };
  };
  
  /* now the length of the current list (actually malloc()ed buffer)
     is the address of the last string, minus the addess of the list
     itself plus the length of the last string plus the \0 terminating
     the last string */
  entries = n;
  n--;
  oldsize = (int) list[n] + strlen (list[n]) + 1 - (int) list;
  DEBUG4("list has  %d entries and is %d long", entries, oldsize);

  /* the new size if the old size plus a new entry in the char * [n+1]
     array at the beginning, plus the length of the new string with
     terminator */
  newsize = oldsize + sizeof(char *) + strlen(addon)+1;
  DEBUG4("new size is %d", newsize);

  // Since we've got a different case if realloc return a different
  // address we got this compiletime test.
  
#ifndef TEST_NEW_ADDR
  newlist = malloc(newsize);
  memset(newlist, '+', newsize);
  memmove ((char*) newlist, (char*) list, oldsize);
#else
  newlist = realloc(list, newsize);
#endif

  // needed below 
  src = list[0];

  /* we now recalc pointers */
  DEBUG4("   %p => %p entries %d", list, newlist, entries);
  for (i = 0; i < entries; i++) {
    
    offset = ((long)list[i]) - ((long) list);
    //must add the offset in the memmoce below 
    newlist [i] = ((char *) newlist) + offset + sizeof(char *) ; 
    
    DEBUG4("   %d  %p => %p offset %d new(%ld) old(%ld) ", i, list[i], newlist[i], offset, 
	   (long)list[i] - (long)list, (long)newlist[i] - (long)newlist);
  };
  list = newlist;
  
  memprint((char*) newlist, newsize);

  DEBUG4("list (%p)  has new length %d bytes %d) added", list, newsize, newsize - oldsize);

  /* now the char * array at the beginning must be extended with one
     entry, thus we must move the rest back. */ 

  /* NOTE: we can make an optimization here by extending the char *
     array with more than one. THe lest of teh array is given my the
     address of list[0] - the address of list / sizeof(char *). */
  
  dst = list[0];
  len = oldsize - (sizeof(char*)*(entries+1));
  
  DEBUG4("list at %p: memmove(%p(%d), %p(%d), %d)", 
	   list, dst, (int)dst - (int) list, src, (int)src - (int) list, len);
  memmove(dst, src, len);
  
  memprint((char*) list, newsize);

  /* set to point if list[n] to the point we place the new string, and
     copy in the new string */
  list[entries] = dst + len;
  DEBUG4("list[n(%d)] at %p", n, list[n]);

  strcpy(list[entries], addon);
  DEBUG4("copied in %s to %s", addon, list[n]);

  /* NULL terminate the array */
  list[entries+1] = NULL;
  DEBUG4("list[%d] = %p", entries+1, NULL);

#ifdef DEBUGGING
#endif
  printlist(list);

  return list; 
};

