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

/* for use with mwlog() */
#define URLLOG_FATAL     0
#define URLLOG_ERROR     1
#define URLLOG_WARNING   2
#define URLLOG_ALERT     3
#define URLLOG_INFO      4
#define URLLOG_DEBUG     5
#define URLLOG_DEBUG1    6
#define URLLOG_DEBUG2    7
#define URLLOG_DEBUG3    8
#define URLLOG_DEBUG4    9

#ifndef  NDEBUG
#define debug(m, ...)  mwlog(URLLOG_DEBUG, m, ## __VA_ARGS__)
#define debug1(m, ...) mwlog(URLLOG_DEBUG1, m, ## __VA_ARGS__)
#define debug2(m, ...) mwlog(URLLOG_DEBUG2, m, ## __VA_ARGS__)
#define debug3(m, ...) mwlog(URLLOG_DEBUG3, m, ## __VA_ARGS__)
#define debug4(m, ...) mwlog(URLLOG_DEBUG4, m, ## __VA_ARGS__)

#else 
#define debug(...)
#define debug1(...)
#define debug2(...)
#define debug3(...)
#define debug4(...)
#endif

#define info(m, ...)    mwlog(URLLOG_INFO, m, ## __VA_ARGS__)
#define warning(m, ...) mwlog(URLLOG_WARNING, m, ## __VA_ARGS__)
#define error(m, ...)   mwlog(URLLOG_ERROR, m, ## __VA_ARGS__)
#define fatal(m, ...)   mwlog(URLLOG_ERROR, m, ## __VA_ARGS__)

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

/* for weakmwlog.c, just to be able to use mwlog inside the url
   lib, which should be independent of libMidWay. */
int mwsetloglevel(int level);

#ifdef __GNUC__
/* gcc hack in order to get wrong arg type in mwlog() */
#define FORMAT_PRINTF __attribute__ ((format (printf, 2, 3)))
#else
#define FORMAT_PRINTF 
#endif
  void mwlog(int level, char * format, ...) FORMAT_PRINTF;



// timepegs which are defined in lib/utils.c

#ifdef TIMEPEGS

#define URLTIMEPEGNOTE(note) __timepeg(__FUNCTION__, __FILE__, __LINE__, note)
#define URLTIMEPEG() __timepeg(__FUNCTION__, __FILE__, __LINE__, NULL)
void  __timepeg(char * function, char * file, int line, char * note);
void timepeg_clear(void);
int timepeg_sprint(char * buffer, size_t size);
void timepeg_log(void);

void _perf_pause(void);
void _perf_resume(void);
#define timepeg_pause() _perf_pause()
#define timepeg_resume() _perf_resume()

#else

#define URLTIMEPEGNOTE(note)
#define URLTIMEPEG()

#define timepeg_pause()
#define timepeg_resume()
#define timepeg_clear()
#define timepeg_sprint(a,b)
#define timepeg_log()

#endif

#ifdef	__cplusplus
}
#endif

#endif /* _URLENCODE_H */
