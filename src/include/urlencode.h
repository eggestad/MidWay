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


/******************************************************************
 * I M P O R T A N T  malloc/realloc
 *
 * This lib is really intended only for C not C++. It is actually
 * discuraged to use malloc and new in the same program, even if I'e
 * never experienced any problems doing so.
 *
 * We use realloc extensivly here in the urlfl APIand therefore we
 * often malloc() buffer bigger than needed, since a realloc is cheap
 * if the cuurent chunk is large enough.  GNU Lib C allocate chunks in
 * 8, 64, 512, etc sizes.
 *
 * C++'s new and delete don't have anything resembeling realloc, a
 * object oriented lib for C++ is provided.  Since this lib
 * inescapedly work with strings changing size a lot, I question the
 * performance.  the C++ classes should give better perf on field list
 * if many operations are performed.
 *
 * To overcome what stated above I added the urlmap API.
 ******************************************************************/

#ifndef _URLENCODE_H
#define _URLENCODE_H

/* gcc hack in order to avoid unused warnings (-Wunused) on cvstags */
#ifdef __GNUC__
#define UNUSED __attribute__ ((unused))
#else
#define UNUSED 
#endif


#ifdef __cplusplus
extern "C" {
#endif

/*********************************************************************
 * API for do string (field) en/decode. 
 * they (un)escapes chars illegal in a URL.
 *********************************************************************/
int urlnencodedup(char ** encoded, char * plain, int len);
int urlnencode(char * encoded, char * plain, int len);
char * urlencodedup(char * plain);
int urlencode(char * encoded, char * plain);

int urldecodedup(char ** plain, char * encoded);
int urldecode(char * plain, char * encoded);

int urllegal(char * string);


/*********************************************************************
 * API for handling field operations.  fields are
 * name=value&name=value all name and value must be previously ncoded
 * with url(n)encode(), and names and values must be decoded.
 *********************************************************************/

char * urlflget(char * list, char * name);
int urlflset(char ** list, char * name, char * value);
int urlflcheck(char * list, char * name);

int urlfladd(char ** list, char * name, char * value);
int urlfldel(char ** list, char * name);
int urlflinit(char ** list, ...);


/*********************************************************************
 * API for maps. Map kays and values are uncoded, and values may
 * contain NULL
 *********************************************************************/

typedef struct {
  char * key;
  char * value;
  int valuelen;
} urlmap;

urlmap * urlmapdecode(char * list);
char * urlmapencode(urlmap * map);
int urlmapnencode(char * list, int len, urlmap * map);

urlmap * urlmapdup(urlmap * map);
void urlmapfree(urlmap * map);

int urlmapnset(urlmap * map, char * key, void * value, int len);
int urlmapset(urlmap * map, char * key, char * value);
int urlmapseti(urlmap * map, char * key, int value);

int urlmapget(urlmap * map, char * key);
char * urlmapgetvalue(urlmap * map, char * key);
int urlmapdel(urlmap * map, char * key);
urlmap * urlmapnadd(urlmap * map, char * key, void * value, int len);
urlmap * urlmapadd(urlmap * map, char * key, char * value);
urlmap * urlmapaddi(urlmap * map, char * key, int value);

#ifdef	__cplusplus
}
#endif

#endif /* _URLENCODE_H */
