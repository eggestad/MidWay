
class UrlListElement friend UrlList
{
  char * name;
  int namelen;
  char * value;
  int valuelen;
  UrlListElement * next;
};


class UrlList
{
  int entries;
  char ** names;
  char ** values;

 public:
  
  char * encode();
  int decode(char *);
  int init(char *, ...);

  int add(char*, char*);
  int del(char*);
  char * get(char*);
  int set(char*,char*);
  char ** names();
  char ** values();

  int merge(UrlList *);
  UrlList * subset(char *, ...);

  int count();

  UrlList() {
    entries = 0;
    names = NULL;
    values = NULL;
  };

  UrlList(char * enc) {
    decode(enc);
  };

  
