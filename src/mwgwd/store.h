/*
  MidWay
  Copyright (C) 2000 Terje Eggestad

  MidWay is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License as
  published by the Free Software Foundation; either version 2 of the
  License, or (at your option) any later version.
  
  MidWay is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.
  
  You should have received a copy of the GNU General Public License 
  along with the MidWay distribution; see the file COPYING.  If not,
  write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA. 
*/

/* $Id$ */

/*
 * $Log$
 * Revision 1.2  2002/07/07 22:45:48  eggestad
 * *** empty log message ***
 *
 * Revision 1.1  2000/07/20 18:49:59  eggestad
 * The SRB daemon
 *
 */


void storePushCall(CLIENTID, int handle, int fd, urlmap *);
int  storePopCall(CLIENTID, int handle, int * fd,  urlmap **);
int  storeGetCall(CLIENTID cid, int ipchandle, int * fd,  urlmap **map);
int  storeSetIPCHandle(CLIENTID cid, int nethandle, int fd, int ipchandle);
void storeLockCall(void);
void storeUnLockCall(void);

void storePushDataBuffer(unsigned int nethandle, int fd, char * data, int datalen);
int  storePopDataBuffer(unsigned int nethandle, int fd, char ** data, int *datalen);
int  storeAddDataBufferChunk(unsigned int nethandle, int fd, char * data, int len);
int  storeGetDataBufferChunk(unsigned int nethandle, int fd);
int  storeSetDataBufferChunk(unsigned int nethandle, int fd, int chunk);

void storePushAttach(char * cltname, int connectionid, int fd, urlmap *);
int  storePopAttach(char * cltname, int * connectionid, int * fd, urlmap **);
