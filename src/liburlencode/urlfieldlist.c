
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
    
#include "urlencode.h"

/*********************************************************
 * API for handling field operations.  fields are
 * name=value&name=value all name and value must be encoded with
 * url(n)encode(), but there are now check for that here.
 *********************************************************/

char * urlflget(char * list, char * name)
{
  char *  value_start;
  int len;

  errno = 0;
  len = strlen(name);
  /* test if first in list */
  if ( (strncmp(name, list, len) == 0) && (list[len] == '=') ) {
    value_start = &list[len+1];
  } else {
    char search [len+2];
    sprintf(search, "&%s=", name);
    value_start = strstr(list, search);
    if (value_start == NULL) {
      errno = ENOENT;
      return NULL;
    };
    value_start = value_start + len + 2;
  };
  /* name exists, and value_start now points to beginning of value we
     return a pointer to the value, = & or NULL terminated. we do not
     return a copy here.*/
  return value_start; 

};

/* same as urlflget() but we return a null terminated copy */
char * urlflgetcpy (char * list, char * name)
{
  char *  ret, * value_start, * value_end;
  int vlen;

  value_start = urlflget(list, name);

  value_end = strchr(value_start, '&');
  if (value_end == NULL) vlen = strlen(value_start);
  else vlen = value_end - value_start;
  
  ret = (char *) malloc(vlen+1);
  strncpy(ret, value_start, vlen);
  ret[vlen] = '\0';
  
  return ret;
};
  

int urlflset (char ** l, char * name, char * value)
{
  char * list;
  int nvallen, ovallen;
  int nlen, llen, diff;
  char * value_start, * value_end;
  char * tmp;

  /* input checking */
  if ( ! (l && name && value) || (l && !*l)) {
    errno = EINVAL;
    return -1;
  };
  list = *l;
    
  /* input now OK, find start and end of value. */
  nvallen = strlen(value);
  nlen = strlen(name);
  llen = strlen(list);
  if ( (strncmp(list, name, nlen) == 0) && list[nlen] == '=') {
    value_start = list + nlen + 1;
  } else {
    char search[nlen+3];

    sprintf(search, "&%s=", name);
    value_start = strstr(list, name);
    if (value_start == NULL) {
      errno = ENOENT;
      return -1;
    }
    value_start +=nlen + 1;
  };
  value_end = strchr(value_start,'&');
  if (value_end == NULL) value_end = list + llen;
  ovallen = value_end - value_start;

  /* calc diff in list length and realloc if more is needed. */
  diff = nvallen - ovallen;
  if (diff > 0) {
    list = realloc(list, llen + diff+1);
  } 

  /* if there is a diff, move the end of list to leave 
     the right space for the new value. */
  if ( diff != 0) {
    char * rest_old, * rest_new;
    int rlen;
    rest_old = value_end;
    rest_new = value_end + diff;
    rlen =  list + llen - value_end;
    tmp = memmove(rest_new, rest_old, rlen);
  };
  strncpy(value_start, value, nvallen);
  llen += diff;    
  list[llen] = '\0';

  *l = list;
  return llen;
};


/* test to see if name exists in list */
int urlflcheck(char * list, char * name)
{
  int len;
  char * value_start;
 
  /* name must be url encoded. */
  if ( (! urllegal(name)) ||  (list == NULL) ) {
    errno = EINVAL;
    return -1;
  };

  len = strlen(name);
  /* test if first in list */
  if ( (strncmp(name, list, len) == 0) && (list[len] == '=') ) {
    return 1;
  } else {
    char search [len+2];
    sprintf(search, "&%s=", name);
    value_start = strstr(list, search);
    if (value_start == NULL) {
      errno = ENOENT;
      return 0;
    };
    return 1;
  };
};

/* add a name/value pair to the list
   fails if name already exists. 
*/
int urlfladd(char ** l, char * name, char * value)
{
  char * list;
  int len, newlen, nlen, vlen;
  char * separator;

  if ( (name == NULL) || (l == NULL) ) {
    errno = EINVAL;
    return -1;
  };
  list = *l;
  if (list == NULL) 
    len = 0;
  else 
    len = strlen(list);
  
  /* We do not allow duplicate parameter names. */
  if (urlflcheck(list, name)) {
    errno = EEXIST;
    return -1;
  };
  /* value must be url encoded if not NULL. */
  if ( (value != NULL) && (! urllegal(value))) {
    errno = EINVAL;
    return -1;
  };

  /* calulate the length of the new list and realloc. */
  nlen = strlen(name);
  if (value == NULL) {
    newlen = len + nlen + 3;
    value="";
  } else {
    vlen = strlen(value);
    newlen = len + nlen + vlen + 3;
  };
  list = (char *) realloc(list, newlen);

  /* if the list is not empty we add the "&" to teh beginning of the
     new pair. Techincally a & at the beginning of the list is legal,
     just like consecutive &&, but it looks ugly. */
  if (len == 0) separator="";
  else separator="&";

  /* now add the new pair at the end. */
  sprintf(list+len, "%s%s=%s", separator, name, value);

  *l = list;
  return newlen;
};

int urlfldel(char ** l, char * name)
{
  char * start;
  char * end;
  char * list;
  int nlen;
  int llen;
  int dlen;

  list = *l;

  /* input check */
  if ( (name == NULL) || (list == NULL) ) {
    errno = EINVAL;
    return -1;
  };

  errno = 0;
  nlen = strlen(name);
  llen = strlen(list);
  /* test if first in list */
  if ( (strncmp(name, list, nlen) == 0) && (list[nlen] == '=') ) {
    start = list;
    end = strchr(start, '&');
    /* the last pair in the list */
    if (end == NULL) {
      list[0] = '\0';
      llen = 0;
    }
    else {
      end++;
      dlen = end - start;
      memmove (start, end, list - end + llen );
      llen -= dlen;
      list[llen] = '\0';
    }
  } else {
    char search [nlen+2];
    sprintf(search, "&%s=", name);
    start = strstr(list, search);
    if (start == NULL) {
      errno = ENOENT;
      return -1;
    };
    end = strchr(start+1, '&');
    if (end == NULL) end = list + llen;
    dlen = end - start;
    memmove (start, end, list + llen - end);
    llen -= dlen;
    list[llen] = '\0';
  };
  return llen;
};
/* int urlinitfl(char * list, ...), the two last args must be NULL */

int urlflinit(char ** list, ...)
{
  va_list ap;
  char * name, * value;
  int len;

  errno = 0;
  *list = NULL;

  /* The usual input validation. */
  if (list == NULL) {
    errno = EINVAL;
    return -1;
  };
  
  *list = malloc(100);
  *list[0] = '\0';
  len = 0;
  va_start(ap, list);

  name = va_arg(ap, char *);
  value = va_arg(ap, char *);

  while(name != NULL) {
    printf("  Adding %s=%s to %s (len=%d)... ", name, value, *list, len);
    len = urlfladd(list, name, value);
    if (len == -1) {
      free (*list);
      * list = NULL;
      return -1;
    };
    printf("\n  list is now %s (len=%d)\n", *list, len);
    name = va_arg(ap, char *);
    value = va_arg(ap, char *);
  } 

  return len;
};
