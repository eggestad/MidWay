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
  License along with the MidWay distribution; see the file COPYING. If not,
  write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA. 
*/

static char * RCSId = "$Id$";
static char * RCSName = "$Name$"; /* CVS TAG */

/* 
 * $Log$
 * Revision 1.4  2001/08/29 17:53:31  eggestad
 * - added missing licence header
 * - added  urlmapgetvalue()
 * - urlmapnset is now an implied urlmapadd() if no key exists
 * - various NULL pointer checks added.
 *
 */

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "urlencode.h"

urlmap * urlmapdecode(char * list)
{
  int i, idx, len, count = 1;
  char * next, * key_end, * value_start, * value_end;
  urlmap * map;

#ifdef DEBUG  
  fprintf (stderr, "urlmapdecode: list = %p\n", list);
  fflush(stderr);
#endif 

  if (list == NULL) {
    errno = EINVAL; 
    return NULL;
  };

  /* count thru and find out how many key/value pairs there are. */
  len = strlen(list);
#ifdef DEBUG  
  fprintf (stderr, "urlmapdecode: strlen(%s) = %d\n", list, len);
  fflush(stderr);
#endif 

  for (i = 0; i < len; i++) {
    if (list[i] == '&') count ++;
  };

#ifdef DEBUG
  fprintf (stderr, "urlmapdecode: count=%d\n", count);
  fflush(stderr);
#endif 

  map = malloc(sizeof(urlmap)*(count + 2));
  idx = 0;
  next = list;
  while(next) {
    urldecodedup(&map[idx].key, next);

    /* find end of key, and start and end of value.*/
    key_end = strchr(next, '=');
    value_start = key_end + 1;
    value_end = strchr(next, '&');

    /* if there is a value decode it and set length */
    if (key_end != NULL)
      map[idx].valuelen = urldecodedup(&map[idx].value, value_start);
    else {
      map[idx].value = NULL;
      map[idx].valuelen = 0;
    }
    idx++;

    /* skip excess & */
    if (value_end != NULL) {
      value_end++;
      while(value_end[1] == '&') value_end++;
      next = value_end;
    } else {
      next = NULL;
    };
  };
  /*  idx++;*/
  map[idx].key = NULL;
  map[idx].value = NULL;
  map[idx].valuelen = 0;

#ifdef DEBUG
  fprintf(stderr, "urlmapdecode returns => %p\n", map);
  fflush(stderr);
#endif

  return map;
};

char * urlmapencode(urlmap * map)
{
  char * list = NULL;
  int listlen, maxlistlen;
  int idx, rc;
  int keylen; 
  int valuelen;

  if (map == NULL) return NULL;

  maxlistlen = 64;
  list = (char *) malloc(maxlistlen);
  listlen = 0;
  idx = 0;

  while(map[idx].key != NULL) {
    /* if not the beginning insert the pair separator. */
    if (listlen != 0)
      list[listlen++] = '&';

    keylen = 3 * strlen(map[idx].key);
    /* should we really allow 0 length keys?? */
    if (keylen + listlen + 1 > maxlistlen) {
      maxlistlen = keylen + listlen + 65;
      list = realloc (list, maxlistlen);
    };
    rc = urlencode(list + listlen, map[idx].key);
    if (rc == -1) {
      free (list);
      return NULL;
    };
    listlen += rc;

    if (map[idx].value != NULL) {
      list[listlen++] = '=';
      if (map[idx].valuelen == 0) valuelen = strlen(map[idx].value);
      else valuelen = map[idx].valuelen;
      if (valuelen + listlen + 1 > maxlistlen) {
	maxlistlen = valuelen + listlen + 65;
	list = realloc (list, maxlistlen);
      };
      rc = urlnencode(list + listlen, map[idx].value, valuelen);
      if (rc == -1) {
	free (list);
	return NULL;
      };
      listlen += rc;
    };
    idx++;
  };
  list[listlen] = '\0';
  return list;

}
int urlmapnencode(char * list, int len, urlmap * map)
{
  int listlen;
  int idx, rc;
  int keylen; 
  int valuelen;

  listlen = 0;
  idx = 0;

  /* input sanity checking */  
  if (list == NULL) {
    errno = EINVAL;
    return -1;
  };
  if (len < 1) {
    errno = EINVAL;
    return -1;
  };

  /* map == NULL is OK */
  if (map == NULL) {
    list[0] = '\0';
    return 0;
  };

  while(map[idx].key != NULL) {
    /* if not the beginning insert the pair separator. */
    if (listlen != 0)
      list[listlen++] = '&';

    keylen = 3 * strlen(map[idx].key);
    /* should we really allow 0 length keys?? */
    if (keylen + listlen + 2 > len) {
      errno = EOVERFLOW;
      return -1;
    };
    rc = urlencode(list + listlen, map[idx].key);
    listlen += rc;

    if (map[idx].value != NULL) {
      list[listlen++] = '=';
      if (map[idx].valuelen == 0) valuelen = strlen(map[idx].value);
      else valuelen = map[idx].valuelen;
      if (valuelen + listlen + 1 > len) {
	errno = EOVERFLOW;
	return -1;
      };
      rc = urlnencode(list + listlen, map[idx].value, valuelen);
      listlen += rc;
    };
    idx++;
  };
  list[listlen] = '\0';
  return listlen;
};


