#include <SRBprotocol.h>

extern char * _mw_srbmessagebuffer;

int _mw_srbsendready(int fd, char * domain);
int _mw_srbsendinitreply(int fd, SRBmessage * srbmsg, int rcode, char * field);
int _mw_srbsendcallreply(int fd, SRBmessage * srbmsg, char * data, int len, 
			 int apprcode, int rcode, int flags);

int srbDoMessage(int fd, char * message);
