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
  License along with the MidWay distribution; see the file COPYING.  If not,
  write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA. 
*/

/*
 * $Id$
 * $Name$
 * 
 * $Log$
 * Revision 1.4  2000/09/21 18:40:56  eggestad
 * Minor changes due to diffrent deadline API
 *
 * Revision 1.3  2000/08/31 21:52:16  eggestad
 * Top level API moved to mwclientapi.c.
 *
 * Revision 1.2  2000/07/20 19:24:22  eggestad
 * CVS keywords were missing.
 *
 * Revision 1.1.1.1  2000/03/21 21:04:12  eggestad
 * Initial Release
 *
 * Revision 1.1.1.1  2000/01/16 23:20:12  terje
 * MidWay
 *
 */

static char * RCSId = "$Id$";
static char * RCSName = "$Name$"; /* CVS TAG */

#include <stdio.h>
#include <errno.h>

/*#include <sys/types.h>
#include <sys/ipc.h>
*/
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>

#include <math.h>

#include <MidWay.h>
#include <ipctables.h>
#include <ipcmessages.h>
#include <shmalloc.h>
#include <address.h>
/*
  Here we provide the "visible" library API, in the IPC only form.
  This is however limited to the client API, since the server API are 
  available in IPC only.

*/


int _mwattachipc(int type, char * name, int key)
{
  int rc;
  
  if (key <= 0) return -EINVAL;

  mwlog(MWLOG_DEBUG3, "_mwattachipc: Attaching to IPC with key %d", key);
  rc = _mw_attach_ipc(key, type);
  if (rc != 0) return rc;
  
  mwlog(MWLOG_DEBUG3, "_mwattachipc: Sending attach request to mwd type=%d name = %s", type, name);
  return _mw_ipcsend_attach(type, name, 0);
};

int _mwdetachipc(void)
{
  ipcmaininfo * ipcmain;
  int rc;

  ipcmain = _mw_ipcmaininfo(); 
  if (ipcmain == NULL) return 0;
  
  rc = _mwsystemstate();
  mwlog(MWLOG_DEBUG3, "_mwdetachipc: system is %s in shutdown", rc?"":"not");

  if (!rc) {
    errno = 0;
    rc = _mw_ipcsend_detach(0);
  } else {
    rc = _mw_ipcsend_detach(1);
  };
  _mw_detach_ipc();
  return 1;
};
/* This conclude the administrative calls.*/




/*****************************************************************
 * 
 * Here We have the API that makes a client able to talk to a server
 * Note that some of these are essetial for making a server talk to 
 * a calling client.
 * 
 * Thus a valid client can be cmplied without mwserver.c but you can't 
 * legally compile a server with out this module, even if it we 
 * attach as a server only.
 *
 *****************************************************************/



/* the now usuall fallback incase libmw.c are not before in the link path */
int _mwfetch_ipc(int handle, char ** data, int * len, int * appreturncode, int flags) 
{
  int rc;

  rc = _mwsystemstate();
  if (rc) return rc;

  return _mwfetchipc ( handle, data, len, appreturncode, flags);
}
/* the usuall IPC only API, except that mwcall() is implemeted
   entierly here. Reemeber to do the same in lwbmw.c and TCP imp.
*/
int _mwacall_ipc(char * svcname, char * data, int datalen, int flags) 
{
  int rc;
  
  rc = _mwsystemstate();
  if (rc) return rc;

  return _mwacallipc (svcname, data, datalen,  flags);
}
