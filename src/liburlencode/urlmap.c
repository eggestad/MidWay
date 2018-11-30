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

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "urlencode.h"

urlmap * urlmapdecode(const char * list)
{
  int i, idx, len, count = 1;
  char * next, * key_end, * value_start, * value_end;
  urlmap * map;

  URLTIMEPEGNOTE("mapdecode begin");
  debug3 ("list = %p", list);

  if (list == NULL) {
    errno = EINVAL; 
    return NULL;
  };

  /* count thru and find out how many key/value pairs there are. */
  len = strlen(list);
  debug3("strlen(%s) = %d", list, len);

  for (i = 0; i < len; i++) {
    if (list[i] == '&') count ++;
  };

  debug3("count=%d", count);

  map = malloc(sizeof(urlmap)*(count + 2));
  if (map == NULL) {
     fatal("Out of memory");
     abort();
  };

  idx = 0;
  next = (char *)list;
  while(next) {
    urldecodedup(&map[idx].key, next);

    /* find end of key, and start and end of value.*/
    key_end = strchr(next, '=');
    value_start = key_end + 1;
    value_end = strchr(next, '&');
    // if next = is past & then there are no value 

    debug3("key_end=%p(next+%ld) value_end=%p(next+%ld) next=%s", 
	   key_end, key_end-next, value_end, value_end - next, next);

    if ((value_end != NULL) && (key_end > value_end)) key_end = NULL;
    
    /* if there is a value decode it and set length */
    if (key_end != NULL) {
      map[idx].valuelen = urldecodedup(&map[idx].value, value_start);
      if (map[idx].valuelen == -1) {
	 warning ("in message decode we got a incorrect formated value %s",
		value_start);
	 goto errout;
      };
    } else {
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

  debug3("returns => %p", map);
  URLTIMEPEGNOTE("mapdecode end");
  return map;

 errout:
  urlmapfree(map);
  URLTIMEPEGNOTE("mapdecode end NULL");
  return NULL;
};

char * urlmapencode(urlmap * map)
{
  char * list = NULL;
  int listlen, maxlistlen;
  int idx, rc;
  int keylen; 
  int valuelen;

  URLTIMEPEGNOTE("mapencode begin");
  if (map == NULL) return NULL;

  maxlistlen = 64;
  list = (char *) malloc(maxlistlen+1);
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
  URLTIMEPEGNOTE("mapencode end");
  return list;

}
int urlmapnencode(char * list, int len, urlmap * map)
{
  int listlen;
  int idx, rc;
  int keylen; 
  int valuelen;

  URLTIMEPEGNOTE("mapnencode begin");
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
  URLTIMEPEGNOTE("mapencode end");
  return listlen;
};

urlmap * urlmapdup(urlmap * map)
{
  urlmap * newmap = NULL;
  int idx, n;
  int l;

  URLTIMEPEGNOTE("mapdup begin");
  if (map == NULL) return NULL;

  debug3("beginning copy of map at %p", map);

  /* find the number of pairs in the map. */
  for (n = 0; map[n].key != NULL;  n++) ;
  
  debug3("map to be copied has %d pairs", n);

  newmap = malloc(sizeof(urlmap) * (n+1));
  
  for (idx = 0; idx < n; idx++) {
    
     debug3("copying pair %d: key=%s :: value=%s(%d)",  idx,  
	    map[idx].key, 
	    map[idx].value!=NULL?map[idx].value:"(null)",
	    map[idx].valuelen);

    /* copy key */
    l = strlen(map[idx].key);
    newmap[idx].key = malloc(l+1);
    memcpy(newmap[idx].key, map[idx].key, l);
    newmap[idx].key[l] = '\0';
    
    /* copy value */
    if (map[idx].value == NULL) {
      newmap[idx].value = NULL;
      newmap[idx].valuelen = 0;
    } else {
      newmap[idx].valuelen = map[idx].valuelen;
      newmap[idx].value = malloc(newmap[idx].valuelen+1);
      memcpy(newmap[idx].value, map[idx].value, newmap[idx].valuelen);
      newmap[idx].value[newmap[idx].valuelen] = '\0'; // just to be on the safe side.
    };
  };
  newmap[idx].key = NULL;
  newmap[idx].value = NULL;
  newmap[idx].valuelen = 0;
  URLTIMEPEGNOTE("mapdup end");
  return newmap;
};

void urlmapfree(urlmap * map)
{
  int idx = 0;

  if (map == NULL) return ;
  
  debug3("urlmapfree(%p)", map);

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


int urlmapget(urlmap * map, const char * key)
{
  int idx = 0;

  URLTIMEPEGNOTE("begin");
  if ( (map == NULL) || (key == NULL) ) {
    errno = EINVAL;
    return -1;
  };

  while (map[idx].key != NULL) {
    if (strcasecmp(key, map[idx].key ) == 0) {
       URLTIMEPEGNOTE("end");
       return idx;
    };
    idx ++;
  };
  errno = ENOENT;
  URLTIMEPEGNOTE("end NOENT");
  return -1;
};

char * urlmapgetvalue(urlmap * map, const char * key)
{
  int n;
  n = urlmapget(map, key);
  if (n >= 0) return map[n].value;
  return NULL;
};


int urlmapnset(urlmap * map, const char * key, const void * value, int len)
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
	if (map[idx].value) free(map[idx].value);
	map[idx].value = NULL;
	map[idx].valuelen = 0;
      };
      return idx;
    };
    idx ++;
  };
  errno = ENOENT;
  return -1;
};

int urlmapset(urlmap * map, const char * key, const char * value)
{
  int len = 0;
  if (value) len = strlen(value);
  return urlmapnset(map, key, value, len);
};


int urlmapseti(urlmap * map, const char * key, int value)
{
  int len = 0;
  char szValue[32]; /* just to be more than 64 bit clean */

  len = sprintf(szValue, "%d", value);
  return urlmapnset(map, key, szValue, len);
};


int urlmapdel(urlmap * map, const char * key)
{
  int idx = 0, ridx = -1;

  if (map == NULL) return 0;
  if (key == NULL) return 0;

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
urlmap * urlmapnadd(urlmap * map, const char * key, const void * value, int len)
{
  int idx = 0;
  int l;
  urlmap * m;

  URLTIMEPEGNOTE("begin");
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

  debug3("realloced %p to %p", m, map);

  /* make a copy and insert key */
  l = strlen(key);
  map[idx].key = malloc(l+1);
  if (map[idx].key == NULL) return NULL;
  strcpy(map[idx].key, key);

  if (value == NULL) {
    map[idx].value = NULL;
    map[idx].valuelen = 0;
  } else {
  /* make a copy and insert value */
    map[idx].value = malloc(len+1);
    if (map[idx].value == NULL) return NULL;
    memcpy(map[idx].value, value, len);
    map[idx].valuelen = len;
  };
  /* NULL term the array */
  map[idx+1].key = NULL;
  map[idx+1].value = NULL;
  map[idx+1].valuelen = 0;

  URLTIMEPEGNOTE("end");
  return map;
};

urlmap * urlmapadd(urlmap * map, const char * key, const char * value)
{
  int len = 0;
  if (value) len = strlen(value);
  return urlmapnadd(map, key, value, len);
};

urlmap * urlmapaddi(urlmap * map, const char * key, int iValue)
{
  char szValue[32]; /* just to be more than 64 bit clean */
  int i;

  i = sprintf(szValue, "%d", iValue);
  return urlmapnadd(map, key, szValue, i);
};

