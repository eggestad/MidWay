
#include <stdio.h>
#include <string.h>

 

static char * sourceline = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}";
 
static char line[132] = {0};

static int currentline = 0;

static int line_len = 74;

void reset() {
   currentline = 0;
}

char * getnextline() {
   int srclen = strlen(sourceline);

   int offset = 1 + ( currentline );

   offset = offset % srclen;
   
   int end = offset + line_len;
   //printf ("offset = %2d end =%3d", offset, end);
   //printf ("srclen=%d =%3d",srclen,end / srclen);

   if (end > srclen) {
      //printf("splitt");
      int lastlen = end % srclen;
      int startlen  = line_len - lastlen;

      strncpy(line, &sourceline[offset], startlen);
      strncpy(line+startlen, sourceline , line_len-startlen);

   } else {
      //printf("direct");
      strncpy(line, sourceline + offset, line_len);
   }
   currentline ++;
   
   return line;
}

int  chargen (char * buffer, size_t buflen) {
   int lines = buflen / (line_len+1);
   reset();
   
   size_t offset = 0;
   printf("lines = %d\n", lines);
   int i;
   for (i = 0; i < lines; i++) {
      char * line = getnextline();
      offset += sprintf(buffer+offset, "%s\n", line);
   }
   printf("lines = %d len = %zu\n", i, offset); 
   return offset;
}



#ifdef TEST
int main() {
   
   for (int i = 0; i < 10000; i++) {
      char * line = getnextline();
      printf(" %s\n", line);
   }
}
#endif
   

   
   
