/*
  MidWay
  Copyright (C) 2001 Terje Eggestad

  MidWay is free software; you can redistribute it and/or
  modify it under the terms of the GNU  General Public License as
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

/*
 * $Id$
 * $Name$
 * 
 * $Log$
 * Revision 1.3  2003/04/25 13:03:11  eggestad
 * - fix for new task API
 * - new shutdown procedure, now using a task
 *
 * Revision 1.2  2002/07/07 22:45:48  eggestad
 * *** empty log message ***
 *
 * Revision 1.1  2002/02/17 13:40:26  eggestad
 * The server manager
 *
 *
 */


/* the name in Group may be NULL, we've this macro for the places a
   string may not be NULL */
#define NONULL(x) x?x:""

struct Server;

struct ServerGroup {
  char * name; 
  char ** env;
  int autoboot;
  char * bindir;

  struct ServerGroup * next;
  struct Server * serverlist;
};

typedef struct ServerGroup ServerGroup;

enum { DOWN = 0, BOOTING, RUNNING, FAILED, SHUTWAIT };

struct ActiveServer {
  pid_t pid;
  int status;
  int kills;
};

typedef struct ActiveServer ActiveServer;

struct Server {
  char * name;

  struct ServerGroup * group;
  struct Server * next;

  /* config params */
  char ** env;
  char * exec;
  char * libexec;
  char * stdout;
  char * stderr;
  char ** args;
  int autoboot;
  int booted;
  int start_min;
  int start_max;

  /* runtime data */
  struct ActiveServer * activelist;
  int active;

  int started;
  
  time_t lastdeath;
  //  int lasterrno;
};

typedef struct Server Server;
  
int smgrCreateGroup(char * name);
int smgrSetGroupSetEnv(char * var, char * value);
int smgrSetGroupUnSetEnv(char * var);
int smgrSetGroupAutoBoot(int flag);

void smgrBeginGroup(char * name);
void smgrEndGroup(void);
int smgrBeginServer(char * name);
void smgrEndServer(void);

int smgrCreateServer(char * name, char * group);
int smgrSetServerSetEnv(char * var, char * value);
int smgrSetServerUnSetEnv(char * var);
int smgrSetServerAutoBoot(int flag);
int smgrSetServerExec(char * path);
int smgrSetServerArgs(char * args);
int smgrSetServerMinMax(int min, int max);

void smgrSetDefaultAutoBoot(int bool);
void smgrSetDefaultBinDir(char *path);

void smgrDumpTree(void);


int smgrDoWaitPid(void);
int smgrTask(PTask pt);

void smgrInit(void);

int smgrCall(mwsvcinfo * svcinfo);
void smgrAutoBoot(void);