void urlmapfree(urlmap * map)
{
  int idx = 0;

  if (map == NULL) return ;
#ifdef DEBUG  
  fprintf(stderr, "urlmapfree(%p)\n", map);
  fflush(stderr);
#endif

  while(map[idx].key != NULL) {
    free(map[idx].key);
    map[idx].key = NULL;
    if (map[idx].value != NULL) {
      free (map[idx].value);
      map[idx].value = NULL;
    };
    idx ++;
  };

  free(map);
};


int urlmapget(urlmap * map, char * key)
{
  int idx = 0;

  if ( (map == NULL) || (key == NULL) ) {
    errno = EINVAL;
    return -1;
  };

  while (map[idx].key != NULL) {
    if (strcasecmp(key, map[idx].key ) == 0) return idx;
    idx ++;
  };
  errno = ENOENT;
  return -1;
};

char * urlmapgetvalue(urlmap * map, char * key)
{
  int n;
  n = urlmapget(map, key);
  if (n >= 0) return map[n].value;
  return NULL;
};


int urlmapnset(urlmap * map, char * key, void * value, int len)
{
  int idx = 0;

  if ( (map == NULL) || (key == NULL) ) {
    errno = EINVAL;
    return -1;
  };

  while (map[idx].key != NULL) {
    if (strcasecmp(key, map[idx].key ) == 0) {
      if (value != NULL) {
	map[idx].value = realloc(map[idx].value, len+1);
	memcpy(map[idx].value, value, len);
	map[idx].valuelen = len;
      } else {
	free(map[idx].value);
	map[idx].value = NULL;
	map[idx].valuelen = 0;
      };
      return idx;
    };
    idx ++;
  };
  
  return urlmapnadd(map, key, value, len);
};

int urlmapset(urlmap * map, char * key, char * value)
{
  int len = 0;
  if (value) len = strlen(value);
  return urlmapnset(map, key, value, len);
};


int urlmapdel(urlmap * map, char * key)
{
  int idx = 0, ridx = -1;

  while (map[idx].key != NULL) {
    if (strcasecmp(key, map[idx].key ) == 0) ridx = idx;
    idx ++;
  };

  if (ridx != -1) {
    idx--;

    free(map[ridx].key);
    free(map[ridx].value);

    map[ridx].key      = map[idx].key;
    map[ridx].value    = map[idx].value;
    map[ridx].valuelen = map[idx].valuelen;
    map[ridx].key      = map[idx].key;
    map[ridx].value    = map[idx].value;
    map[ridx].valuelen = map[idx].valuelen;

    map[idx].key      = NULL;
    map[idx].value    = NULL;
    map[idx].valuelen = -1;
    return 1;
  };

  return 0;
};

/* note that urlmapfree() requires that all strings and the map array
   itself are malloced.*/
urlmap * urlmapnadd(urlmap * map, char * key, void * value, int len)
{
  int idx = 0;
  int l;
  urlmap * m;

  if (key == NULL) return map;

  if (len < 0) {
    errno = EINVAL;
    return NULL;
  };

  m = map;
  /* extend the array */
  if (map != NULL)
    while (map[idx].key != NULL) {
      if (strcmp(key,map[idx].key) == 0) {
	errno = EEXIST;
	return NULL;
      };
      idx++;
    }
  map = realloc (map, sizeof(urlmap) * (idx + 2));
  if (map == NULL) {
    return NULL;
  };

#ifdef DEBUG
  fprintf(stderr, "urlmapnadd realloced %p to %p\n", m, map);
  fflush(stderr);
#endif

  /* make a copy and insert key */
  l = strlen(key);
  map[idx].key = malloc(l+1);
  if (map[idx].key == NULL) return NULL;
  strcpy(map[idx].key, key);

  /* make a copy and insert value */
  map[idx].value = malloc(len+1);;
  if (map[idx].value == NULL) return NULL;
  memcpy(map[idx].value,value, len);
  map[idx].valuelen = len;

  /* NULL term the array */
  map[idx+1].key = NULL;
  map[idx+1].value = NULL;
  map[idx+1].valuelen = 0;

  return map;
};

urlmap * urlmapadd(urlmap * map, char * key, char * value)
{
  int len = 0;
  if (value) len = strlen(value);
  return urlmapnadd(map, key, value, len);
};

urlmap * urlmapaddi(urlmap * map, char * key, int iValue)
{
  char szValue[32]; /* just to be more than 64 bit clean */
  int i;

  i = sprintf(szValue, "%d", iValue);
  return urlmapnadd(map, key, szValue, i);
};

