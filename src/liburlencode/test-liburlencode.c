#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "urlencode.h"


/*****************************************************
 * testing functions
 *****************************************************/
void packnunpack(char * str, int slen)
{
  char * encode, * decode;
  int encodelen, decodelen;

  printf (" encode decode test string=\"%s\"(%d) \n",
	  str, slen);

  encodelen = urlnencodedup(&encode, str, slen);
  printf ("                is encoded \"%s\"(%d) \n", 
	  encode, encodelen);

  decodelen = urldecodedup(&decode, encode);
  printf ("               and decoded \"%s\"(%d)\n", 
	  decode, decodelen);
};

int testencode(int flag)
{
  char * X, *ptr;
  int len;

  if (!flag) return 0;

  printf("\nENCODE/DECODE ESCAPE STRINGS\n");

  printf("\n** Testing url escape of \"binary\" data\n");
  packnunpack("hei hei", 7);
  packnunpack("hei hei", 8);

  X = "hei + \\ / & % # hei";
  packnunpack(X, strlen(X));

  len = urlnencodedup(&X, X, strlen(X));
  packnunpack(X, strlen(X));

  printf("\n** Testing url escape of strings\n");
  printf("  urlencodedup(%s) = %s\n", X, urlencodedup(X));
  urldecodedup(&ptr, X);
  printf("  urldecodedup(%s) = %s\n", X, ptr);
  free (ptr);

  return 1;
};

int testfl(int flag)
{
  char * list, *l2, *X;
  int len;

  if (!flag) return 0;

  printf("\nFIELD LISTS\n");

  list = strdup("hei=b13&f2=541306&f3=666");
  len = strlen(list);

  printf ("\n** Testing urlflget() list is \"%s\" len=%d\n", list, len);
  printf(" urlflget(\"%s\", \"%s\") = \"%s\"\n", 
	 list, "hei", urlflget(list, "hei"));
  printf(" urlflget(\"%s\", \"%s\") = \"%s\"\n", 
	 list, "f2", urlflget(list, "f2"));
  printf(" urlflget(\"%s\", \"%s\") = \"%s\"\n", 
	 list, "f3", urlflget(list, "f3"));
  printf(" urlflget(\"%s\", \"%s\") = \"%s\"\n", 
	 list, "ff3", urlflget(list, "ff3"));

  list = NULL;
  printf ("\n** Testing urlflinit()\n");
  len = urlflinit(&list, 
		  "name", "Terje", 
		  "age", "31", 
		  "trusted", NULL,
		  "smart", "yes",
		  "sex", "male", 
		  NULL, NULL);
  printf (" list is \"%s\" len=%d\n", list, len);
  
  /* save for later use */
  l2 = strdup(list);
  /*
  if (l2 == NULL) printf("strdup failed with errno=%d \n", errno);
  else printf("list@%#x copy@%#X\n", (unsigned int)list, (unsigned int)l2);
  */
  printf ("\n** Testing urlfldel() list is \"%s\" len=%d\n", list, len);
  X = "trusted";
  len = urlfldel(&list, X);
  printf (" deleting \"%s\"\n  list is \"%s\"@%p len=%d\n", 
	  X, list, list, len);
  X = "name";
  len = urlfldel(&list, X);
  printf (" deleting \"%s\"\n  list is \"%s\"@%p len=%d\n", 
	  X, list, list, len);
  X = "sex";
  len = urlfldel(&list, X);
  printf (" deleting \"%s\"\n  list is \"%s\"@%p len=%d\n", 
	  X, list, list, len);
  X = "age";
  len = urlfldel(&list, X);
  printf (" deleting \"%s\"\n  list is \"%s\"@%p len=%d\n", 
	  X, list, list, len);

  /* rollback to saved copy */
  free (list);
  list = l2; 
  len = strlen(list);

  printf ("\n** Testing urlflset()\n  list is \"%s\" len=%d\n", list, len);
  len = urlflset(&list, "trusted", "yes");
  printf (" urlflset( , \"trusted\", \"yes\")\n  list is \"%s\" len=%d\n", list, len);
  len = urlflset(&list, "trusted", "no");
  printf (" urlflset( , \"trusted\", \"no\")\n  list is \"%s\" len=%d\n", list, len);
  len = urlflset(&list, "trusted", "");
  printf (" urlflset( , \"trusted\", \"\")\n  list is \"%s\" len=%d\n", list, len);
  return 1;
};


int testmap(int flag)
{
  char * list, *l2;
  urlmap * map;
  char buffer[1000];
  int rc;

  if (!flag) return 0;

  list = "name=Terje&age=31&trusted=&smart=yes&sex=male";

  printf("\n** TESTING MAP FUNCTIONS\n");

  map = urlmapdecode(list);
  if (map == NULL) printf(" urlmapdecode(%s) failed with errno=%d\n", list, errno);
  else {
    int i = 0;
    printf(" urlmapdecode(\"%s\") yieleded this map:\n", list);
    while (map[i].key != NULL) {
      printf("  %s=>%s(%d)\n", map[i].key, map[i].value, map[i].valuelen);
      i++;
      if (i > 50) break;
    };
  };

  if (map != NULL) {
    l2 = urlmapencode(map);
    printf(" urlmapencode gave this list: \"%s\"\n", l2);
    free(l2);
  };

  urlmapfree(map);

  map = malloc(sizeof(urlmap) * 10);

  map[0].key = "PAN";
  map[0].value = "5413061234567890";
  map[0].valuelen = 0;

  map[1].key = "STAN";
  map[1].value = "1 67\r\n";
  map[1].valuelen = 0;

  map[2].key = NULL;

  memset(buffer, '\0', 1000);
  rc = urlmapnencode(buffer, 30, map);
  printf(" urlmapnencode gave this list: \"%s\"(%d)\n", buffer, rc);
  rc = urlmapnencode(buffer, 1000, map);
  printf(" urlmapnencode gave this list: \"%s\"(%d)\n", buffer, rc);

  return 1;
}; 


int main(int argc, char ** argv)
{
  testencode(0);
  testfl(0);
  testmap(1);
  return 0;
};

