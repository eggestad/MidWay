
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

void urlmapfree(urlmap * map);

int urlmapnset(urlmap * map, char * key, void * value, int len);
int urlmapset(urlmap * map, char * key, char * value);

int urlmapget(urlmap * map, char * key);
int urlmapdel(urlmap * map, char * key);
urlmap * urlmapnadd(urlmap * map, char * key, void * value, int len);
urlmap * urlmapadd(urlmap * map, char * key, char * value);
urlmap * urlmapaddi(urlmap * map, char * key, int value);

#ifdef	__cplusplus
}
#endif

#endif /* _URLENCODE_H */
