#include <SRBprotocol.h>
#include <connection.h>

extern char * _mw_srbmessagebuffer;

int _mw_srbsendready(Connection * conn, char * domain);
int _mw_srbsendinitreply(Connection * conn, SRBmessage * srbmsg, int rcode, char * field);
int _mw_srbsendcallreply(Connection * conn, SRBmessage * srbmsg, char * data, int len, 
			 int apprcode, int rcode, int flags);
int _mw_srbsendready(Connection * conn, char * domain);
int _mw_srbsendprovide(Connection * conn, char * service, int cost);
int _mw_srbsendunprovide(Connection * conn, char * service);
int _mw_srbsendgwinit(Connection * conn);

int srbDoMessage(Connection * conn, SRBmessage * srbmsg);
